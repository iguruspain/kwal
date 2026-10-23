# Plan de implementación: transiciones, caché de medios y cambio de fondo para una app de wallpapers de Plasma (Qt6 / Kirigami / C++)

*Documento para el agente implementador. Oráculo de comportamiento: la implementación en Rust `kdewall-plugin-switcher` (repo adjunto: `src/main.rs`, `transitions.rs`, `effects.rs`, `monitors.rs`, `cache.rs`, `lib.rs`). No se modifica; se usa para comparar.*

## 1. Objetivo y alcance

Portar a C++ las capacidades del programa Rust, **no como CLI sino como componentes reutilizables** (biblioteca + tipos expuestos a QML) integrados en la app Kirigami de wallpapers que ya existe. Capacidades:

1. Cambiar el plugin de fondo de Plasma en vivo (`org.kde.image` y `luisbocanegra.smart.video.wallpaper.reborn`) por D-Bus, con respaldo escribiendo el fichero de configuración.
2. Transiciones estilo awww sobre un overlay (grow, outer, wipe, wave, fade, simple, left/right/top/bottom, center, any, random, none).
3. Cambio opcional del fondo de la pantalla de bloqueo (`kscreenlockerrc`).
4. Caché de vídeos redimensionados y de primeros fotogramas con `ffmpeg`.
5. Numeración correcta de pantallas de Plasma frente a las salidas del compositor.

Fuera de alcance: CLI, sesiones X11 (layer-shell es solo Wayland: sin Wayland o sin layer-shell, el cambio se hace **sin transición**, nunca con error), otros escritorios.

**Primera tarea:** leer el código de la app anfitriona y adaptar nombres, módulos y estilo. Este plan fija comportamiento y contratos, no la estructura interna de la app (que no conozco).

## 2. Decisión: LayerShellQt frente a libwayland para el overlay

**Recomendación: LayerShellQt**, porque la app ya es una aplicación Qt (ya paga el coste de `QGuiApplication`) y Qt resuelve justo la familia de errores que costó corregir en la versión Rust:

| Aspecto | LayerShellQt (Qt) | libwayland directo |
|---|---|---|
| Escala fraccional / viewporter | Lo gestiona QtWayland | Hay que implementar `wp_viewporter` y calcular tamaño lógico/físico a mano (fuente del error de zoom) |
| Selección de monitor | `QScreen` (nombre = conector) | Enlazar `wl_output` y casar nombres a mano; `wl_output.geometry` no es fiable (wlroots da 0,0) y exige `xdg-output` |
| Buffers, doble buffer, release | Qt | `mmap`, memfd y seguimiento de `wl_buffer.release` a mano |
| Sincronía con vsync | `QWindow::requestUpdate()` | `wl_surface.frame` a mano |
| Integración con el bucle de eventos | Nativa | `QSocketNotifier` sobre un segundo fd de Wayland |
| Código a mantener | Del orden de 250 líneas | Del orden de 900 (la versión Rust) |
| Idiomático / revisable en KDE | Sí | No (segunda conexión Wayland dentro de una app Qt) |
| Dependencias | LayerShellQt6 (viene con Plasma 6) | Solo libwayland |

**Riesgo que decide el diseño (verificar primero, Spike S1):** `LayerShellQt::Shell::useLayerShell()` se llama antes de crear `QGuiApplication` y cambia la integración de shell de **toda** la app. Hay que comprobar con la versión de LayerShellQt instalada si las ventanas normales (la ventana principal Kirigami) siguen funcionando como `xdg_toplevel`. No lo doy por supuesto.

- **Plan A (si S1 pasa):** overlay en proceso, clase `LayerShellOverlay`.
- **Plan B (si S1 falla o es frágil):** overlay en un **proceso auxiliar** pequeño (`kdewall-overlay`, un `QGuiApplication` propio con `useLayerShell()`), que la app lanza con `QProcess`. Ventaja añadida: si el auxiliar muere, el compositor destruye sus superficies y no queda ningún overlay huérfano. Protocolo en el apéndice B.
- **Ambos planes** usan la misma interfaz `IOverlayBackend` (sección 5), de modo que la lógica no cambia.

libwayland solo compensaría si hubiera que usarlo sin Qt. Se descarta salvo que S1 y S2 fallen los dos.

**Render:** backend inicial por CPU (`QImage` en formato `Format_ARGB32_Premultiplied` + `QRasterWindow`), porque permite pruebas exactas contra la referencia. Un backend por GPU (QtQuick + `ShaderEffect`) queda como mejora opcional posterior; no será idéntico bit a bit, así que se validaría con tolerancia.

## 3. Arquitectura

Biblioteca `kdewallcore` (C++20, Qt6 Core/DBus/Gui; sin dependencia de KF6 en el núcleo) más una capa fina de integración QML.

| Módulo | Responsabilidad | Equivale en Rust |
|---|---|---|
| `Effects` | Matemática pura de píxeles (sin Qt Gui salvo tipos básicos): grow, outer, wipe, wave, fade, easing | `effects.rs` |
| `TransitionSpec` | Opciones awww como tipo valor (`Q_GADGET`, expuesto a QML) y su resolución | `TransitionArgs`, `Spec` en `transitions.rs` |
| `IOverlayBackend` + `LayerShellOverlay` | Ventanas layer-shell por pantalla, animación | parte Wayland de `transitions.rs` |
| `PlasmaClient` | D-Bus `org.kde.plasmashell`: `setWallpaper`, `evaluateScript` | funciones D-Bus de `main.rs` |
| `ScreenMap` | Número de pantalla de Plasma a `QScreen` | `monitors.rs` y `load_monitors` |
| `PlasmaConfig` | Lectura/escritura de `appletsrc` y `kscreenlockerrc` tras una interfaz `IConfigStore` | `Ini` en `lib.rs` |
| `WallpaperSwitcher` | Orquestación (dirección Cover/Reveal, secuencia, respaldo a fichero, lockscreen) | `switch()`, `plan_overlay()` en `main.rs` |
| `MediaCache` | `ffprobe`/`ffmpeg`, SHA-256 de contenido, resized y first-frame, list/clean | `cache.rs` |

`IConfigStore`: primera implementación = editor de INI **línea a línea** (porta `Ini`: conserva comentarios, orden y secciones que no toca). Una implementación con `KConfig` puede añadirse después; no escribirla sin poder compilar con KF6.

## 4. Contratos de comportamiento (fuente: la referencia Rust)

### 4.1 Opciones de transición (semántica awww)

- Tipos: `none simple fade left right top bottom wipe wave grow center any outer random`. Por defecto `grow`.
- `duration` en segundos, por defecto **0.9**. `fps`: tope opcional; por defecto el refresco de la pantalla (`simple`: 30). `step` (solo `simple`): por defecto **2**, 1..255.
- `angle` (wipe/wave), por defecto 45: **0 = derecha a izquierda, 90 = arriba a abajo, 270 = abajo a arriba**. Dirección de avance del frente en coordenadas de pantalla (y hacia abajo): `dir = (-cos a, sin a)`. `left`=180, `right`=0, `top`=90, `bottom`=270.
- `pos` (grow/outer), por defecto `center`: alias `center top left right bottom top-left top-right bottom-left bottom-right`, o `X,Y`: **flotante con punto = fracción, entero = píxeles lógicos, `N%` = fracción**. **Y se mide desde abajo** salvo `invertY`. `center` y `any` son `grow` (centro / punto aleatorio).
- `bezier` por defecto `.54,0,.34,.99`. Evaluación: resolver `x(s)=t` por bisección (32 iteraciones) y devolver `y(s)`.
- `wave` `(ancho, alto)` por defecto `20,20`: desplazamiento del borde `sin(perp/ancho)*alto`.
- `random`: elige entre `fade left right top bottom wipe wave grow outer any`, con ángulo aleatorio.
- Análisis de cadenas (`fromString`) opcional, para guardar ajustes del usuario con la misma gramática que awww.

### 4.2 Efectos de transición (especificación normativa)

Esta sección manda sobre cualquier otra. Cada efecto es una **función pura** que escribe en un buffer de píxeles, más un **conductor** que decide los parámetros de cada fotograma (4.2.8). El pseudocódigo es C++ equivalente, línea a línea, a `effects.rs`: el resultado debe coincidir **bit a bit** con los vectores del Apéndice C.

#### 4.2.1 Modelo

- El overlay es una imagen del tamaño **físico** de la pantalla. Cada efecto define, para cada píxel, una cobertura `cover` entre 0 y 1: la parte del píxel que cae dentro de la forma (círculo, semiplano…).
- **Reveal** (`orig` = fondo saliente, opaco): `píxel = orig * (1 - cover)`. La forma se vuelve transparente y deja ver el fondo nuevo que Plasma dibuja debajo. El buffer empieza igual a `orig`.
- **Cover** (`orig` = fondo entrante): `píxel = orig * cover`. La forma se pinta sobre un overlay transparente y tapa el fondo viejo. El buffer empieza a ceros.
- Las formas **solo crecen** con el tiempo (son monótonas). Por eso basta rellenar el interior y reescribir el borde móvil; nunca se repinta todo el buffer salvo en `fade`.
- Cover y Reveal son complementarios: con los mismos parámetros, la suma de sus alfas es 254 a 256 en cada píxel. Además `outer` en Reveal es **idéntico** a `grow` en Cover con el mismo radio (el Apéndice C lo muestra).

#### 4.2.2 Formato y primitivas (copiar tal cual)

Píxel `uint32_t` premultiplicado: `A<<24 | R<<16 | G<<8 | B`, el formato nativo de `QImage::Format_ARGB32_Premultiplied` en little endian. Puntero: `reinterpret_cast<uint32_t*>(image.bits())`, comprobando que `image.bytesPerLine() == width * 4`. Las coordenadas usan el **centro** del píxel `(x+0.5, y+0.5)`, con origen arriba a la izquierda y la y hacia abajo.

```cpp
using px = uint32_t;
enum class Paint { Reveal, Cover };

inline px scalePx(px p, uint32_t a) {            // a: 0..256 (256 = identidad, 0 = transparente)
    px rb = (((p & 0x00FF00FFu) * a) >> 8) & 0x00FF00FFu;
    px ag = (((p >> 8) & 0x00FF00FFu) * a) & 0xFF00FF00u;
    return rb | ag;
}
inline uint32_t keep256(double cover) {          // TRUNCAR tras sumar 0.5
    return static_cast<uint32_t>((1.0 - std::clamp(cover, 0.0, 1.0)) * 256.0 + 0.5);
}
inline void put(Paint m, px* buf, const px* orig, size_t i, double cover) {
    const uint32_t k = (m == Paint::Reveal) ? keep256(cover) : 256 - keep256(cover);
    buf[i] = scalePx(orig[i], k);
}
inline void fillFull(Paint m, px* buf, const px* orig, size_t a, size_t b) {   // píxeles [a, b)
    if (b <= a) return;
    if (m == Paint::Reveal) std::fill(buf + a, buf + b, 0u);
    else                    std::copy(orig + a, orig + b, buf + a);
}
```

No usar `QPainter`, `qAlpha` ni mezcla en coma flotante para los bordes: el resultado no sería exacto. `scalePx` **trunca**, no redondea: `scalePx(0xFFFFFFFF, 1) == 0` y `scalePx(0xFFFFFFFF, 255) == 0xFEFEFEFE`.

#### 4.2.3 Reglas comunes

1. **Precondición:** el buffer contiene el original (Reveal) o ceros (Cover), o un fotograma anterior del *mismo* efecto con parámetros menores o iguales. Con un único `QImage` que se va modificando se cumple siempre.
2. **Fotograma final:** cuando `done`, usar el valor extremo exacto (4.2.8) para que el resultado sea perfecto: Reveal = todo `0`; Cover = idéntico a `orig`.
3. **Sin asignaciones por fotograma** y sin recorrer el buffer entero salvo en `fade`. Presupuesto: peor fotograma ≤ 8 ms a 2560x1440 en una CPU de gama media (la referencia Rust, en perfil release, mide de 3 a 7 ms).
4. **Píxeles físicos:** todo se calcula en el tamaño del buffer físico. Solo la posición del centro (4.2.9) parte de coordenadas lógicas.

#### 4.2.4 grow (círculo que crece)

La forma es el disco de radio `r` centrado en `(cx, cy)`. Borde suavizado de 1 px: `cover = r + 0.5 - distancia(centro del píxel, (cx, cy))`, recortado a 0..1. Por fila se calcula el semiancho exterior `ho` (radio `r+0.5`) y el interior totalmente cubierto (radio `r-0.5`); el interior se rellena de golpe y solo los píxeles entre ambos se evalúan uno a uno.

```cpp
void drawGrow(Paint m, px* buf, const px* orig, int w, int h, double cx, double cy, double r) {
    if (r <= 0) return;
    const double rIn = r - 0.5, rOut = r + 0.5;
    const int y0 = (int)std::max(0.0, std::floor(cy - rOut));
    const int y1 = (int)std::clamp(std::ceil(cy + rOut), 0.0, (double)h);
    for (int y = y0; y < y1; ++y) {
        const double dy = y + 0.5 - cy;
        const double ho2 = rOut * rOut - dy * dy;
        if (ho2 <= 0) continue;                                   // el disco no toca esta fila
        const double ho = std::sqrt(ho2);
        const size_t row = (size_t)y * w;
        const int xo0 = (int)std::max(0.0, std::floor(cx - ho - 0.5));
        const int xo1 = (int)std::max(0.0, std::min(std::ceil(cx + ho - 0.5), w - 1.0));
        int xi0 = 0, xi1 = 0;                                     // tramo totalmente cubierto [xi0, xi1)
        const double ri2 = rIn * rIn - dy * dy;
        if (rIn > 0 && ri2 > 0) {
            const double hi = std::sqrt(ri2);
            const double a = std::max(0.0, std::ceil(cx - hi - 0.5));
            const double b = std::min(std::floor(cx + hi - 0.5), w - 1.0);
            if (a <= b) { xi0 = (int)a; xi1 = (int)b + 1; }
        }
        auto rim = [&](int x) {
            const double dx = x + 0.5 - cx;
            put(m, buf, orig, row + x, rOut - std::sqrt(dx * dx + dy * dy));
        };
        if (xi1 > xi0) {
            fillFull(m, buf, orig, row + xi0, row + xi1);
            for (int x = xo0; x < std::min(xi0, xo1 + 1); ++x) rim(x);
            for (int x = xi1; x <= xo1; ++x) rim(x);
        } else {
            for (int x = xo0; x <= xo1; ++x) rim(x);               // radio pequeño: solo borde
        }
    }
}
```

#### 4.2.5 outer (círculo que se cierra)

Lo contrario de `grow`: la forma es todo lo que queda **fuera** del círculo, y el radio **decrece** con el tiempo. En Reveal, lo de fuera se vuelve transparente y la imagen vieja se encoge hacia el punto; en Cover, la imagen nueva aparece desde los bordes. Con `r <= -0.5` toda la fila queda dentro de la forma.

```cpp
void drawOuter(Paint m, px* buf, const px* orig, int w, int h, double cx, double cy, double r) {
    const double rIn = r - 0.5, rOut = r + 0.5;
    for (int y = 0; y < h; ++y) {
        const double dy = y + 0.5 - cy;
        const size_t row = (size_t)y * w;
        const double ho2 = rOut * rOut - dy * dy;
        if (rOut <= 0 || ho2 <= 0) { fillFull(m, buf, orig, row, row + w); continue; }   // fila entera fuera
        const double ho = std::sqrt(ho2);
        const int lEnd   = (int)std::clamp(std::floor(cx - ho - 0.5) + 1.0, 0.0, (double)w);
        const int rStart = std::max((int)std::clamp(std::ceil(cx + ho - 0.5), 0.0, (double)w), lEnd);
        fillFull(m, buf, orig, row, row + lEnd);                  // fuera por la izquierda
        fillFull(m, buf, orig, row + rStart, row + w);            // fuera por la derecha
        int i0 = rStart, i1 = rStart;                             // interior intacto [i0, i1)
        const double ri2 = rIn * rIn - dy * dy;
        if (rIn > 0 && ri2 > 0) {
            const double hi = std::sqrt(ri2);
            i0 = std::clamp((int)std::max(0.0, std::ceil(cx - hi - 0.5)), lEnd, rStart);
            i1 = std::clamp((int)std::clamp(std::floor(cx + hi - 0.5) + 1.0, 0.0, (double)w), i0, rStart);
        }
        auto rim = [&](int x) {
            const double dx = x + 0.5 - cx;
            put(m, buf, orig, row + x, std::sqrt(dx * dx + dy * dy) - rIn);   // cover = distancia - (r - 0.5)
        };
        for (int x = lEnd; x < i0; ++x) rim(x);
        for (int x = i1; x < rStart; ++x) rim(x);
    }
}
```

#### 4.2.6 wipe y wave (barrido por una recta, con borde ondulado opcional)

- **Dirección de avance** del frente para un ángulo `θ` en grados: `dir = (-cos θ, sin θ)` en coordenadas de pantalla (y hacia abajo). Por tanto 0° = de derecha a izquierda, 90° = de arriba a abajo, 180° = de izquierda a derecha, 270° = de abajo a arriba. `left`, `right`, `top` y `bottom` son wipes con 180°, 0°, 90° y 270°.
- `s(p) = p·dir` es la posición de un punto a lo largo de la dirección de avance. El frente `front` es el valor de `s` hasta el que la forma ha llegado.
- **wave** desplaza el borde: `off = sin(perp / ancho) * alto`, con `perp = -px*dy + py*dx`. Por defecto `(ancho, alto) = (20, 20)`; un wipe normal es `alto = 0`.
- Cobertura exacta de un píxel: `cover = front + off - s + 0.5`; el borde mide 1 px en la dirección de avance (el recorte a 0..1 lo hace `put`).
- **Optimización por filas (obligatoria):** en una fila, la distancia lineal es `d(x) = a - dx*x` con `a = front - 0.5*dx - (y+0.5)*dy`. Hay un tramo `[f0, f1)` donde seguro `d >= margen` (relleno con `fillFull`), un tramo de borde donde se evalúa `wipeCover` píxel a píxel y el resto queda intacto. El margen es `|alto| + 0.5` (la ola puede mover el borde hasta `alto`).

```cpp
struct Wave { double width = 20, height = 0; };          // height 0 = wipe normal
inline double wipeCover(int x, int y, double dx, double dy, double front, Wave wv) {
    const double px = x + 0.5, py = y + 0.5;
    const double s = px * dx + py * dy;
    double off = 0;
    if (wv.height != 0) {
        const double perp = -px * dy + py * dx;
        off = std::sin(perp / std::max(std::abs(wv.width), 1e-3)) * wv.height;
    }
    return front + off - s + 0.5;                         // >= 1 cubierto del todo, <= 0 intacto
}
struct Span { size_t b0, f0, f1, b1; };                  // [f0,f1) lleno; [b0,f0) y [f1,b1) exactos; resto intacto
Span rowRanges(size_t w, double a, double dx, double m) {
    auto cl = [&](double v) { return (size_t)std::clamp(v, 0.0, (double)w); };
    if (std::abs(dx) < 1e-9)                              // frente horizontal: la fila entera es igual
        return a >= m ? Span{0, 0, w, w} : a <= -m ? Span{0, 0, 0, 0} : Span{0, 0, 0, w};
    if (dx > 0) {                                         // d decrece con x
        const size_t f1 = cl(std::floor((a - m) / dx) + 1.0);
        const size_t b1 = std::max(cl(std::ceil((a + m) / dx)), f1);
        return {0, 0, f1, b1};
    }
    const double k = -dx;                                 // d crece con x
    const size_t f0 = cl(std::ceil((m - a) / k));
    const size_t b0 = std::min(cl(std::floor((-m - a) / k)), f0);
    return {b0, f0, w, w};
}
void drawWipe(Paint m, px* buf, const px* orig, int w, int h, double dx, double dy, double front, Wave wv) {
    const double margin = std::abs(wv.height) + 0.5;
    for (int y = 0; y < h; ++y) {
        const double a = front - 0.5 * dx - (y + 0.5) * dy;
        const Span sp = rowRanges(w, a, dx, margin);
        const size_t row = (size_t)y * w;
        fillFull(m, buf, orig, row + sp.f0, row + sp.f1);
        for (size_t x = sp.b0; x < sp.f0; ++x) put(m, buf, orig, row + x, wipeCover(x, y, dx, dy, front, wv));
        for (size_t x = sp.f1; x < sp.b1; ++x) put(m, buf, orig, row + x, wipeCover(x, y, dx, dy, front, wv));
    }
}
```

La prueba más importante de este apartado: `drawWipe` debe dar **exactamente** el mismo buffer que evaluar `wipeCover` en todos los píxeles, para muchos ángulos y valores de `front` (4.2.11).

#### 4.2.7 fade, simple y none

```cpp
void drawFade(px* buf, const px* orig, size_t n, uint32_t keep) {   // keep: parte visible de orig, 0..256
    keep = std::min<uint32_t>(keep, 256);
    if (keep == 0) { std::fill(buf, buf + n, 0u); return; }
    for (size_t i = 0; i < n; ++i) buf[i] = scalePx(orig[i], keep);
}
```

- **fade:** `keep = round((1 - p) * 256)` en Reveal y `round(p * 256)` en Cover (con `std::lround`), con `p` ya suavizado por la bezier. El último fotograma: Reveal `0`, Cover `256` (copia exacta de `orig`).
- **simple:** igual que `fade`, pero el progreso es **lineal y por fotogramas** (4.2.8), no por reloj ni por bezier.
- **none:** es el último fotograma de `fade` pintado de golpe. Con Cover, el overlay debe quedar opaco; con Reveal no se anima nada (4.4).

#### 4.2.8 Conductor por fotograma

Sea `t = min(elapsed / duration, 1)` (reloj monótono, `QElapsedTimer`) y `p = bezierEase(t)`. Cada fotograma calcula `(p, done)` y llama al efecto con los parámetros de la tabla; `done` fuerza el estado final exacto.

| Efecto | Progreso `p` / `done` | Llamada |
|---|---|---|
| `none` | `p = 1`, `done` siempre | como `fade` con `done` |
| `simple` | `n` = nº de fotograma desde 1; `covered = min(n*step, 255)`; `p = covered/255` (lineal); `done` si `covered >= 255` | `drawFade` |
| `fade` | `p = bezier(t)`; `done` si `t >= 1` | `drawFade` |
| `wipe`, `wave`, `left`, `right`, `top`, `bottom` | `p = bezier(t)`; `done` si `t >= 1` | `drawWipe` con `front` (abajo) |
| `grow`, `center`, `any` | igual | `drawGrow`, `r = done ? rMax + 2 : p * rMax` |
| `outer` | igual | `drawOuter`, `rMax' = rMax + 1`, `r = done ? -1 : (1 - p) * rMax' - p` |

```cpp
std::pair<double,double> projRange(int w, int h, double dx, double dy) {   // min y max de p·dir en las 4 esquinas
    const double c[4] = {0.0, w * dx, h * dy, w * dx + h * dy};
    return {*std::min_element(c, c + 4), *std::max_element(c, c + 4)};
}
double maxRadius(double cx, double cy, int w, int h) {                       // distancia a la esquina más lejana
    return std::hypot(std::max(cx, w - cx), std::max(cy, h - cy));
}
double bezierEase(double x1, double y1, double x2, double y2, double t) {    // cubic-bezier CSS
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    auto coef = [](double a, double b) { const double c = 3 * a, bb = 3 * (b - a) - c; return std::array<double,3>{1 - c - bb, bb, c}; };
    auto ev = [](const std::array<double,3>& k, double s) { return ((k[0] * s + k[1]) * s + k[2]) * s; };
    const auto kx = coef(x1, x2), ky = coef(y1, y2);
    double lo = 0, hi = 1;
    for (int i = 0; i < 32; ++i) { const double mid = (lo + hi) / 2; (ev(kx, mid) < t ? lo : hi) = mid; }
    return ev(ky, (lo + hi) / 2);
}
// wipe / wave (wave solo si es wave; en wipe amp = 0)
const double a = angleDeg * M_PI / 180.0, dx = -std::cos(a), dy = std::sin(a);
const auto [lo, hi] = projRange(w, h, dx, dy);
const double amp = isWave ? std::abs(wave.height) : 0.0, mg = amp + 1.0;
const double front = done ? hi + mg : (lo - mg) + p * ((hi + mg) - (lo - mg));
drawWipe(paint, buf, orig, w, h, dx, dy, front, {isWave ? wave.width : 20.0, amp});
```

**Integración con Qt (backend CPU):**

- Un único `QImage frame` ARGB32 premultiplicado a resolución física, inicializado con `orig` (Reveal) o a ceros (Cover). Cada fotograma se **modifica en sitio** con la función del efecto; no se reconstruye ni se copia entero.
- `QRasterWindow::paintEvent` dibuja `frame` con `QPainter::CompositionMode_Source` (sin mezclar) y pide el siguiente fotograma con `requestUpdate()` mientras no se haya alcanzado `done`.
- El primer fotograma (overlay opaco en Reveal, transparente en Cover) se muestra **antes** de empezar a animar y se espera a que el compositor lo presente.
- Si se pierden fotogramas no importa: el progreso depende del reloj, no del número de fotogramas (excepto `simple`). Como `p` es monótono, nunca hay que deshacer nada.
- `simple` va con un temporizador a 30 fps (o el tope `fps` pedido): cada tic es un fotograma `n`.

#### 4.2.9 Centro de grow y outer (posición a píxeles)

Entrada: `Frac(f)` (fracción) o `Px(p)` (píxeles lógicos) por eje, tamaño lógico `(lw, lh)` y tamaño de buffer `(bw, bh)`.

- `x`: `Frac(f) -> f * bw`; `Px(p) -> p * bw / lw`.
- `y` (se mide **desde abajo**, salvo `invertY`): `Frac(f) -> (invertY ? f : 1 - f) * bh`; `Px(p) -> (invertY ? p : lh - p) * bh / lh`.
- Ejemplo comprobado (lógico 1000x500, buffer 2000x1000): `top` -> `(1000, 0)`, con `invertY` -> `(1000, 1000)`; `200,100` -> `(400, 800)`, con `invertY` -> `(400, 200)`.

#### 4.2.10 Errores típicos que hay que evitar

1. Mezclar con `QPainter` o con coma flotante en los bordes: pierde la igualdad exacta con los vectores.
2. Olvidar el fotograma final `done`: quedan píxeles semitransparentes o el overlay no queda perfecto.
3. Calcular en píxeles lógicos: el efecto sale ampliado o desplazado en pantallas escaladas.
4. Invertir el signo de la dirección: 0° debe ir de derecha a izquierda y 90° de arriba a abajo.
5. Medir la `y` de la posición desde arriba por defecto: en awww es desde abajo.
6. Reutilizar un buffer con estado de **otro** efecto o de otro modo (Reveal frente a Cover) sin reinicializarlo.
7. Dejar que el radio de `grow` (o el frente de `wipe`) retroceda: rompe el supuesto monótono.
8. En `outer`, empezar con radio demasiado pequeño: debe empezar en `rMax + 1` para que no haya nada transparente en el primer fotograma.
9. Asignar memoria o reescalar imágenes en el hilo de GUI durante la animación.

#### 4.2.11 Orden de trabajo y verificación (H1)

1. Leer 4.2 entera y el Apéndice C **antes** de escribir código.
2. Implementar las primitivas y comprobar `scalePx`, `keep256`, `bezierEase` con el Apéndice C.
3. `fade`, luego `grow` (con Reveal y Cover), luego `outer`, luego `wipe` y por último `wave`. Cada uno con sus vectores del Apéndice C **y** estas propiedades en QtTest:
    - incremental == directo: dibujar una secuencia creciente de parámetros en el mismo buffer da lo mismo que dibujar solo el último;
    - estado inicial (ceros en Cover, `orig` en Reveal) y estado final exacto;
    - Reveal + Cover suman alfa 254 a 256 por píxel;
    - `outer` en Reveal == `grow` en Cover con el mismo radio;
    - invariante premultiplicado: R, G y B nunca superan A;
    - `drawWipe` == evaluación píxel a píxel con `wipeCover`, para ángulos 0, 17, 45, 90, 133, 180, 225, 270 y 359, olas `(20,0)`, `(20,12)` y `(7,5)`, y 11 posiciones de `front` entre `lo-1` y `hi+1`;
    - `bezierEase` monótono y acotado en 0..1.
4. Solo entonces el conductor (4.2.8) y, por último, el overlay (4.3).

### 4.3 Overlay

- Una superficie por pantalla de destino, anclada a los 4 bordes, **exclusive zone -1** (cubre toda la salida aunque haya paneles), **entrada transparente** (los clics atraviesan), sin foco de teclado. Capa por defecto `Bottom` (por encima del fondo, por debajo de las ventanas); configurable.
- Imagen del overlay a **resolución física** (`size * devicePixelRatio`), no lógica; Qt la reduce al tamaño lógico. Verificar a escala 1.0, 1.25 y 1.5 que no hay zoom ni desplazamiento.
- Ajuste del fondo (`FillMode` de la config de Plasma): `0` estirar, `1` ajustar con barras del color `Color`, `2` recortar centrado (por defecto), `6` centrado sin escalar; 3 a 5 (mosaico) se aproximan con recorte.
- Cada pantalla usa su **propio** tamaño de modo; el desplazamiento entre pantallas de distinto tamaño fue un error real en la referencia.
- La decodificación y el reescalado de una imagen 4K cuestan unos 0.8 s: hacerlos en un hilo de trabajo (`QtConcurrent`), nunca en el hilo de GUI.

### 4.4 Orquestación

Transición **siempre que cambie el plugin** (salvo efecto `none`):

| Cambio | Dirección | Qué lleva el overlay |
|---|---|---|
| a `image` (desde cualquier cosa) | **Cover** | la imagen nueva (con el `FillMode`/`Color` ya guardados para `org.kde.image` en ese escritorio, si existen) |
| a `reborn` desde `org.kde.image` en **todas** las pantallas de destino | **Reveal** | la imagen vieja (ruta de `Image`, decodificada con percent-decoding; si el fichero no existe, no cuenta) |
| a `reborn` desde otra cosa | **Cover** | el primer fotograma del vídeo nuevo (`MediaCache`, ffmpeg); si no hay ffmpeg, cambio sin transición |

Secuencias:

- **Cover:** preparar overlay (ya compuesto en pantalla) -> animar -> cambiar plugin por escritorio -> esperar `hold` -> cerrar.
- **Reveal:** preparar overlay -> cambiar plugin -> esperar `hold` -> animar -> cerrar.
- `hold` por defecto: `reborn` 1500 ms, `image` 400 ms (tiempo para que cargue el fondo nuevo debajo).
- `none`: Cover no crea overlay; Reveal solo mantiene la imagen vieja durante `hold` (oculta el parpadeo de carga).
- Si falla D-Bus para un escritorio: respaldo escribiendo el fichero. Si D-Bus tuvo éxito en otros, esperar `hold` antes de releer (Plasma persiste su propio cambio de forma asíncrona) y no pisar su estado.
- Si no se puede crear el overlay, continuar **sin** transición y registrar el motivo.

### 4.5 Plasma: configuración y pantallas

- D-Bus: servicio `org.kde.plasmashell`, objeto `/PlasmaShell`, interfaz `org.kde.PlasmaShell`. `setWallpaper(plugin: s, params: a{sv}, screen: u)` y `evaluateScript(script: s) -> s`. Parámetros de imagen: `{Image: 'file:///ruta%20codificada'}` (percent-encoding de todo salvo `A-Za-z0-9-_.~/`). Parámetros de `reborn`: ver `video_params`/`compute_video_urls` en `main.rs` (playlist `VideoUrls` en JSON con `filename` y `enabled`, `LastVideo`, `LastVideoPosition=0`; las URL de `reborn` van **sin** codificar: `file://` + ruta).
- **Serialización de `VideoUrls` (requisito de bytes idénticos):** la página de configuración del plugin re-serializa la playlist con `JSON.stringify` al cargarla y marca la config como «con cambios sin aplicar» si los bytes difieren. El string debe emitirse con el orden exacto de campos por entrada que usa la página — `filename`, `enabled`, `duration`, `customDuration`, `playbackRate`, `alternativePlaybackRate`, `loop`, `dayNightPhase` (defaults `0,0,0,0,false,4` si faltan), compacto, sin espacios — y el vídeo objetivo añadido como **última entrada**. `plasmaapply::computeVideoUrls` lo implementa; lo pincha `test_videoUrls_matchPluginStringifyOutput`. No reordenar campos ni «limpiar» el JSON.
- Escritorios: contenedores `[Containments][N]` con `formfactor=0`; pantalla en `lastScreen`; plugin en `wallpaperplugin`; config del plugin en `[Containments][N][Wallpaper][plugin][General]`. Ids vivos: ver `live_desktop_ids` en `main.rs`.
- **Numeración de pantallas:** el id de Plasma (`lastScreen`, argumento `screen`) **no** sigue el orden de las salidas. Se obtiene preguntando a plasmashell con `evaluateScript`: `var r=[];for(var i=0;i<screenCount;i++){var g=screenGeometry(i);r.push(i+':'+g.x+','+g.y+','+g.width+','+g.height)};print(r.join(';'))`, y se casa cada `QScreen` por `geometry().topLeft()`. Si no es posible casar todas (sin D-Bus, salidas en espejo), caer al orden de `QGuiApplication::screens()` **y avisar**. El overlay se asigna a la `QScreen` por nombre de conector, nunca por índice.
- **Lockscreen** (opcional, apagado por defecto; sin API D-Bus, solo fichero): en `kscreenlockerrc`, `[Greeter] WallpaperPlugin=<plugin>` y `[Greeter][Wallpaper][<plugin>][General]` con `Image` (imagen) o `VideoUrls`, `LastVideo`, `LastVideoPosition` (vídeo). Es un único fondo para todas las pantallas.

### 4.6 Caché de medios (`ffmpeg`/`ffprobe` externos)

- Raíz: `<GenericCacheLocation>/kdewall-plugin-switcher/` (configurable por la app) con `resized/` y `first-frame/`. Nunca junto al vídeo.
- Nombre: `<sha256 del CONTENIDO>_<alto>p<fps>.mp4` y `<sha256>_<alto>p.png`. **Sin fecha ni ruta**: renombrar o mover no duplica. SHA-256 en streaming (bloques de 1 MiB, `QCryptographicHash`).
- Normalización a parámetros **efectivos**: alto = `min(pedido, alto del origen)` y par; FPS solo se aplica si es menor que el del origen (tolerancia 0.01); si no cambia nada, se usa el original y no se crea entrada. Así peticiones equivalentes comparten archivo. El fotograma sin escala pedida usa el alto del origen.
- Gramática: resize `<ALTO>p<FPS>` (`720p`, `1080p60`, `60` = solo FPS; sin ALTO = 1080p; sin FPS = los del vídeo); first-frame `<ALTO>p` (sin valor = dimensiones del vídeo).
- Órdenes: resize `-map 0:v:0 -an -sn -dn -vf scale=-2:H:flags=lanczos[,fps=N] -c:v libx264 -preset veryfast -crf 23 -pix_fmt yuv420p -movflags +faststart -f mp4`; first-frame `-frames:v 1 [-vf scale=-2:H:flags=lanczos] -c:v png -f image2 -update 1`. Siempre a un temporal `*.tmp<pid>` y `rename` atómico al final.
- Progreso: `-progress pipe:1 -nostats`, campo `out_time_us` entre la duración de `ffprobe` (señal `progress(0..1)`).
- `list` (marcando las entradas que aparecen en `appletsrc` o `kscreenlockerrc` por coincidencia de ruta) y `clean(all=false)` que conserva las usadas y borra siempre los temporales huérfanos.
- La lista de reproducción de Plasma queda apuntando a la copia en caché: borrarla rompe ese fondo hasta regenerarla; la app debe regenerar si falta.

## 5. API pública sugerida (esbozo, adaptar a la app)

```cpp
namespace KdeWall {
struct TransitionSpec { /* Q_GADGET: type, duration, fps, step, angle, pos, invertY, bezier, wave */ };
enum class Paint { Reveal, Cover };

class IOverlayBackend : public QObject { Q_OBJECT
public:
  struct Plane { int plasmaScreen; QImage image; };           // imagen ya compuesta a resolución física
  virtual void prepare(const QList<Plane>&, Layer, Paint) = 0;  // emite ready()
  virtual void animate(const TransitionSpec&) = 0;              // emite animationFinished()
  virtual void close() = 0;                                      // emite closed()
Signals: void ready(); void animationFinished(); void closed(); void failed(const QString&);
};

class WallpaperSwitcher : public QObject { Q_OBJECT
public:
  void switchToImage(const QUrl&, const SwitchOptions&);
  void switchToVideos(const QList<QUrl>&, const SwitchOptions&);   // SwitchOptions: screens, includeLockscreen, hold, layer, transition, resize, firstFrame
Signals: void stageChanged(Stage); void progress(double); void finished(); void failed(const QString&);
};

class MediaCache : public QObject {   // todo asíncrono
  void resized(const QUrl&, ResizeSpec); void firstFrame(const QUrl&, FrameSpec);
  // señales: ready(QString path, Source), progress(double), failed(QString)
};
}
```

## 6. Concurrencia (la app es gráfica: nada bloquea el hilo de GUI)

- `ffprobe`/`ffmpeg`: `QProcess` asíncrono. SHA-256 de ficheros grandes y decodificación/escalado de imágenes: `QtConcurrent`.
- D-Bus: `QDBusPendingCall` / `QDBusPendingCallWatcher`.
- Esperas (`hold`): `QTimer::singleShot`, no `sleep`.
- Animación: avance por **reloj** (`QElapsedTimer`) con `requestUpdate()`; `simple` por contador de fotogramas.
- `WallpaperSwitcher` como máquina de estados explícita (`Preparing`, `Animating`, `Switching`, `Holding`, `Closing`); una operación activa a la vez; si llega otra, cancelar limpiamente (cerrar overlay) antes de empezar.
- Mensajes de error al usuario con `i18n()`; el registro con categorías de logging de ECM, no con `qDebug` suelto.

## 7. Construcción y convenciones KDE

- CMake con ECM (`KDEInstallDirs`, `KDECMakeSettings`, `KDECompilerSettings`), C++20, `qt_add_library`, instalación de cabeceras y `*Config.cmake` si la biblioteca se comparte.
- Dependencias: Qt6 Core, DBus, Gui, Qml (+ Concurrent, Test); LayerShellQt6; KI18n solo en la capa de la app. `libwayland` no se necesita en el Plan A.
- Cabeceras SPDX/REUSE en cada fichero (`SPDX-FileCopyrightText`, `SPDX-License-Identifier`); licencia MIT (SPDX-License-Identifier: MIT), con la regla de sala limpia de la sección 11. Estilo `clang-format` de KDE.
- Nada de `exit()`, `eprintln`-equivalentes ni `sleep` en la biblioteca: errores por señal y registro por categoría.

## 8. Estrategia de pruebas

1. **Efectos (QtTest, sin Wayland):** portar las pruebas de `effects.rs`: monotonicidad y progreso incremental == directo; Reveal y Cover complementarios (alfa suma 254 a 256); wipe/wave idénticos a una evaluación píxel a píxel de referencia para ángulos 0, 17, 45, 90, 133, 180, 225, 270, 359 y olas `(20,0)`, `(20,12)`, `(7,5)`; estado final exacto; invariante premultiplicado (R,G,B <= A); bezier monótono. Usar además los vectores exactos del Apéndice C y las propiedades de 4.2.11.
2. **Contra la referencia:** generar con el binario Rust fotogramas de salida para parámetros fijos (volcado a PNG o hash) y comprobar que la versión C++ coincide **exactamente** en `grow`, `outer` y `wipe`.
3. **Opciones y rutas:** gramática de `pos`, `bezier`, `wave`, `resize`, `first-frame`; percent-encoding y decoding; expansión de `~`; propiedades del nombre de caché (mismo contenido y otro nombre, mismo archivo; parámetros equivalentes, mismo archivo).
4. **Plasma falso:** servicio D-Bus de prueba (apéndice B) que numera las pantallas al revés que el compositor; comprobar `--display`-equivalente `[0]`, `[0,1]`, rechazo de pantallas inexistentes, mapeo de `screenGeometry` y respaldo a fichero. Comprobar `kscreenlockerrc` en los tres casos (imagen, vídeo, fichero inexistente) y que **sin** la opción no se toca.
5. **Overlay real:** sway headless con dos salidas de distinto tamaño y capturas con `grim` (apéndice B).
6. **ffmpeg:** pruebas condicionadas a su presencia (clip sintético con `lavfi`).

## 9. Hitos y criterios de aceptación

- **H0 (spikes, antes de escribir código de producción):** S1 integración de shell de LayerShellQt con ventanas normales; S2 ventana transparente con entrada transparente y `exclusiveZone=-1` en KWin y en sway; S3 escalas 1.25 y 1.5 sin zoom ni desplazamiento; S4 ritmo por `requestUpdate()`; S5 mapeo `QScreen` a id de Plasma. Entregable: nota con resultados y elección Plan A o B.
- **H1:** `Effects`, `TransitionSpec`, pruebas 1 a 3. Aceptación: haber leído 4.2 y el Apéndice C antes de empezar; todo verde, coincidencia exacta con la referencia y con los vectores del Apéndice C.
- **H2:** `PlasmaConfig`, `PlasmaClient`, `ScreenMap`, `WallpaperSwitcher` sin overlay (cambio directo e `includeLockscreen`). Aceptación: prueba 4.
- **H3:** overlay y transiciones en ambas direcciones. Aceptación: escenarios de sway del apéndice B con las cifras indicadas y sin overlay residual.
- **H4:** `MediaCache`, `resize`, `firstFrame`, integración con Cover de `reborn`. Aceptación: prueba 6 y la norma de nombres sin duplicados.
- **H5:** capa QML, integración en la app, documentación, REUSE, paquetes. Aceptación: la app cambia un fondo con transición desde la interfaz sin bloquearse (sin tirones del hilo de GUI durante una generación de `resize`).

## 10. Riesgos y decisiones abiertas

1. **Licencia**: decidida (MIT, con regla de sala limpia respecto a awww); ver sección 11. Solo se reabre si se quiere copiar código de awww (entonces GPL-3.0-or-later).
2. **S1** puede forzar el Plan B (proceso auxiliar); el diseño ya lo contempla.
3. **GPU frente a CPU:** si el rendimiento de la CPU en pantallas 4K no cumple el presupuesto, evaluar el backend GPU.
4. **Plasma de la app:** las llamadas D-Bus `screenCount`/`screenGeometry` de `evaluateScript` no se han podido comprobar contra un plasmashell real; verificarlas en una sesión Plasma 6 y, si fallan, usar el orden de `QScreen` con aviso.
5. **Sin ffmpeg:** `resize` avisa y usa el original; `firstFrame` falla sin cambiar nada; el primer fotograma para Cover degrada a cambio sin transición.
6. **Tamaño de la lista de reproducción de `reborn`:** reproducir exactamente `compute_video_urls`; hay pruebas en la referencia que sirven de modelo.

## 11. Licencia y procedencia del código

**Decisión: MIT.** Todos los ficheros nuevos llevan `SPDX-License-Identifier: MIT` y se añade `LICENSES/MIT.txt` (REUSE). Es la licencia más permisiva de las habituales en el ecosistema KDE; si algún día se propone el código para los repositorios de KDE, confirmar con sus mantenedores que les vale. Cabecera tipo:

```
// SPDX-FileCopyrightText: 2026 <titular del copyright>
// SPDX-License-Identifier: MIT
```

**Por qué MIT es compatible con usar awww como referencia.** awww está bajo **GPL-3.0** (copyleft): una obra derivada de su **código** tendría que ser GPL-3.0. Este proyecto no es derivado: la referencia Rust se escribió a partir del **comportamiento documentado** en su página de manual (nombres de transición, opciones, convención de ángulos y de posición, curva por defecto), no de su código fuente. Los nombres de opciones y el comportamiento observable no los protege el copyright de un programa; el código sí. Las fórmulas de 4.2.4 a 4.2.8 son originales, y la de `wave` es una interpretación propia porque el manual no la define.

**Regla de sala limpia (obligatoria para el agente):**

- No abrir, copiar, traducir ni parafrasear el código fuente de awww/swww, ni el de otros proyectos GPL con transiciones o shaders.
- Implementar solo desde este documento, el Apéndice C y el código Rust de referencia. Ante una duda de comportamiento, consultar la página de manual `awww-img(1)`, no el código.
- Si el agente ya vio código de awww, debe decirlo antes de seguir, para revisar qué partes pudieron contaminarse.
- Si en el futuro se quiere reutilizar código de awww, **todo el componente pasa a GPL-3.0-or-later**. Es una decisión consciente y no se puede deshacer sobre código ya mezclado.

**Compatibilidad con el resto:**

- La app anfitriona puede ser GPL o LGPL: el código MIT se puede incluir en ella conservando su aviso.
- Qt6 y KF6 son LGPL y se enlazan dinámicamente sin problema. Comprobar la licencia exacta de LayerShellQt en la versión que se use.
- `ffmpeg` y `ffprobe` se ejecutan como procesos externos y no se enlazan: su licencia (que depende de cómo se compiló; por ejemplo, `libx264` es GPL) no afecta a la de este código.
- Dependencias nuevas: solo MIT, BSD, Apache-2.0 o LGPL con enlace dinámico; nada GPL dentro de la biblioteca.
- El código Rust de referencia es del propietario del proyecto; por coherencia debería llevar también MIT.

**Pendiente del propietario:** el nombre del titular del copyright para la cabecera.

## Apéndice A: lecciones aprendidas (errores reales de la referencia)

- El orden de registro de `wl_output` no es la numeración de Plasma: provocaba que el overlay saliera en otro monitor.
- Sin `exclusive zone -1`, KWin encogía el overlay para esquivar los paneles.
- Un buffer a tamaño físico sin viewport ni escala parecía ampliado y desplazado.
- No se puede congelar un vídeo: de ahí la dirección **Cover** con la imagen nueva o el primer fotograma.
- Rutas con `%20` en `Image` no se decodificaban y el overlay no aparecía.
- `~` entre comillas no lo expande el shell (en la app, expandir al recibir rutas de texto).
- Premultiplicado: `scale(p,a)` debe escalar también el canal alfa.
- `wl_buffer.release`: no reescribir un buffer que el compositor aún no ha soltado (Qt lo gestiona; relevante solo si se usara libwayland).

## Apéndice B: recetas de verificación

**Compositor headless (sway, soporta layer-shell y LayerShellQt):**

```
WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman sway -c sway.conf
# sway.conf
output HEADLESS-1 resolution 1280x720 position 0 0
output HEADLESS-2 resolution 1024x768 position 1280 0
output * bg #0000ff solid_color
```

Capturar con `grim -o HEADLESS-1 f.png`. El fondo azul hace de «vídeo en reproducción»: medir la fracción de píxeles `(0,0,255)` en cada captura. Lanzar sway con `setsid` si se ejecuta desde un entorno que mata los procesos hijos.

**Cifras esperadas** (duración 3 s, hold 2 s):

| Escenario | Fracción de azul en capturas sucesivas |
|---|---|
| a imagen desde vídeo (Cover) | ~37% a mitad del efecto, ~0% durante el hold, 100% tras cerrar |
| a reborn desde imagen (Reveal) | 0% durante el hold inicial, ~56 a 58% a mitad, 100% al terminar |
| otro plugin a reborn (Cover con primer fotograma) | igual que el primero |
| efecto `none` | sin overlay en ningún momento |

Con dos salidas y numeración de Plasma invertida (pantalla 0 = HEADLESS-2) el overlay debe aparecer solo en el monitor correcto, con la imagen de **ese** escritorio y sin zoom ni desplazamiento (diferencia media con el render esperado ≈ 1 sobre 255).

**Plasma falso por D-Bus** (con `dbus-run-session`, p. ej. `dbus-next` de Python): nombre `org.kde.plasmashell`, objeto `/PlasmaShell`, interfaz `org.kde.PlasmaShell`; `evaluateScript(s)->s` devuelve `0:1280,0,1024,768;1:0,0,1280,720` si el script contiene `screenCount`, y la lista de ids de escritorio en otro caso; `setWallpaper(s,a{sv},u)` registra plugin y pantalla.

**Protocolo del auxiliar (solo Plan B)**, líneas JSON por stdin/stdout: órdenes `prepare{planes,layer,paint}`, `animate{spec}`, `close`; eventos `ready`, `animationFinished`, `closed`, `failed{msg}`. Las imágenes se pasan como rutas de ficheros temporales ya compuestos, no por la tubería.

**Tabla de mapa Rust a C++:** `effects.rs`→`Effects`; `transitions.rs` (opciones)→`TransitionSpec`; `transitions.rs` (Wayland)→`LayerShellOverlay`; `monitors.rs`→`ScreenMap`; `lib.rs::Ini`→`IConfigStore`; `main.rs::switch/plan_overlay`→`WallpaperSwitcher`; `cache.rs`→`MediaCache`.

## Apéndice C: vectores de prueba exactos (salida de la referencia Rust)

Todas las pruebas usan `orig` = blanco opaco (`0xFFFFFFFF`). Se muestra el **alfa** (`píxel >> 24`) en hexadecimal por píxel: `00` transparente, `FF` opaco. Hay que reproducirlos exactamente, píxel a píxel. Si alguno difiere, el fallo está en la implementación, no en el vector.

**grow, Reveal.** w=9, h=7, centro (4.5, 3.5), r=3.2:

```
FF FF E7 75 4C 75 E7 FF FF
FF E7 20 00 00 00 20 E7 FF
FF 75 00 00 00 00 00 75 FF
FF 4C 00 00 00 00 00 4C FF
FF 75 00 00 00 00 00 75 FF
FF E7 20 00 00 00 20 E7 FF
FF FF E7 75 4C 75 E7 FF FF
```

**grow, Cover** (buffer inicial a ceros), mismos parámetros. Es además idéntico al de **outer, Reveal** con centro (4.5, 3.5) y r=3.2 (los dos dan exactamente esta salida):

```
00 00 17 89 B2 89 17 00 00
00 17 DE FF FF FF DE 17 00
00 89 FF FF FF FF FF 89 00
00 B2 FF FF FF FF FF B2 00
00 89 FF FF FF FF FF 89 00
00 17 DE FF FF FF DE 17 00
00 00 17 89 B2 89 17 00 00
```

**wipe, Reveal.** w=10, h=6, ángulo 30°, `dir = (-0.866025, 0.500000)`, rango de proyección `(-8.660254, 3.000000)`, `front = lo + (hi - lo) / 2 = -2.830127`, `wave = (20, 0)`. Se llama a `drawWipe` directamente con ese `front`, sin el margen del conductor:

```
FF FF FF 8C 00 00 00 00 00 00
FF FF FF FF 2E 00 00 00 00 00
FF FF FF FF AE 00 00 00 00 00
FF FF FF FF FF 50 00 00 00 00
FF FF FF FF FF D0 00 00 00 00
FF FF FF FF FF FF 72 00 00 00
```

**wave, Reveal.** w=12, h=6, ángulo 0° (`dir = (-1, 0)`), rango `(-12, 0)`, `front = -6`, `wave = (3, 1.5)`; mismo modo de llamada:

```
FF FF FF FF FF FF 3F 00 00 00 00 00
FF FF FF FF FF FF B7 00 00 00 00 00
FF FF FF FF FF FF FF 1B 00 00 00 00
FF FF FF FF FF FF FF 60 00 00 00 00
FF FF FF FF FF FF FF 7E 00 00 00 00
FF FF FF FF FF FF FF 72 00 00 00 00
```

**bezierEase(0.54, 0, 0.34, 0.99, t)**, tolerancia 1e-6:

| t | 0.1 | 0.25 | 0.5 | 0.75 | 0.9 |
|---|---|---|---|---|---|
| resultado | 0.013002 | 0.105114 | 0.603659 | 0.932590 | 0.990049 |

**scalePx(p, a):**

| p | a | resultado |
|---|---|---|
| `0xFF4080C0` | 256 | `0xFF4080C0` |
| `0xFF4080C0` | 128 | `0x7F204060` |
| `0xFF4080C0` | 0 | `0x00000000` |
| `0xFFFFFFFF` | 1 | `0x00000000` |
| `0xFFFFFFFF` | 255 | `0xFEFEFEFE` |

**Fotogramas finales:** `grow` con `r = rMax + 2` y `outer` con `r = -1` sobre cualquier tamaño dejan el buffer entero a `0` en Reveal; en Cover, `grow` final (`rMax + 2`) y `outer` final (`-1`) dejan el buffer idéntico a `orig`.
