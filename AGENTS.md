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

## Tooling rules (mandatory)

1. **Skills first:** read the relevant skill before writing code (`qt-qml`, `qt-cmake-project`, `Kirigami`). Follow them.
2. **Installed module is the source of truth, not memory or master docs.** Before using any Kirigami type/property, verify it exists in this system's build:
   - QML types/properties: `grep` `/usr/lib/qt6/qml/org/kde/kirigami/{templates,controls,dialogs,forms,primitives,layouts}/{qmldir,*.qmltypes}` or the `.qml` sources.
   - LSP: `xd://lsp` diagnostics/hover on the file (clangd + qmlls6 configured in `.omp/lsp.json`).
   - Docs: `qt_documentation_search` MCP (Qt 6.12) for Qt APIs; api.kde.org / develop.kde.org for Kirigami.
3. **After every edit** of `.qml`/`.cpp`: run `xd://lsp` `diagnostics` on the file (per-file; workspace-wide `file:"*"` fails — project type not detected); fix all warnings before calling it done.
4. **Verify UI empirically:** kwin MCP — `session_start`, then `launch_app` with `command:` (session_start does not launch the app), `screenshot` + `window_geometry`, then `session_stop`. Screenshots are the proof.
5. Do not assume APIs from other Kirigami versions. If a pattern is not verifiable locally, say so instead of guessing.

## Architecture

- Single executable `kwal` + QML module `org.kde.kwal`; `main.cpp` loads `Main` via `loadFromModule`. On KDE desktops it sets `QT_QPA_PLATFORMTHEME=kde` and `QT_QUICK_CONTROLS_STYLE=org.kde.desktop` (never overriding explicit user settings).
- `AppController` (`QML_ELEMENT`, instantiated in `Main.qml`) is the app state/services: folders, wallpaper selection, config, and applying the wallpaper by running a Plasma JS snippet via D-Bus `org.kde.plasmashell` → `evaluateScript`.
- `WallpaperModel` (QAbstractListModel over the current folder) + `FdoThumbnailProvider` (QQuickImageProvider `fdo_thumbnail`) feed the grid.
- Two windows: `Main` (tabs: Wallpapers live, Fastfetch/Ulauncher placeholders) and a lazily-created `SettingsWindow` from the header menu.
- Settings via KConfig (`kwalrc`); identity: `OrganizationName "KDE"`, domain `kde.org`. Groups:
  - `[Wallpapers]`: `folders`, `currentFolderIndex`, `currentWallpaper(+Size)`.
  - `[General]`: `windowWidth`/`windowHeight` — applied **at startup only** (window default 960×650, clamped to 700×400–3840×2160); no live resize. `transparency` — window background opacity.
- QML: `qsTr()` for all user strings, `Kirigami.Units` for spacing/sizing, semantic `Kirigami.Theme` colors, Kirigami semantic components over raw Controls.
- Install (ECM): desktop file (→ `kwal.desktop`) + hicolor icon; the executable itself is not installed yet.
