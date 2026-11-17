<p align="center">
  <img src="icons/hicolor/scalable/apps/kwal.svg" width="180" alt="Kwal jellyfish logo">
</p>

# Kwal

KDE Plasma utility to manage the desktop wallpaper and to tint supported
tools with color palettes extracted from the wallpaper. C++/Qt6/QML
(Kirigami) rewrite of the original Python app.

App id: `org.kde.kwal`.

<p align="center">
  <img src="screenshots/kwal-tour.webp" width="600" alt="Animated tour of the kwal interface: wallpaper grid, folder sidebar, wallpaper detail drawer with color categories, tags and custom commands, Fastfetch logo tinting, Starship palette with terminal preview, and the Ulauncher theme with its live preview">
</p>

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
- Wayland for the `kwal-transition` overlay: `wayland` (libwayland-client +
  `wayland-scanner`) and `wayland-protocols` ≥ 1.24 (xdg-shell, viewporter,
  xdg-output XMLs); the wlr-layer-shell XML is vendored in the repo

CMake fetches two third-party dependencies via `FetchContent`:
`tomlplusplus` and `material-color-utilities` (built as a static `mcu_lib`,
patched to build without abseil via `cmake/mcu-gcc16.patch`). For offline or
packaged builds, point `FETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS` and
`FETCHCONTENT_SOURCE_DIR_MCU` at prepared source trees and configure with
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`; the patch is applied to a pristine
tree automatically.

## Installation

Both options build the same signed source, so start by cloning the tag you want
and checking its signature:

```sh
git clone --branch v0.1.0 https://github.com/iguruspain/kwal.git   # the tag you want
cd kwal
git -c gpg.ssh.allowedSignersFile=docs/kwal_signers verify-tag v0.1.0
```

Every release tag is signed with the SSH key published in `docs/kwal_signers`,
so verification needs nothing but the repository itself: the command above
prints `Good "git" signature for …`. GitHub's `Source code (zip/tar.gz)` links
on a release page are the same tree without the signature — convenient, not
verifiable, and their bytes are GitHub's gzip output, which is why no checksum
for them is published anywhere.

### Option A — Arch Linux

```sh
cd packaging/aur
makepkg -si
```

*(Installs `kwal` and `kwal-transition`, the desktop entry and the icon through
pacman; remove with `sudo pacman -Rns kwal`.)*

### Option B — any distro: build with CMake

```sh
cmake --preset default          # Ninja, Debug, compile_commands.json
cmake --build --preset default
ctest --test-dir build          # QtTest unit tests (sandbox HOME/XDG)
./build/kwal                    # run
```

`./build.sh [build|test|run|config|clean]` wraps the same steps and
auto-configures on first run.

To install (`kwal` + `kwal-transition`, desktop file and icon; the QML module
`org.kde.kwal` is embedded in the binary):

```sh
cmake --install build                     # prefix = CMake default
cmake --install build --prefix ~/.local   # per-user instead
```

To uninstall a manual install, remove exactly what `cmake --install` copied.
CMake records those paths in `build/install_manifest.txt`, but that file lives
in the build tree — it is gone as soon as the tree is cleaned, and
`xargs -r rm < …` then does nothing at all. Do not depend on it; the install set
is fixed (the `install()` rules at the bottom of `CMakeLists.txt`):

```sh
PREFIX=~/.local   # the --prefix you installed with; /usr if you used no --prefix
rm    -f "$PREFIX"/bin/kwal "$PREFIX"/bin/kwal-transition \
         "$PREFIX"/share/applications/org.kde.kwal.desktop \
         "$PREFIX"/share/icons/hicolor/scalable/apps/kwal.svg
sudo rm -f /usr/bin/kwal /usr/bin/kwal-transition \
           /usr/share/applications/org.kde.kwal.desktop \
           /usr/share/icons/hicolor/scalable/apps/kwal.svg
```

Distro packages uninstall through their package manager as usual. Uninstalling
never removes user state: `~/.config/kwalrc` (settings),
`~/.config/kwal/templates` (bundled tool templates),
`~/.cache/kwal/` (thumbnails, video frames and resizes, index) and the Plasma
configuration the app applied stay untouched by design. Drop those too if you
want a clean slate: `rm -rf ~/.config/kwalrc ~/.config/kwal ~/.cache/kwal`.

> On systems where Qt 5 and Qt 6 coexist, `/usr/bin/qml` and
> `/usr/bin/qmllint` may be Qt 5. Use `qml6` and the Qt 6 `qmllint` (or the
> built binary) for QML tooling.

## Configuration

Settings are stored by KConfig in `~/.config/kwalrc` (`KConfig::SimpleConfig`,
so there is no `~/.config/kwal/` subfolder for them) in the groups
`[Wallpapers]`, `[Commands]`,
`[Transitions]`, `[General]` and `[Ulauncher]`. Media caches live under
`~/.cache/kwal/` (thumbnail/first-frame cache, video resizes, and a
versioned per-file index).

## Acknowledgements

- [awww](https://codeberg.org/LGFae/awww) — the animated wallpaper switch of
  `kwal-transition` is a C++ port of its transition renderer (via the
  awww-based `kdewall-plugin-switcher` reference implementation).
- [Smart Video Wallpaper Reborn](https://github.com/luisbocanegra/plasma-smart-video-wallpaper-reborn)
  by [luisbocanegra](https://github.com/luisbocanegra) — the Plasma 6 wallpaper
  plugin kwal drives to play video wallpapers (desktop and lockscreen),
  including the video playlists it maintains in the plugin's config.

## License

GPL-3.0-or-later — see `LICENSE.txt`.
