# Repository Guidelines

## Project

Kwal — KDE Plasma utility to manage the wallpaper and tint applications/tools with palettes extracted from the wallpaper. C++/Qt6/QML rewrite of the Python original (sibling repo `../Kwal` — read its AGENTS.md for feature spec). App ID: `org.kde.kwal`.

Future design notes (thumbnail cache, video/SVG support, palettes) live in `diseñando.txt` — roadmap only, do not nothing with this, not a spec for current work.

## Development Directives

- **Simplicity first.** The rewrite keeps the new project as simple and clean as possible: smallest code that does the job, no speculative abstractions, no legacy quirks.
- **No legacy features by default.** The old app included: wallpaper management (folders, thumbnails, filters, video/SVG support), palette extraction (Material You / Color Thief / wal), tool tinting (Fastfetch, Starship, Ulauncher), live preview with shaders. **Before implementing any of these, ask the user which ones to keep** — never port unasked.
- **Standards over precedent.** Follow KDE HIG + Kirigami skills + current Qt best practices. Where the old app did something differently (threading, file watching, config format), the new code follows current standards — the old implementation is a feature reference, not a pattern reference.
- **Minimal dependencies.** System packages only; no new third-party library without explicit user approval.

## Stack (exact versions on this system)

- Qt 6.12.0 (system packages, pkg-config + CMake)
- Kirigami 6.31.0 / KDE Frameworks 6.31 (`KF6::Kirigami`, `KF6::ConfigCore`)
- CMake 4.x + Ninja, GCC 16
- **Qt5/Qt6 coexist:** `/usr/bin/qml` and `/usr/bin/qmllint` are **Qt 5.15**. Use `qml6` and `/usr/lib/qt6/bin/qmllint` (or the app binary directly).

## Commands

```sh
./build.sh             # helper: [build|test|run|config|clean], auto-configures on first run
cmake --preset default     # configure (Ninja, Debug, compile_commands.json)
cmake --build --preset default
ctest --test-dir build     # run tests (QtTest + CTest)
./build/kwal               # run (or via kwin MCP for headless UI verification)
```

Tests sandbox the user dirs: `tst_appcontroller` sets `HOME`/`XDG_*` to a temp dir *before* constructing `QCoreApplication` so KConfig and QStandardPaths never touch the real home. New tests must keep this.
## Build assurance (mandatory)

- **Clean build is the only proof.** Never declare a build healthy from an incremental compile on a stale tree: configure warnings and compiler warnings only surface on a fresh configure + full recompile. Verification = `./build.sh clean` → `cmake --preset default` → `cmake --build --preset default` → **zero errors, zero warnings** → `ctest`.
- **Never filter or truncate build output** (`tail`, `grep` on the log) when verifying — a filtered log is not a verification; read the full output.
- **Zero-warning policy.** Every compiler, AutoMoc, and CMake warning must be fixed at the source — no suppressions, no pragmas, no "pre-existing" excuses. A warning found while working (even in code you did not touch) is fixed in the same task.

## Working method (mandatory)

- **Evidence before claims.** Never assert how code or tooling behaves from memory or general knowledge — read the installed headers/packages/sources that actually govern the behavior. The installed file is the proof.
- **Root cause, not symptom.** When an error or warning appears, identify its cause (read the relevant file) before fixing it. Silencing, suppressing, or special-casing without understanding the cause is forbidden.
- **Full output is the proof.** Verification means reading the complete unfiltered output (see Build assurance), not a filtered count.
- **Honest reporting.** Report what was done and what is proven; state explicitly what is unverified. Never hide, gloss over, or blame the user/environment without evidence.
- **An alive process is not a loaded UI.** A running PID is not proof the window exists. When the window fails to appear, the first diagnostic is the app's own stdout/stderr: QML load failures print there (`Cannot assign to non-existent property "onX"`, `Type <X> unavailable` → `Component unavailable`). A missing window is a QML load failure to investigate before the session or environment is suspected.

## Tooling rules (mandatory)

1. **Skills first:** read the relevant skill before writing code (`qt-qml`, `qt-cmake-project`, `Kirigami`). Follow them.
2. **Installed module is the source of truth, not memory or master docs.** Before using any QML type/property from any module (Kirigami, Qt, third-party), verify it exists in **this** build:
   - QML types/properties: `grep` the module's `plugins.qmltypes`/`qmldir` under `/usr/lib/qt6/qml/` (e.g. `org/kde/kirigami/…`, `org/qtquick/webengine/plugins.qmltypes`) or the `.qml` sources. Never from memory of another Qt version — this Qt 6.12 `QQuickWebEngineView` has no `loadFinished` signal (older Qt did); the load-completed equivalent is `loading`/`loadingChanged`.
   - LSP: `xd://lsp` diagnostics/hover on the file (clangd + qmlls6 configured in `.omp/lsp.json`).
   - Docs: `qt_documentation_search` MCP (Qt 6.12) for Qt APIs; api.kde.org / develop.kde.org for Kirigami.
3. **LSP is the priority tool** (clangd + qmlls6, configured in `.omp/lsp.json`) for both code intelligence and quality:
   - Code intelligence (definitions, references, hovers, renames) MUST use `xd://lsp`, never raw text search; cross-file renames only via LSP `rename`/`rename_file`.
   - After every edit of a `.cpp`/`.h`/`.qml` file: run `xd://lsp` `diagnostics` on it (per-file; workspace-wide `file:"*"` fails — project type not detected) and fix every diagnostic before calling the file done. **There is no minor warning**: a diagnostic — warning or error; qmllint's `no matching signal found for handler` is a hard defect, not a nit — is resolved in the same pass or proven a false positive against the installed module. A green build never outranks an unresolved diagnostic.
   - LSP complements the compiler — it catches what GCC misses (e.g. unused includes). A file is only clean when **both** the compiler and LSP report zero diagnostics.
4. **Verify UI empirically:** kwin MCP — `session_start`, then `launch_app` with `command:` (session_start does not launch the app), `screenshot` + `window_geometry`, then `session_stop`. Screenshots are the proof. The QML cache compiles fine even with a handler for a signal that does not exist; the failure surfaces only at runtime and takes down the whole component hierarchy from a single broken file. A green build is a weaker guarantee for `.qml` than for C++: any edit touching handlers/properties must be loaded live and the window confirmed.
5. Do not assume APIs from other Kirigami versions. If a pattern is not verifiable locally, say so instead of guessing.

## Architecture

- Single executable `kwal` + QML module `org.kde.kwal`; `main.cpp` loads `Main` via `loadFromModule`. On KDE desktops it sets `QT_QPA_PLATFORMTHEME=kde` and `QT_QUICK_CONTROLS_STYLE=org.kde.desktop` (never overriding explicit user settings).
- `AppController` (`QML_ELEMENT`, instantiated in `Main.qml`) is the app state/services: folders, wallpaper selection, config, and applying the wallpaper by running a Plasma JS snippet via D-Bus `org.kde.plasmashell` → `evaluateScript`.
- `WallpaperModel` (QAbstractListModel over the current folder) + `FdoThumbnailProvider` (QQuickImageProvider `fdo_thumbnail`) feed the grid.
- Tool tabs (Fastfetch, Starship, Ulauncher): a `QML_ELEMENT` controller each (`FastfetchController`, `StarshipController`, `UlauncherController`) backed by `src/tools/` (config read/write with `.bak` backup, Starship HTML terminal preview, Ulauncher theme + live QML mockup). `UlauncherController` is owned by `AppController` (constructor with the app's own KConfig `[Ulauncher]` group) and shared by the tab and the Ulauncher settings page, so preview-mockup settings (window width, typed query, sample rows) are app-local and persist in `kwalrc`. Bundled templates are compiled in and copied to `~/.config/kwal/templates` at startup (`tools::installTemplates`); a tab appears only when its binary is on `PATH`.
- Two windows: `Main` (tabs: Wallpapers, Fastfetch, Starship, Ulauncher) and a lazily-created `SettingsWindow` from the header menu. SettingsWindow pages: General and Ulauncher are implemented (Ulauncher: preview-mockup settings); Wallpapers, Fastfetch and Starship are placeholders.
- Settings via KConfig (`kwalrc`); identity: `OrganizationName "KDE"`, domain `kde.org`. Groups:
  - `[Wallpapers]`: `folders`, `currentFolderIndex`, `currentWallpaper(+Size)`.
  - `[General]`: `windowWidth`/`windowHeight` — applied **at startup only** (window default 960×650, clamped to 700×400–3840×2160); no live resize. `transparency` — window background opacity.
  - `[Ulauncher]`: `previewWindowWidth` (launcher width in the preview mockup, default 900, clamped 400–1200), `previewQuery` (text typed in the mockup input, default `sptf`), `sampleRows` (list of `{icon, name, shortcut, selected}` maps shown in the mockup; empty falls back to the built-in samples).
- QML: `qsTr()` for all user strings, `Kirigami.Units` for spacing/sizing, semantic `Kirigami.Theme` colors, Kirigami semantic components over raw Controls.
- Install (ECM): desktop file (→ `kwal.desktop`) + hicolor icon; the executable itself is not installed yet.
