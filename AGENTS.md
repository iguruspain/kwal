# Repository Guidelines

## Project

Kwal — KDE Plasma utility to manage the wallpaper and tint applications/tools with palettes extracted from the wallpaper. C++/Qt6/QML rewrite of the Python original (sibling repo `../Kwal` — read its AGENTS.md for feature spec). App ID: `org.kde.kwal`.

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
cmake --preset default     # configure (Ninja, Debug, compile_commands.json)
cmake --build --preset default
ctest --test-dir build     # run tests (QtTest + CTest)
./build/kwal               # run (or via kwin MCP for headless UI verification)
```

## Tooling rules (mandatory)

1. **Skills first:** read the relevant skill before writing code (`qt-qml`, `qt-cmake-project`, `Kirigami`). Follow them.
2. **Installed module is the source of truth, not memory or master docs.** Before using any Kirigami type/property, verify it exists in this system's build:
   - QML types/properties: `grep` `/usr/lib/qt6/qml/org/kde/kirigami/{templates,controls,dialogs,forms,primitives,layouts}/{qmldir,*.qmltypes}` or the `.qml` sources.
   - LSP: `xd://lsp` diagnostics/hover on the file (clangd + qmlls6 configured in `.omp/lsp.json`).
   - Docs: `qt_documentation_search` MCP (Qt 6.12) for Qt APIs; api.kde.org / develop.kde.org for Kirigami.
3. **After every edit** of `.qml`/`.cpp`: run `xd://lsp` `diagnostics` on the file; fix all warnings before calling it done.
4. **Verify UI empirically:** kwin MCP — `session_start` with the binary, `screenshot`, then `session_stop`. Screenshots are the proof.
5. Do not assume APIs from other Kirigami versions. If a pattern is not verifiable locally, say so instead of guessing.

## Architecture

- Single executable `kwal` + QML module `org.kde.kwal`. C++ root context (e.g. `AppController`) for app state/services; QML for UI.
- Settings via KConfig (`kwalrc`); identity: `OrganizationName "KDE"`, domain `kde.org`.
- QML: `qsTr()` for all user strings, `Kirigami.Units` for spacing/sizing, semantic `Kirigami.Theme` colors, Kirigami semantic components over raw Controls.
- KDE install layout (ECM): binary → `libexec`, desktop file + icon → `share`.
