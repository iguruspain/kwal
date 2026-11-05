# Kwal

KDE Plasma utility to manage the desktop wallpaper and to tint supported
tools with color palettes extracted from the wallpaper. C++/Qt6/QML
(Kirigami) rewrite of the original Python app.

App id: `org.kde.kwal`.

## Features

- **Wallpapers**: browse folders, thumbnail grid, images and videos (with
  inline preview), search by name/tags and by 15 color categories.
- **Palettes**: Material You (official `material-color-utilities`, Celebi
  color extraction) plus ImageMagick accent/unique colors, per wallpaper,
  light and dark.
- **Custom commands**: shell commands run after a wallpaper is applied, with
  placeholders (`%path%`, `%image%`, `%scolor%`, `%mode%`, `%ccolor%`, …).
- **Tool tinting**: Fastfetch, Starship and Ulauncher tabs (each appears only
  when its binary is on `PATH`) recolor those tools with the extracted
  palette and preview the result.
- **Wallpaper transitions**: a standalone `kwal-transition` overlay (Wayland
  layer-shell) animates the switch; falls back to a plain switch when
  unavailable.
- **AI tagging**: optional OpenAI-compatible vision endpoint to tag
  wallpapers.

## Requirements

- CMake ≥ 3.16 + Ninja, GCC with C++20
- ECM / KDE Frameworks 6.31 (`Config`, `Kirigami`, `KirigamiPlatform`)
- Qt 6 (≥ 6.8): `Core`, `Gui`, `Multimedia`, `Network`, `Qml`, `Quick`,
  `QuickControls2`, `Widgets`, `WebEngineQuick`
- ffmpeg (`libavutil` dev package; Qt Multimedia's FFmpeg backend)
- ImageMagick (`magick`) at runtime

CMake fetches two third-party dependencies via `FetchContent`:
`tomlplusplus` and `material-color-utilities` (built as a static `mcu_lib`).

## Build

```sh
cmake --preset default          # Ninja, Debug, compile_commands.json
cmake --build --preset default
ctest --test-dir build          # QtTest unit tests (sandbox HOME/XDG)
./build/kwal                    # run
```

`./build.sh [build|test|run|config|clean]` wraps the same steps and
auto-configures on first run.

> On systems where Qt 5 and Qt 6 coexist, `/usr/bin/qml` and
> `/usr/bin/qmllint` may be Qt 5. Use `qml6` and the Qt 6 `qmllint` (or the
> built binary) for QML tooling.

## Configuration

Settings are stored by KConfig in `kwalrc` (`[Wallpapers]`, `[Commands]`,
`[Transitions]`, `[General]`, `[Ulauncher]`). Media caches live under
`~/.cache/kwal/` (thumbnail/first-frame cache, video resizes, and a
versioned per-file index).

## License

GPL-3.0-or-later — see `LICENSE.txt`.
