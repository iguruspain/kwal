# Plan — Kwal: wallpapers (vídeos, caché, colores, paletas, aplicación, resize, transiciones)

> **FUENTE DE VERDAD ÚNICA.** Este documento consolida `KWAL_WALLPAPERS_PLAN.md` (el plan con las decisiones correctas D1–D17) y lo útil de `plan-implementacion-wallpapers.md` (el plan que el agente ejecutó y fue modificando). Sustituye a ambos. No se edita ninguna decisión sin aprobación del usuario.
>
> **Estado:** P1, P2 y P3 están **implementadas bajo el plan antiguo**; la auditoría de código (2026-10-22) fijó la reconciliación final en **§2b**, que incluye tres puntos donde **el código se bendice y el plan se corrige** (R6, R7, R8). P4 y P5 están **implementadas** (2026-10-22); P6–P7 siguen **pendientes** y siguen este documento tal cual.
>
> | Fase | Estado | Acción |
> |---|---|---|
> | P1 Caché + vídeos en rejilla | implementada | ninguna (R8 resuelto: el diseño implementado se bendice — D16) |
> | P2 Colores + búsqueda | implementada | corregir R1–R5, R9; migración R10 al final (R7 → P4) |
> | P3 Drawer de detalle | implementada | reconciliada (R4 hecha: paleta mcu, 16 swatches; drawer modal desde `98d036d`) |
| P4 Aplicación | implementada (2026-10-22) | hecha: D-Bus `setWallpaper` + fallback `appletsrc` (D9), fallback vídeo `org.kde.video` (D11), lockscreen (D15) + toggle, JS muerto eliminado; verificado (build limpio + `tst_plasmaapply`) |
| P5 Resize | implementada (2026-10-22; on-demand 2026-10-23) | hecha: plan ffprobe/ffmpeg, `video_resizes/`, **generación on-demand** (botón "Generate resizes" por carpeta + batch con progreso), objetivo `resized` en el apply; verificado (build limpio + `testVideoResize`) |
> | P6 Transiciones | pendiente | |
> | P7 Limpieza de huérfanos | pendiente | extiende `sweep()` existente (D12 reformulado) |

## 1. Objetivo

Implementar las funcionalidades de wallpapers recogidas en `diseñando.txt`:

1. Caché de medios + JSON de información por wallpaper (clavado por MD5).
2. Soporte de **vídeos como wallpaper** (miniatura = primer frame, reproducción al seleccionar, aplicación vía plugin *reborn*).
3. Colores de matugen (`source_color_index`), categorías de color y tags editables.
4. Drawer de detalle al doble clic (categorías, tags, colores matugen, paletas).
5. Nuevos backends de paletas: **Material You** y **Imagemagick** (reimplementados más simples).
6. **Nueva forma de aplicar** wallpapers (image y video): D-Bus `setWallpaper` + fallback `appletsrc`.
7. **Resize** opcional de vídeos (ffmpeg) antes de aplicar.
8. **Transiciones** de wallpaper — se implementa siguiendo `plan-implementacion-transiciones-plasma.md` (documento de referencia; **no se copia aquí**).

Referencias de comportamiento (solo referencia, no de patrones):

- `../Kwal` — app Python original: features de referencia (matugen, categorías, lightbox, paletas, búsqueda).
- `~/VSCode/kdewall-plugin-switcher` — app Rust: referencia de comportamiento para caché, D-Bus, plugin *reborn*.

Herramientas verificadas en este sistema (disponibles): `ffmpeg`, `ffprobe`, `matugen`, `magick`/`convert`, plugin *reborn* (`~/.local/share/plasma/wallpapers/luisbocanegra.smart.video.wallpaper.reborn`), módulo QML `QtMultimedia` (Qt 6.12).

## 2. Decisiones (fijadas; cualquier cambio requiere aprobación)

| # | Decisión | Nota |
|---|---|---|
| D1 | Clave de caché: **MD5** del contenido del archivo — clave de los **artefactos de caché** (nombres `video_frames/<md5>.png`, `video_resizes/<md5>_*`). El índice `wallpapers_info.json` **no** se clava por md5: es un **array versionado de entradas** (formato bendecido, R6) | Según `diseñando.txt` (el ref. Rust usa SHA-256; aquí manda el diseño). El plan original describía un mapa `md5 → entry` en el JSON; la auditoría (2026-10-22) bendice el formato implementado (R6). |
| D2 | Caché en `~/.cache/kwal/` (`QStandardPaths CacheLocation` + `/kwal`): `video_frames/`, `video_resizes/`, `thumbnails/large/` (caché FDO **propia de la app** — bendecida, D16/R8), `wallpapers_info.json` | La caché de thumbnails del sistema (`~/.cache/thumbnails/`) **no se usa ni se toca** (verificado `fdothumbnailprovider.cpp:79-87`). |
| D3 | `source_color_index`: **todos los colores** que imprime `matugen image <ruta> --show-source-colors` (número variable, normalmente 3–4 — verificado empíricamente), hex en minúsculas, dedup preservando el orden de salida; sin cap fijo, sin fallback: si matugen no está o falla, lista vacía | Para vídeos, matugen sobre el PNG del primer frame. Igual que el app viejo (`_extract_colors_matugen` guarda todos los hexes; su `count=8` era un tope que nunca se alcanzaba). |
| D4 | Clasificación de color: **círculo cromático estándar (12 sectores) en HCT/OKLCH** — el algoritmo del app viejo (`color_extractor.get_color_category`, `color_extractor.py:54-124`) portado 1:1 con la clase `mcu::Hct` de la librería D6 (el viejo usaba `Hct.from_int` de materialyoucolor; `mcu::Hct` es el mismo HCT CAM16/OKLCH — la misma rueda, sin derivación nueva; buscado: no existe librería C++ dedicada a clasificar por sectores de rueda). `src/colorcategory.{h,cpp}`: `QString colorCategory(uint32_t argb)` (internamente `mcu::Hct(argb)` → `hue()/chroma()/tone()`) + `const QStringList &kCategoryList()`. Reglas (portadas del viejo): `tone<20` → `black`; `tone>90 ∧ chroma<10` → `white`; `chroma<10` → `gray`; si no, sectores por matiz OKLCH: rose `<15 ∨ ≥348.5`, red `<40`, orange `<81.8`, yellow `<123.6`, yellow-green `<139.0`, green `<147.4`, cyan-green `<174.6`, cyan `<230.2`, blue-cyan `<273.4`, blue `<293.7`, violet `<319.6`, magenta `<348.5`. `kCategoryList()` = 15 nombres (viejo `CATEGORY_LIST`, mismo orden); swatches UI = hex del viejo `categoryColorModel` (verificado `wallpapers.qml:22-38`): red `#E53935`, orange `#FB8C00`, yellow `#FDD835`, yellow-green `#7CB342`, green `#43A047`, cyan-green `#00897B`, cyan `#00ACC1`, blue-cyan `#1E88E5`, blue `#3949AB`, violet `#8E24AA`, magenta `#D81B60`, rose `#F06292`, black `#212121`, gray `#757575`, white `#F5F5F5`. | Sin dependencias nuevas más allá de mcu (D6). |
| D5 | `color_categories` = **conjunto ordenado** de `colorCategory(c)` (D4) sobre los **top 8 colores de la imagen** (vídeo → su `first_frame`), extraídos con mcu (`QuantizeCelebi(pixels,128)` → `RankedSuggestions{desired=8, filter=true}`) — mismo pipeline que el app viejo (Celebi materialyoucolor → clasificación HCT), ahora en C++ con la librería D6. **Independiente de `source_color_index`** (D3, matugen): esa lista solo aparece en los dots del drawer y **nunca** participa en la búsqueda (D8) ni en `color_categories`. La edición manual (drawer, P3) **sobrescribe** la lista persistida (override manual, como el viejo `update_entry_categories`), sin límite (D13). | Dos listas, dos pipelines: matugen → dots; imagen → categorías. |
| D6 | Paleta Material You: **librería C++ oficial de Google `material-color-utilities`** (Apache-2.0) — no reimplementación del algoritmo. Integración: CMake `FetchContent` fijado al commit `bd811e4ca2ab94c4ce3cadc89c19b1820b9704b3` (el repo no tiene tags); include dir = raíz del repo (las cabeceras hacen `#include "cpp/..."`); lib estática `mcu` con **todos** los `.cc` de `cpp/` excepto `*_test.cc` (27 archivos: blend, cam×4, contrast, dislike, dynamiccolor×3, palettes/tones, quantize×4, scheme×9, score, temperature, utils); conservar cabeceras de copyright y atribución Apache-2.0 (compatible con el GPLv3 del app). El proyecto compila en C++20 (`CMakeLists.txt:9`), suficiente para la librería (usa `std::optional`, C++17). Dependencia nueva, aprobada explícitamente por el usuario. | Pipeline (nada de matugen): `QImage` del wallpaper (vídeo → `first_frame`) `scaled(128,128,KeepAspectRatio,SmoothTransformation)` → `std::vector<Argb>` de píxeles con alfa>128 → `QuantizeCelebi(pixels, 128)` → `RankedSuggestions(qr.color_to_count, ScoreOptions{desired=3, filter=true})` → seed = `ranked[0]` → `SchemeX(Hct(seed), is_dark)`. 16 swatches en orden: `GetBackground, primary_palette.get(80), primary_palette.get(70), secondary_palette.get(70), secondary_palette.get(60), tertiary_palette.get(60), tertiary_palette.get(50), GetOnBackground, neutral_palette.get(50), GetPrimaryContainer, GetSecondaryContainer, GetTertiaryContainer, GetSurfaceContainer, GetSurfaceContainerHigh, GetOnSurface, GetOnSurfaceVariant`; hex vía `HexFromArgb`. Namespace `material_color_utilities`, `Argb = uint32_t`. UI: **ComboBox con los 9 schemes mcu** (`TonalSpot`, `Vibrant`, `Expressive`, `Content`, `FruitSalad`, `Rainbow`, `Monochrome`, `Neutral`, `Fidelity`), default `TonalSpot`, solo visible con backend Material You (el selector que tenía el app viejo; **añadido 2026-10-22 con aprobación del usuario** — antes estaba en no-metas). |
| D7 | Imagemagick: `magick <img> -resize 128x128 -colors 16 -unique-colors txt:-` (≤16 colores) + accent por `histogram:info` (64×64, 8 colores, mejor score HSV con s>0.15, 0.15<v<0.95) | Igual que el app viejo. |
| D8 | Búsqueda: texto = subcadenas **case-insensitive** sobre el nombre del archivo **o cualquier tag**; categorías = chips con lógica AND (todas las seleccionadas deben estar en la entrada) | |
| D9 | Aplicación: D-Bus `org.kde.plasmashell.setWallpaper(plugin, params, screen)` por desktop; ids vivos por `evaluateScript` (fallback: `appletsrc` `formfactor=0`); `lastScreen` de `appletsrc` (default 0); fallo por desktop → **fallback por archivo** (`appletsrc`) releyendo el archivo si algún desktop fue por D-Bus | Param. `Image` = `file://` percent-encoded; *reborn* = URLs **sin** encoding (`file://` + ruta). |
| D10 | Playlist *reborn* (`VideoUrls`): el app **deshabilita** las entradas ajenas al objetivo al aplicar (no se portea `no_disable`); merge conserva claves de entradas existentes; entrada por defecto si no existe (`duration 0, customDuration 0, playbackRate 0, alternativePlaybackRate 0, loop false, dayNightPhase 4`) | |
| D11 | Aplicar vídeo: si hay plugin *reborn* → vídeo (params D9/D10); **si no, aplicar el `first_frame` PNG como imagen** (`org.kde.image`, `{Image: fileUrl(frame)}`); si tampoco hay `first_frame` (ffmpeg falló) → el apply falla con mensaje (la rejilla sigue mostrando vídeos) | Fallback decidido por el usuario (2026-10-21). |
| D12 | **Limpieza de huérfanos** (P7; decisión del usuario 2026-10-21, reformulada 2026-10-22): `WallpapersInfo::sweep()` **ya existe** (verificado `wallpapersinfo.cpp:303-344`: tras cada escaneo borra entradas JSON cuyo `file` no existe y frames huérfanos de `video_frames/` por referencia directa de ruta). P7 la extiende: (1) `sweep()` también **al arrancar** (tras `load()`); (2) purga de `video_resizes/` (fichero sin entrada que lo reference vía `resized`); (3) playlist *reborn* de `appletsrc`: eliminar entradas de `VideoUrls` cuya ruta ya no existe (editor `Ini` lineal de P4). Eliminaciones best-effort (`QFile::remove`, log del fallo). **Fuera:** `~/.cache/kwal/thumbnails/large/` (el tema de thumbnails queda **tal cual** — carpeta propia de la app confirmada como la adecuada, 2026-10-22; los huérfanos son pequeños y se regeneran al pedirlos) y el wallpaper "actual" de `appletsrc`/`kscreenlockerrc` apuntando a un fichero borrado (lo reescribe el próximo apply). | |
| D13 | Auto-cálculo de categorías por wallpaper: **top 5 por dominancia** (revisión del 2026-10-22 que sustituye el "sin límite" del 2026-10-21): los top-8 de R3 ya vienen ordenados por importancia (`RankedSuggestions`), así que clasificar → dedup preservando el orden → **truncar a 5** (lo que se corta es lo menos dominante). Edición manual: **sin límite** (cualquiera de las 15, ya implementado). | Decidido por el usuario (2026-10-22): uniformidad — top 5 por dominancia. |
| D14 | Defaults: matugen dots **on** (si matugen está), **categorías on (siempre** — no dependen de matugen, D5/R9), resize **off** (altura 1080, fps 0=original), **lockscreen off** (el doc de transiciones lo fija "apagado por defecto"), transiciones **off** (duración 0.9 s, tipo `left`) | |
| D15 | **Lockscreen: se implementa** (decisión del usuario 2026-10-21; sustituye el anterior "fuera de alcance"). Config: `[Wallpapers] lockscreen` (bool, default false). Mecanismo (línea 374 del doc de transiciones): **sin API D-Bus**, escritura directa de `~/.config/kscreenlockerrc` con el mismo editor `Ini` lineal que `appletsrc`: `[Greeter] WallpaperPlugin=<plugin>` + `[Greeter][Wallpaper][<plugin>][General]` con `Image=fileUrl` (imagen) o `VideoUrls`/`LastVideo=rawVideoUrl`/`LastVideoPosition=0` (vídeo); un único fondo para todas las pantallas. Se aplica en P4 (tras los desktops) y en P6 (`SwitchOptions.includeLockscreen` = la misma config). UI: toggle en `WallpapersSettingsPage` — **se añade en P4** junto con la escritura (revisión 2026-10-22, ex-R7: no se crea un setting muerto en P2). | |
| D16 | Miniatura de vídeo (revisión 2026-10-22; **reemplaza** el "puente" original): el `ThumbnailRole` del vídeo parte **vacío** (icono genérico, sin petición al proveedor) y, al llegar `first_frame` (`entryChanged`), se fija a **`image://fdo_thumbnail/<frame>`**: el frame es un PNG que `QImageReader` decodifica como imagen normal — **sin puente** en `FdoThumbnailProvider` (verificado `wallpapermodel.cpp:248-309`, `fdothumbnailprovider.cpp:50-77`). El frame vive en `video_frames/<md5 del contenido>.png` (ruta estable por contenido), así que la caché de thumbnail es estable sin más mecanismo. El thumbnail resultante se cachea en `~/.cache/kwal/thumbnails/large/<md5(URI percent-encoded del frame)>.png` (convención FDO, **directorio propio de la app** — `fdothumbnailprovider.cpp:79-87`; **carpeta confirmada como la adecuada**, 2026-10-22). El cambio de valor del rol (vacío → URL) obliga a QML a re-pedir; un `dataChanged` con la misma URL no re-dispararía al provider. | Diseño implementado, más simple que el puente original del plan y sin coste de caché; se bendice tal cual. |
| D17 | Licenciamiento de `src/transitions/` según el §11 del documento de transiciones (MIT, clean-room) | |

## 2b. Reconciliación de P1–P3 (código existente vs. decisiones correctas)

P1–P3 se escribieron con el plan antiguo. Esta sección lista **cada divergencia** y su resolución final, fijada por la auditoría de código (2026-10-22) con evidencia `archivo:línea`. Tres puntos —R6, R7, R8— se resuelven **corrigiendo el plan** (el código está bien o es mejor). **Estado (2026-10-22): R1–R5, R9 y R10 implementados y verificados (build + ctest); R7 pliegue en P4.**

**Orden de corrección:** R4 (integrar `mcu`, es la base de R2/R3) → R2 → R3 → R5 → R9 → R1 → R10 (migración de datos, al final y con confirmación del usuario). R7 se pliega en P4; R6 y R8 no tocan código.

### R1 — `source_color_index` sin tope (D3)
- **Antes:** cap de 8 (`kMaxSourceColors`, `wallpapersinfo.cpp:23,57`), dedup en orden.
- **Ahora:** **todos** los hex que imprime `matugen image <ruta> --show-source-colors` (normalmente 3–4), minúsculas, dedup preservando el orden de salida, sin tope; sin matugen o fallo → lista vacía.
- **Acción:** quitar `kMaxSourceColors` en `extractSourceColors`; el `matugenRow` del drawer ya soporta longitud variable. Auditoría 2026-10-22: matugen imprime 3–4 colores, así que el cap nunca dispara en la práctica — corrección de spec, no de comportamiento visible.

### R2 — Clasificador de categorías HCT/OKLCH (D4)
- **Antes:** clasificador simple por tono/luminosidad/saturación (verificado `WallpapersInfo::colorCategory`, `wallpapersinfo.cpp:540-592`), no HCT.
- **Ahora:** port 1:1 del algoritmo del app viejo con `mcu::Hct` (`src/colorcategory.{h,cpp}`: `QString colorCategory(uint32_t argb)` + `kCategoryList()` con 15 nombres). Umbrales exactos en D4.
- **Acción:** reemplazar el clasificador (borrar el antiguo, sin dejar ambos); migrar los callers (los hex de `source_color_index` pasan a ARGB). Sustituir los tests por la tabla hex → categoría de D4 (umbrales de sector y orden black/white/gray).

### R3 — `color_categories` desde la imagen, no desde matugen (D5)
- **Antes:** categorías = `colorCategory` sobre `source_color_index`, calculadas **solo** si matugen está activo (verificado `wallpapersinfo.cpp:251-267`). Sin matugen (o con dots y categorías off) → `color_categories` vacío y la búsqueda por chips no existe.
- **Ahora:** top-8 colores de la **imagen** (vídeo → `first_frame`) con mcu (`QuantizeCelebi(pixels,128)` → `RankedSuggestions{desired=8, filter=true}`) → `colorCategory` → conjunto ordenado. **Independiente de matugen**: se calcula siempre. `source_color_index` solo alimenta los dots del drawer y nunca participa en búsqueda ni en categorías.
- **Acción:** en `processFile` (`wallpapersinfo.cpp:241-286`): mover el cálculo de categorías **fuera** del `if (m_matugenEnabled && matugenAvailable())`; vídeo sin `first_frame` → vacío y reintento en el próximo escaneo. Es la corrección de mayor valor de P2: convierte la búsqueda por color de "feature de esta máquina" (dependía de matugen) en feature del producto, y pasa de 3–4 a 8 colores de entrada.

### R4 — Paleta Material You con la librería oficial (D6)
- **Antes:** median-cut propio + rampas HSL, 13 swatches (verificado `paletteextractor.cpp:20-275`); `mcu` no está en `CMakeLists.txt` (el proyecto ya usa `FetchContent` con `tomlplusplus`: mismo patrón).
- **Ahora:** `material-color-utilities` (Google, Apache-2.0) vía CMake `FetchContent` al commit `bd811e4ca2ab94c4ce3cadc89c19b1820b9704b3`, lib estática `mcu` (todos los `.cc` de `cpp/` salvo `*_test.cc`, 27 archivos), include dir = raíz del repo. Pipeline y **16 swatches** exactamente como en D6. **Confirmada por el usuario (2026-10-22): se quieren paletas Material You de verdad.**
- **Acción:** integrar `mcu` en `CMakeLists.txt`, reescribir `PaletteExtractor` (Material You) con `QuantizeCelebi → RankedSuggestions(desired=3) → SchemeTonalSpot`, eliminar el median-cut/rampas HSL, y actualizar la rejilla del drawer a 16 swatches. Añadir `tests/tst_paletteextractor.cpp` determinista (fixture de pixmap sintético; era "opcional" en el plan antiguo). Conservar cabeceras de copyright/atribución Apache-2.0.

### R5 — Top-5 de categorías automáticas por dominancia (D13)
- **Antes:** cap de 5 **solo** en la auto-cálculo (`kMaxCategories`, `wallpapersinfo.cpp:24,262`), aplicado en orden de salida de matugen (no por dominancia). Auditoría 2026-10-22: el menú "Añadir" del drawer (`WallpaperDetailDrawer.qml:259-260`) y `setCategories` (`wallpapersinfo.cpp:482-495`) **ya** son sin cap — el plan antiguo los citaba por error.
- **Ahora:** auto-cálculo = **top 5 por dominancia** (D13, revisión 2026-10-22): top-8 colores de R3 (ya en orden de importancia) → `colorCategory` → dedup preservando el orden → truncar a 5. Edición manual sin límite (las 15).
- **Acción:** en `processFile`, mantener `kMaxCategories` (5) aplicado **tras** el dedup de R3 — el orden de entrada pasa a ser de dominancia (cambia la lista, el cap queda). Tests: fixture con top-8 de 6+ categorías distintas conserva las 5 primeras por dominancia; `setCategories` no cap.

### R6 — El formato JSON implementado es el canónico (corregir el plan, no el código)
- **Descubierto en la auditoría (2026-10-22):** el JSON **no** es el mapa `md5 → entry` de P1, sino `{ "version": 1, "files": [ …entries… ] }` con 3 claves opcionales de estado: `matugen_done`, `categories_manual`, `tags_manual` (verificado `wallpapersinfo.cpp:372-449`).
- **Decisión:** el formato implementado **se bendice** como canónico (es mejor: campo `version` para migrar, legible, y los flags protegen tags/categorías manuales en el rescan — `wallpapersinfo.cpp:229-232,269-283`). D1 se actualiza (md5 = clave de los artefactos de caché, no del índice). Solo cambia el *contenido* de `source_color_index` y `color_categories` (R1–R3). **Acción:** ninguna de código.

### R7 — Lockscreen (D14, D15) — pliegue en P4
- **Descubierto en la auditoría (2026-10-22):** no hay config ni toggle de lockscreen en `src/`/`qml/` (coherente: la escritura de `kscreenlockerrc` es de P4).
- **Decisión (2026-10-22):** el toggle **no** se añade en P2: sería un setting muerto hasta que P4 escriba `kscreenlockerrc`. Config (`[Wallpapers] lockscreen`, default false) + Switch en `WallpapersSettingsPage` + escritura se implementan juntos en **P4** (D15); `includeLockscreen` en P6.
- **Acción:** ninguna ahora; ver P4.

### R8 — Miniatura de vídeo: el diseño implementado se bendice (corregir el plan, no el código)
- **Descubierto en la auditoría (2026-10-22):** el código pide el frame directamente — `ThumbnailRole` vacío al escanear y `image://fdo_thumbnail/<frame>` al llegar `first_frame` (`wallpapermodel.cpp:248-309`); `FdoThumbnailProvider` **no** tiene puente y no lo necesita: el frame es un PNG que `QImageReader` decodifica (`fdothumbnailprovider.cpp:50-77`).
- **Decisión (2026-10-22, confirmada por el usuario):** **no implementar** el puente. El frame vive en `video_frames/<md5 del contenido>.png` (ruta estable por contenido), así que la caché de thumbnail es estable; el puente solo cambiaría el md5 bajo el que se guarda el PNG (URI del frame vs URI del vídeo) a costa de más complejidad (fallback en el provider + cambio de valor del rol). La carpeta de caché `~/.cache/kwal/thumbnails/large/` (propia de la app) **es la adecuada y queda tal cual**.
- **Acción:** ninguna de código. D16 se reescribe para describir el diseño implementado (el plan original citaba por error la caché del sistema `~/.cache/thumbnails/large/`); D12 deja los thumbnails fuera de la purga.

### R9 — Chips de categoría siempre visibles (P2)
- **Antes:** la fila de chips solo se mostraba con `app.matugenAvailable && app.colorCategoriesEnabled` (verificado `WallpapersPage.qml:148`; el switch de categorías de settings también gated — `WallpapersSettingsPage.qml:44-47`), y `colorCategoriesEnabled` hacía default a `matugenAvailable` (`appcontroller.cpp:431`).
- **Ahora:** visible **siempre** que `colorCategoriesEnabled` (las categorías no dependen de matugen, D5); 15 chips con el swatch hex de D4 + chip "Limpiar"; lógica AND (D8).
- **Acción:** quitar la condición `matugenAvailable` de la fila de chips y del switch de categorías; default de `colorCategoriesEnabled` → **true** (D14). El switch de *dots* sigue gated por matugen (los dots son de matugen, D3).

### R10 — Migración de datos ya generados (simplificada por R6)
- Las entradas existentes en `~/.cache/kwal/wallpapers_info.json` tienen `color_categories` y `source_color_index` calculados con el pipeline antiguo (clasificador HSL, cap 8, de matugen). Los `tags` y las ediciones manuales de categorías son datos del usuario.
- **Decisión (2026-10-22):** migración única (no borrado), con el mecanismo que el formato bendecido (R6) ya trae: campo `version` del JSON + flags `categories_manual`/`tags_manual`. Al cargar, si `version < 2`: guardar `version: 2` y re-encolar todas las entradas a `processFile`; el carry existente preserva las categorías manuales (`wallpapersinfo.cpp:269-283`) y los `tags` reales siempre tienen `tags_manual: true` (los pinta `setTags`, `wallpapersinfo.cpp:499-511`), así que **no se pierde ninguna edición del usuario**. Sin claves nuevas en `kwalrc` (no hace falta `infoVersion`).
- **Acción:** implementarla **al final** de la reconciliación, con confirmación del usuario (pisa solo las categorías auto-calculadas del periodo de pruebas).

### Lo que se conserva del plan antiguo (ya integrado en P3 más abajo)
Comportamiento del drawer: drawer **modal** (`modal: true`, `handleVisible: true`, ~40% del ancho de la ventana — `98d036d`, cambio posterior a este plan; originalmente no-modal, ~60%, sin handle), `drawerOpen: false` fijado, instanciación perezosa vía `Loader`, capa base con primer frame, patrón *armed* con `Timer` de 250 ms, `MediaPlayer` destruido (no pausado) al cerrar/cambiar, guarda `detailOpen` en `WallpaperItem`. Cierre por X o Escape (`closePolicy: Popup.CloseOnEscape`); un clic fuera **no** lo cierra. No tocar salvo que contradiga una decisión.

## 3. Cambios transversales (resumen)

- **Nuevos C++:** `src/wallpapersinfo.{h,cpp}`, `src/colorcategory.{h,cpp}`, `src/paletteextractor.{h,cpp}`, `src/plasmaapply.{h,cpp}` (+ `src/transitions/…` en P6).
- **Modificados:** `src/appcontroller.{h,cpp}` (config nueva, apply nuevo), `src/wallpapermodel.{h,cpp}` (vídeos, filtros, roles), `src/fdothumbnailprovider.cpp` (caché de thumbnails propia `~/.cache/kwal/thumbnails/`, sin puente — D16), `src/main.cpp` (init `WallpapersInfo`, Qt6::Multimedia), `CMakeLists.txt` (fuentes, `Qt6::Multimedia`, `FetchContent` de `material-color-utilities` en P2, `LayerShellQt::Interface` en P6), `qml/Main.qml`, `qml/WallpapersPage.qml`, `qml/WallpaperItem.qml`, `qml/SettingsWindow.qml`.
- **Nuevos QML:** `qml/WallpaperDetailDrawer.qml`, `qml/WallpapersSettingsPage.qml` (reemplaza al placeholder de Wallpapers).
- **Nuevos tests:** `tests/tst_wallpapersinfo.cpp`, `tests/tst_plasmaapply.cpp`; extensión de `tests/tst_appcontroller.cpp`.
- **KConfig `[Wallpapers]`** (grupo existente, se extiende; conserva `folders`, `currentFolderIndex`, `currentWallpaper`): `matugenDots`, `colorCategories`, `resizeEnabled`, `resizeHeight`, `resizeFps`, `lockscreen`, `transitionsEnabled`, `transitionType`, `transitionDuration`, `transitionFps`, `transitionStep`, `transitionAngle`, `transitionPos`, `transitionInvertY`, `transitionBezier`, `transitionWave`.
- Se **elimina** el snippet `wallpaperScript` (JS de `evaluateScript`) de `AppController::setWallpaper`: código muerto tras P4 (cutover limpio). `imageResolution()` (QImageReader, local) se **conserva**: alimenta la propiedad `selectedWallpaperSize` (la etiqueta de resolución de `WallpapersPage.qml`).

## 4. Fases

Cada fase se verifica con: build limpio completo (`./build.sh clean` → `cmake --preset default` → `cmake --build --preset default`, **cero warnings**, salida completa) → `ctest --test-dir build` → UI en vivo vía kwin (pantallas = prueba).

### P1 — Caché de medios + vídeos en la rejilla  ✅ implementada · conforme (R8 resuelto a favor del código; ver D16)

**Objetivo:** JSON de información por wallpaper + primer frame + vídeos visibles y reproducibles en la rejilla.

C++:

- `src/wallpapersinfo.{h,cpp}` — `WallpapersInfo : QObject` (QML_SINGLETON, `initialize(QObject*)` en `main.cpp`, `static instance()`):
  - Raíz `~/.cache/kwal/`, subdirs `video_frames/`, `video_resizes/` (P5), `thumbnails/large/` (caché FDO propia de la app — D16/R8); `wallpapers_info.json`.
  - `struct Entry { QString file; QString type; QString resized; QString firstFrame; QStringList sourceColorIndex; QStringList colorCategories; QStringList tags; double lastModified = 0; bool matugenDone = false; bool categoriesManual = false; bool tagsManual = false; }` (claves snake_case en JSON; `type` = `"image"|"video"`; los tres flags de estado se persisten como claves opcionales, R6).
  - `load()` al init (falta/corrupto → vacío, reescribir); `save()` atómico (tmp + `QFile::rename`) tras cada cambio.
  - `Q_INVOKABLE void refreshFile(path, type)` — encola en `QThreadPool` (máx. 2): hash MD5 (chunks 1 MiB, `QCryptographicHash`) → entrada nueva si `lastModified` difiere → si vídeo y falta `first_frame`:
    `ffmpeg -v error -y -i <video> -map 0:v:0 -frames:v 1 -c:v png -f image2 -update 1 -an <video_frames/md5.png>` (tmp + rename; skip si existe no-vacío).
  - `Q_INVOKABLE QString firstFrameFor(path) const`, `sourceColorsFor`, `colorCategoriesFor`, `tagsFor`, `matugenAvailable()`, `ffmpegAvailable()`.
  - Señales: `entryChanged(QString path)`, `scanFinished()`.
**Formato de `~/.cache/kwal/wallpapers_info.json`** (canónico, R6; verificado `wallpapersinfo.cpp:372-449`) — top level: `{ "version": N, "files": [ …entries… ] }` (el índice es un **array de entradas**, no un mapa `md5 → entry`; el md5 clave aplica solo a los artefactos de caché — D1); todos los campos de la entrada presentes (cadena vacía para N/A); `last_modified` = epoch en segundos; 3 claves opcionales de estado, presentes solo si son true: `matugen_done`, `categories_manual`, `tags_manual`:

```json
{
  "version": 1,
  "files": [
    {
      "file": "/home/iguruspain/Vídeos/wallpapers/wave.mp4",
      "type": "video",
      "resized": "",
      "first_frame": "/home/iguruspain/.cache/kwal/video_frames/3a7bd3e2360a3d29eea436fcfb7e44c0.png",
      "source_color_index": ["#2b5876", "#4e4376", "#8e6ea8", "#f5f7fa"],
      "color_categories": ["blue-cyan", "violet", "blue", "white", "black"],
      "tags": ["mar", "nocturno"],
      "last_modified": 1729564800,
      "matugen_done": true,
      "tags_manual": true
    },
    {
      "file": "/home/iguruspain/Imágenes/wallpapers/mountains.png",
      "type": "image",
      "resized": "",
      "first_frame": "",
      "source_color_index": ["#1c2b3a", "#44607e", "#8899a6", "#d6dde2"],
      "color_categories": ["blue-cyan", "blue", "gray", "white", "black"],
      "tags": ["montañas"],
      "last_modified": 1729478400
    }
  ]
}
```

- `WallpaperModel`:
  - `videoExtensions()`: mp4, webm, mkv, mov, m4v, avi; rol nuevo `IsVideoRole` (conservar `ExtensionRole`).
  - Miniatura (D16, diseño bendecido): imagen → `ThumbnailRole` = `image://fdo_thumbnail/<ruta>`. Vídeo → `""` al escanear (QML: icono genérico, sin petición al proveedor); al llegar `first_frame` (`entryChanged`) → `dataChanged` con valor = `image://fdo_thumbnail/<frame>` (el frame es un PNG que el proveedor decodifica como imagen — **sin puente**). Verificado `wallpapermodel.cpp:248-309`.
  - Señal `scanned()` → `AppController` llama `refreshFile` a cada entrada.
  - `Q_INVOKABLE QStringList filePaths() const` para el AppController.
- `src/fdothumbnailprovider.cpp` (D16): **sin cambios** — decodificación directa del PNG (imagen o frame), caché en `~/.cache/kwal/thumbnails/large/<md5(URI percent-encoded)>.png` (directorio propio de la app, no la caché del sistema — carpeta confirmada como la adecuada, 2026-10-22).
- `src/main.cpp`: `WallpapersInfo::initialize(&app)`; `find_package(Qt6 COMPONENTS Multimedia)` + enlazar (QML importa `QtMultimedia`).
- `qml/WallpaperItem.qml`: `import QtMultimedia`; par `MediaPlayer`+`VideoOutput` (muted, `loops: true`) visible solo cuando `isVideo && root.isSelected`; `stop()` al deseleccionar (liberar decoder).
- `qml/WallpapersPage.qml`: `onScanned` → `refreshFile` por fila.

**Fuera de alcance:** tags/categorías (P2), drawer (P3), apply de vídeo (P4).

**Verificación:** kwin con `~/Imágenes/wallpapers` (imágenes) y `$HOME/Vídeos/wallpapers` (vídeos): frames PNG creados en `~/.cache/kwal/video_frames/`, JSON escrito (array versionado — R6), rejilla muestra miniaturas de vídeo; `~/.cache/kwal/thumbnails/large/` contiene `<md5(URI del frame)>.png` (el frame, no el vídeo, es la clave de la caché — D16); al seleccionar una celda de vídeo se ve reproducción en vivo; al deseleccionar para. `tests/tst_wallpapersinfo.cpp` (nuevo): round-trip JSON (formato de R6), MD5 de fixture conocido, primer frame sobre un clip sintetizado con `ffmpeg lavfi` (condicionado a ffmpeg), sandbox `HOME`/`XDG_*` como en `tst_appcontroller`.

### P2 — Colores matugen, categorías y búsqueda  ✅ implementada · ✅ reconciliada (R1–R5, R9 y R10 hechos; R7 → P4)

**Objetivo:** `source_color_index` + `color_categories` en el JSON; búsqueda por tag y categoría; página de settings de wallpapers.

C++:

- `WallpapersInfo` (extiende el pipeline de P1) para cada entrada nueva/cambiada:
  - Si `matugenEnabled()` (D14): `QProcess` `matugen image <ruta> --show-source-colors` (vídeo → ruta del `first_frame`); parsear `#[0-9a-fA-F]{6}` de stdout, pasar a minúsculas, dedup preservando orden de salida, sin tope (D3); fallo → `[]` → `source_color_index`.
  - Siempre (imagen, o vídeo con `first_frame`; vídeo sin frame → se deja vacío y se reintenta en el próximo escaneo): `QImage` 128×128 (alfa>128) → mcu `QuantizeCelebi(pixels, 128)` → `RankedSuggestions{desired=8, filter=true}` → `colorCategory` (D4) de cada color → conjunto ordenado → `color_categories` (D5).
  - Persistir, `entryChanged`.
- `src/colorcategory.{h,cpp}` (D4): `QString colorCategory(uint32_t argb)` (usa `mcu::Hct`) + `const QStringList &kCategoryList()` (15 nombres).
- `CMakeLists.txt` (P2): bloque `FetchContent` de `https://github.com/material-foundation/material-color-utilities` fijado al commit `bd811e4ca2ab94c4ce3cadc89c19b1820b9704b3` (sin tags); target estático `mcu` con todos los `.cc` de `cpp/` menos `*_test.cc`; `target_include_directories(mcu PUBLIC <raíz del fetch>)` (los includes son `#include "cpp/..."`); enlazar `mcu` al target principal. P3 (paletas) lo reutiliza.
- `Q_INVOKABLE void setMatugenEnabled(bool)` (lo fija `AppController` desde KConfig).
- `WallpaperModel`: `setCategoryFilter(const QStringList&)` (AND, D8) + texto sobre nombre o tag (D8); tags leídos de `WallpapersInfo` al filtrar.
- `AppController`: propiedad `matugenDotsEnabled` (KConfig `[Wallpapers] matugenDots`, default true si matugenAvailable) con load/save; expone `matugenAvailable`. Propiedad `colorCategoriesEnabled` (KConfig `[Wallpapers] colorCategories`, **default true siempre** — no depende de matugen, D5/R9).

QML:

- `qml/WallpapersPage.qml`: al `searchOpen`, fila de **chips de categoría** bajo el `SearchField` (15 `Kirigami.Chip`, uno por categoría de D4, con el swatch hex de D4; chip "Limpiar"); visible siempre (las categorías existen aunque matugen no esté — D5); toggle → `model.setCategoryFilter`.
- `qml/WallpapersSettingsPage.qml` (nueva; reemplaza placeholder en `SettingsWindow.qml`): Switch "Mostrar puntos de color (matugen)" (gated por matugen — los dots son de matugen, D3); Switch "Categorías de color" (**siempre activo**, default true — R9); `searchKeys` para la búsqueda de settings. El switch de lockscreen **se añade en P4** (D15, ex-R7). (El grupo de resize llega en P5 y el de transiciones en P6.)

**Verificación:** JSON contiene `source_color_index` (con matugen) y `color_categories` (con **y** sin matugen) en entradas nuevas; búsqueda: tag en el texto filtra, chip de categoría filtra (AND), "Limpiar" resetea; los toggles persisten en `kwalrc`. kwin: capturas de la fila de chips con resultados filtrados. Extender `tst_wallpapersinfo.cpp`: tabla del clasificador D4 (hex → categoría, incluyendo los umbrales de sector y el orden black/white/gray), semántica de filtros del modelo.

### P3 — Drawer de detalle (doble clic)  ✅ implementada · ✅ reconciliada (R4 hecha: paleta mcu, 16 swatches)

**Objetivo:** `WallpaperDetailDrawer`: imagen ampliada animada (slide del drawer), categorías/tags editables (chips), `matugenRow` (dots seleccionables), paletas Material You + Imagemagick.

  - `qml/WallpaperDetailDrawer.qml` (nuevo): `Kirigami.OverlayDrawer` (lado derecho, ~40% del ancho de la ventana; abrir al doble clic — API verificada en Kirigami 6.31 instalado: `drawerOpen`, `side`).
  - **Comportamiento (aprendido en la implementación; conservar):**
    - **Modal** (`modal: true`; `98d036d`, cambio posterior a este plan — originalmente no-modal): la rejilla de fondo no es operable mientras está abierto. Cierre por X o Escape (`closePolicy: Popup.CloseOnEscape`); un clic fuera **no** lo cierra.
    - `drawerOpen` fijado a `false` (la plantilla interna lo liga a `!modal`, que daría true).
    - `handleVisible: true` (`98d036d`; el bug de `TypeError` al arrancar en no-modal se evita con la instanciación perezosa del punto siguiente).
    - La página lo instancia de forma **perezosa** (`Loader` activo solo al primer doble clic): el drawer nunca existe antes de que el overlay de la ventana esté adjunto.
  - Área media: capa base siempre visible — imagen `file://` (`PreserveAspectFit`) o **primer frame** del vídeo — sobre la que arranca la reproducción en vivo. Vídeo: `VideoOutput`+`MediaPlayer` (muted, loop) con el mismo patrón *armed* que las celdas de la rejilla: un `Timer` de 250 ms arma la reproducción mientras el drawer está abierto; el `MediaPlayer` se **destruye** (no se pausa) al cerrar o al cambiar de vídeo, para que la celda de la rejilla pueda reanudar su bucle.
  - **Categorías:** `Kirigami.Chip` (verificado `closable` → `removed()`) + botón de reset (ícono `view-refresh`) + chip "Añadir" con `Menu` de las 15 categorías no presentes (**sin cap**, D13) → `WallpapersInfo.setCategories`. El reset (añadido 2026-10-22, petición del usuario) llama a `WallpapersInfo::recalculateCategories(path)`: recalcula las categorías automáticas desde la imagen (o `first_frame`) **asíncrono** y limpia `categories_manual` (sin tocar tags ni dots); si la imagen no decodifica, la entrada queda intacta.
  - **Tags:** chips + `TextField` inline para añadir (Enter o botón).
  - **Colores matugen:** `RowLayout` de `ColorDot` (componente existente) sobre `sourceColorsFor` (variable, normalmente 3–4); tap → seleccionado (borde, como `rightPane` viejo).
  - **Paletas:** `ComboBox` [Material You, Imagemagick] + `ComboBox` de scheme M3 (los 9 de D6, solo Material You, dimensionada a su contenido) + botón "Extraer" + Switch dark/light (solo Material You) + rejilla de 16 swatches (Material You: la lista M3 de D6; Imagemagick: ≤16 colores + accent); tap → seleccionar, right-click → copiar hex. Cambiar el scheme **re-pide** la paleta sin vaciar la rejilla: el extractor cachea los 9 esquemas por wallpaper + dark mode (pasada única de Celebi), así que responde en el acto sin re-extraer (2026-10-22, petición del usuario).
  - Botón "Fijar como wallpaper" → `app.applyWallpaper()`.
- `src/paletteextractor.{h,cpp}` — `PaletteExtractor : QObject` (poseído por `AppController`, `QThreadPool`):
  - `Q_INVOKABLE void extract(path, backend, dark, scheme = "TonalSpot")` (`scheme` = uno de los 9 esquemas M3 de D6); señal `paletteReady(path, colors, accents, seed)` (`colors` = lista de hex; Material You = los 16 swatches de D6; Imagemagick = colores + accent).
  - Imagemagick (D7).
  - Material You (D6): pipeline con la librería oficial (`QuantizeCelebi` → `RankedSuggestions` → el `SchemeX` pedido), todo en la worker thread; el paso caro (Celebi) se ejecuta **una vez** por wallpaper + dark mode y los 9 esquemas quedan **cacheados en memoria** (`QHash` con `QMutex`, clave `path|dark`/`path|light`) — un `extract()` repetido (cambio de scheme) responde de forma síncrona desde la caché (2026-10-22, petición del usuario); si el pixmap no carga → `paletteReady` vacío (el drawer muestra "no disponible").
  - Vídeo → extraer sobre el `first_frame`.
  - El target `mcu` ya se integra en P2 (ver su `CMakeLists.txt`); aquí solo se enlaza para la paleta.
- `qml/WallpaperItem.qml`: `MouseArea` en `contentItem` con `onDoubleClicked` → callback `onOpenDetail(path)` (el `onClicked` del `ItemDelegate` se mantiene para selección; el `MouseArea` no roba focus — la navegación por teclado de `GridView` no se afecta). Guarda `detailOpen`: el vídeo de la celda no se arma mientras el drawer está abierto (evita dos decodificadores del mismo archivo).

**Verificación:** kwin: doble clic abre el drawer (captura animación cerrada/abierto); chips editables persisten (`entryChanged` actualiza drawer y JSON); `matugenRow` muestra los dots de `source_color_index` (variable, normalmente 3–4) con selección; paleta Material You dark+light y Imagemagick sobre imagen **y** vídeo; apply desde el drawer. Flujo de vídeo 1 (cierre): doble clic → la celda para → primer frame en el drawer → ~250 ms → reproducción; X/Escape → la reproducción del drawer se destruye → la celda reanuda el bucle. Flujo 2 (cambio): cerrar (X/Escape) y doble clic en otro vídeo → primer frame del nuevo → ~250 ms → reproducción del nuevo (con el drawer modal no existe el cambio in situ — `98d036d`). Log sin `TypeError` en arranque, apertura, cierre y cambio de vídeo. Test `tst_paletteextractor`: paleta Material You determinista — fixture de pixmap sintetizado en el test (ej. 128×128 con dos bloques de color), seed esperado y assert de hex de `GetBackground`/`GetPrimaryContainer` (valores de referencia calculados contra la librería al escribir el test). Caché de schemes (2026-10-22): `testSchemeSwitchIsCached` — una segunda `extract()` con otro scheme emite de forma síncrona (antes de que `extract()` regrese), sin re-extraer.

### P4 — Nueva aplicación de wallpaper (image + video)  ✅ implementada (2026-10-22)

**Objetivo:** reemplazar el apply por `evaluateScript` con D-Bus `setWallpaper` + fallback `appletsrc`, para imagen y vídeo (*reborn*).

- `src/plasmaapply.{h,cpp}`:
  - `QString fileUrl(path)` — `file://` + percent-encoding (preserva `A-Za-z0-9-_.~/`), como el ref. Rust.
  - `QString rawVideoUrl(path)` — `file://` + ruta, **sin** encoding (D9).
  - `QString computeVideoUrls(existingJson, targetUrl)` — merge (D10).
  - Editor `Ini` lineal (patrón del `lib.rs` Rust: `get_key(parts,key)`, `set_key` crea la sección al final, preserva bytes del resto; escritura atómica) para `~/.config/plasma-org.kde.plasma-desktop-appletsrc` **y** `~/.config/kscreenlockerrc` (mismo editor, D15).
  - `desktopIds()` — vivos por `evaluateScript` (`desktops[i].formFactor == 0`); fallback: secciones `[Containments][N]` con `formfactor=0`; `lastScreen` del appletsrc (default 0).
  - `applyWallpaper(type, path)`:
    - Imagen → plugin `org.kde.image`, `{Image: fileUrl}`.
    - Vídeo → si `rebornAvailable()` (D11; dirs `~/.local/share/plasma/wallpapers` y `/usr/share/plasma/wallpapers`): plugin `luisbocanegra.smart.video.wallpaper.reborn`, `{VideoUrls, LastVideo: rawVideoUrl, LastVideoPosition: 0}`; si no → **aplicar el `first_frame` PNG como imagen** (`org.kde.image`, `{Image: fileUrl(frame)}`); sin frame → fallo con mensaje.
    - Por desktop: D-Bus `org.kde.plasmashell / org.kde.plasma.wallpapers.<id> / org.kde.plasma.wallpapers / setWallpaper(QString, QVariantMap, int)`; fallos → fallback archivo (releer appletsrc si algún desktop fue por D-Bus, D9).
    - Señal `result(bool allOk, QString message)`.
    - **Lockscreen** (D15): si la config `lockscreen` está activada → escribir `kscreenlockerrc`: `[Greeter] WallpaperPlugin=<plugin>` + `[Greeter][Wallpaper][<plugin>][General]` con `Image=fileUrl` (imagen) o `VideoUrls`/`LastVideo=rawVideoUrl`/`LastVideoPosition=0` (vídeo); un único fondo para todas las pantallas.
- `AppController`: `setWallpaper` enruta por tipo (`WallpaperModel::isVideo`, extensión) y delega en `PlasmaApply` (P5: prefiere la copia `resized` si existe); **eliminar** `wallpaperScript()` (JS muerto tras el cutover); `imageResolution()` se **conserva** (QImageReader local; alimenta `selectedWallpaperSize`, etiqueta de resolución de `WallpapersPage.qml`); `applyWallpaper()` conserva la misma semántica para QML (aplica la selección, actualiza `currentWallpaper`).
- QML sin cambios de contrato: botón "Aplicar" de la rejilla, tecla Return, botón del drawer → `app.applyWallpaper()`. `AppController`: propiedad `lockscreen` (KConfig `[Wallpapers] lockscreen`, default false) + Switch "Aplicar al lockscreen" en `WallpapersSettingsPage` — **se añade aquí** (D15, ex-R7; no se crea un setting muerto en P2).

**Verificación:** tests puros `tests/tst_plasmaapply.cpp`: vectores de `fileUrl` (espacio → `%20`, UTF-8 → `%C3%A1`, chars preservados), `computeVideoUrls` (añadir/deshabilitar/conservar claves), `Ini` (get/set/crear sección/preservar), `desktopIds` sobre fixture de appletsrc. kwin: aplicar imagen (pantalla de fondo cambia — captura antes/después); aplicar **vídeo** (reborn instalado): captura con el vídeo reproduciéndose en el escritorio; inspeccionar `appletsrc` (contenido de `VideoUrls` con target habilitado y otros deshabilitados). Lockscreen: toggle on → tras aplicar imagen, `kscreenlockerrc` contiene `[Greeter] WallpaperPlugin=org.kde.image` y la clave `Image` (y tras aplicar vídeo, `VideoUrls`/`LastVideo`); toggle off → el fichero queda intacto.

### P5 — Resize de vídeo  ✅ implementada (2026-10-22) · ✅ **on-demand** (revisión 2026-10-23: generación explícita, no auto)

**Objetivo:** redimensionado opcional de vídeos antes de aplicar (ffmpeg), configurable.

- `WallpapersInfo` (extiende):
  - Settings: `resizeEnabled` (default false), `resizeHeight` (default 1080), `resizeFps` (default 0 = original) — KConfig vía `AppController`.
  - Planificación por vídeo (`processResize`, al generar on-demand):
    - `ffprobe -v error -select_streams v:0 -show_entries stream=width,height,avg_frame_rate:format=duration -of json <video>` → srcH, srcFps.
    - Normalización (como el ref. Rust): `targetH = min(resizeHeight, srcH) & ~1`; `scale = targetH < (srcH & ~1)`; `fpsFilter = resizeFps>0 && resizeFps < srcFps - 0.01`; sin scale ni fps → `resized = ""` (se aplica el original).
    - Salida `video_resizes/<md5>_<targetH>p<fpsToken>.mp4` (`fpsToken` = fpsFilter o `round(srcFps)`); skip si existe no-vacío.
    - `ffmpeg -v error -y -progress pipe:1 -nostats -i <video> -map 0:v:0 -an -sn -dn -vf scale=-2:<H>:flags=lanczos[,fps=<f>] -c:v libx264 -preset veryfast -crf 23 -pix_fmt yuv420p -movflags +faststart -f mp4 <tmp>` (tmp + rename).
    - Guardar `resized`, `entryChanged`.
  - **Generación on-demand (revisión 2026-10-23):** el cambio de settings **ya no** replanea. La generación es **explícita**: `AppController::videoFolders` (carpetas configuradas con ≥1 vídeo, rutas completas) alimenta el picker; el botón "Generate resizes" → `generateVideoResizes(folder)` → `setResizeParameters(height, fps)` + `beginResizeBatch(n)` + `planResize(path)` por vídeo (batch con progreso `resizing`/`resizeTotal`/`resizeCompleted`; `processResize` re-intenta las entradas ausentes/no-vídeo sin romper el batch). Las salidas con nombre distinto al objetivo quedan huérfanas — **D12/P7** (purga al `sweep()`).
- Objetivo de aplicación (en `AppController::setWallpaper`, sobre `PlasmaApply` de P4): para vídeo con `resizeEnabled`, usar la copia `resized` si existe en disco, else original (mismas URLs de D9/D10 sobre la copia de caché).
- `qml/WallpapersSettingsPage.qml`: grupo "Redimensionado": Switch "Resize videos" (aplica la copia `resized` en vez del original) + `SpinBox` altura (480–4320, step 120) + `SpinBox` FPS (0–240, "0 = original") + `ComboBox` de carpetas con vídeo (`videoFolders`, rutas completas) + botón "Generate resizes" + `ProgressBar` de batch.
- Sin ffmpeg → `resized=""` + warning único (comportamiento del ref. Rust).

**Verificación:** tests (en `tst_wallpapersinfo.cpp`): planificación (srcH<altura → no-op; fps menor → filtro) con ffprobe mock/fijo; kwin: activar resize → archivo creado en `video_resizes/` (verificar con `ffprobe` que altura/fps coinciden) → aplicar vídeo → `VideoUrls` apunta a la copia de caché; desactivar → aplicar usa el original.

### P6 — Transiciones  ⏳ pendiente

**Objetivo:** transiciones de wallpaper según `plan-implementacion-transiciones-plasma.md`.

- **Implementar los hitos H0–H5 de ese documento** (§9), con sus contratos (§4), API (§5), concurrencia (§6) y criterios de aceptación (§9). No se duplica aquí su contenido; este plan solo añade la integración con el app:
  - `H0` (spike de integración LayerShell con ventana normal en este KWin) = **puerta go/no-go**: si falla el criterio del documento, seguir su plan B (proceso auxiliar) con sus reglas de decisión.
  - Nuevo árbol `src/transitions/` (effects, transitionspec, wallpaperswitcher, plasmacomp, screenmap, overlay LayerShell).
  - `CMakeLists.txt`: `find_package(LayerShellQt 6 REQUIRED CONFIG)` → enlazar `LayerShellQt::Interface` (target verificado en `/usr/lib/cmake/LayerShellQt/`); `Qt6::Concurrent` (decodificar/escalar off-thread).
  - Cabeceras MIT + regla clean-room según §11 del documento (D17).
  - **Lockscreen:** `SwitchOptions.includeLockscreen` = la config `lockscreen` (D15); el camino de lockscreen del switcher sigue el documento (línea 374: `kscreenlockerrc`, único fondo para todas las pantallas).
- Integración con el app:
  - `AppController::setWallpaper` (P4): si `transitionsEnabled` y el overlay está disponible → `WallpaperSwitcher` (Cover/Reveal según §4.4 del doc); si no, apply directo (P4) sin transición.
  - `qml/WallpapersSettingsPage.qml`: grupo "Transiciones": Switch (default off, D14), tipo (`none, simple, fade, left, right, top, bottom, wipe, wave, grow, center, outer, random`), duración (0.1–10 s, def 0.9), FPS (0=auto), step simple (1–16), ángulo (−360–360, def 45), pos (`center, top, bottom, left, right, corners`), invertY, bezier (def `.54,0,.34,.99`), wave W×H (def `20,20`) — claves KConfig de §3.
- El parámetro `includeLockscreen` del §4.5 del doc sale de la misma config `lockscreen` (D15).

**Verificación:** los criterios de aceptación de §9 del documento (H1: match de vectores de efectos contra tabla de referencia; H3: overlay por encima de Plasma; H5: aplicación completa) + kwin: transición visible al cambiar de wallpaper (imagen y vídeo — portada del vídeo = `first_frame` de P1), captura antes/durante/después.

### P7 — Limpieza de huérfanos (D12)  ⏳ pendiente (extiende `sweep()` existente)

**Objetivo:** no dejar entradas JSON, ficheros de caché ni entradas de playlist *reborn* de archivos que ya no existen.

- **Nota de auditoría (2026-10-22):** `WallpapersInfo::sweep()` **ya existe y funciona** (`wallpapersinfo.cpp:303-344`): tras cada escaneo de folder borra las entradas JSON cuyo `file` no existe en disco y los frames huérfanos de `video_frames/` (referencia directa por ruta — el índice es el array de R6, no un mapa md5). P7 la **extiende**, no la reescribe:
  - `sweep()` también **al arrancar** (tras `WallpapersInfo::load()`).
  - Añadir la purga de `video_resizes/` (criterio igual: fichero sin entrada que lo reference vía `resized`).
  - Guardar el JSON si hubo cambios; señal `purged()`. **Fuera:** `~/.cache/kwal/thumbnails/large/` (thumbnails tal cual — carpeta propia de la app confirmada como la adecuada, 2026-10-22; los huérfanos son pequeños y se regeneran al pedirlos).
- `PlasmaApply` (extiende P4):
  - `QString pruneVideoUrls(const QString &existingJson) const` — pura: quita las entradas de `VideoUrls` cuya URL `file://` es un fichero local inexistente en disco; conserva el resto (incluidas claves no `file://`); devuelve el JSON nuevo (o el original sin cambios).
  - `Q_INVOKABLE void pruneDeadVideoUrls()` — lee `appletsrc`, aplica `pruneVideoUrls` a las secciones con `VideoUrls` (editor lineal de P4) y escribe solo si cambió.
- `AppController`: al arrancar (tras `WallpapersInfo::load()`) y en cada `scanFinished` → `sweep()` → en `purged()` → `pruneDeadVideoUrls()`.

**Verificación:** tests en `tst_wallpapersinfo.cpp`: fixture JSON (una entrada viva + una con ruta inexistente) + `video_frames/` con un PNG huérfano y uno correspondiente (y `video_resizes/` con su huérfano, si resize está activo) → `sweep()` elimina la entrada muerta y los ficheros huérfanos y conserva los vivos. `tst_plasmaapply.cpp`: vectores de `pruneVideoUrls` (URL muerta sale, la viva queda, claves no `file://` se conservan, sin cambios → misma cadena). kwin: con el app abierto borrar un vídeo del folder → arrancar de nuevo → la entrada desaparece de la rejilla y del JSON, el PNG del frame se borra y `appletsrc` ya no contiene la URL muerta.

## 5. Orden y dependencias

```mermaid
flowchart LR
    P1[P1 Caché + vídeos en rejilla] --> P2[P2 Colores + búsqueda]
    P2 --> P3[P3 Drawer de detalle]
    P1 --> P4[P4 Nueva aplicación]
    P1 --> P7[P7 Limpieza de huérfanos]
    P4 --> P5[P5 Resize de vídeo]
    P4 --> P6[P6 Transiciones]
    P4 --> P7
    P5 --> P7
```

P4 solo depende de P1 (detección de vídeo del modelo). P3 y P4 son independientes entre sí. P6 consume `first_frame` (P1) y el apply (P4). P7 depende de P1 (`WallpapersInfo`) y P4 (editor de `appletsrc`); se ejecuta tras todas las fases para que la purga cubra también `video_resizes` (P5).

## 6. Verificación global (al finalizar)

- Build limpio completo con cero warnings (política del repo) + `ctest`.
- Verificación de UI: **la hace el usuario** en su escritorio real (capturas a su gusto); el agente NO usa kwin MCP para inspeccionar la app (el usuario prefiere probarlo él, y la sesión aislada no tiene `plasmashell`/`plasmawallpapers` de todos modos). El agente entrega los pasos exactos a comprobar: rejilla (frames, reproducción al seleccionar) → doble clic (drawer completo) → búsqueda → apply de imagen y de vídeo → resize on/off → transición → borrar un vídeo del folder y arrancar de nuevo (huérfanos limpiados: JSON, frame, playlist) → settings persisten en `kwalrc`.
- Reconciliación (§2b): informe de auditoría R1–R5, R9, R10 con evidencia por punto y todos los puntos en OK antes de empezar P4 (R6/R7/R8 ya resueltos por este plan).
- `AGENTS.md`: actualizar la sección *Architecture* y *config groups* con los nuevos grupos/claves y componentes al completar cada fase.

## 7. No metas (fuera de alcance)

- Port del resto de features del app viejo (filtros de carpetas, thumbnails FreeDesktop propios, paletas Color Thief/wal, tinting de tools — ya existe).
- `no_disable` de la playlist (D10), soporte de SVG/video en thumbs FDO, purga de `~/.cache/kwal/thumbnails/large/` (los thumbnails quedan tal cual — D12).
- Nada de `diseñando.txt` más allá de lo listado (video/SVG de wallpapers como carpetas, etc., no está en este plan).
