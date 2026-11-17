<!--
  Appended verbatim by .github/workflows/release.yml to the [X.Y.Z] section it
  extracts from CHANGELOG.md, so keep this file free of placeholders and of
  instructions for whoever cuts the release: it IS the release text. Only the
  facts that hold for every release belong here; anything version- or
  channel-specific that does not exist yet stays out until it does.
-->

### Requirements

- KDE Plasma 6 on Wayland (the app talks to `org.kde.PlasmaShell` and uses
  layer-shell overlays for the transitions)
- Qt ≥ 6.8 and KF6 ≥ 6.31 — distro packages provide these on rolling and on
  recent Fedora/openSUSE; older stable distros trail behind
- `magick` (ImageMagick) at runtime — palettes and Fastfetch logo tinting
- `ffmpeg` / `ffprobe` at runtime — video first frames and resizes
- optional: [Smart Video Wallpaper Reborn](https://github.com/luisbocanegra/plasma-smart-video-wallpaper-reborn) for video wallpapers
- optional: `fastfetch` / `starship` / `ulauncher` for the tool tabs

### Install

- **From source** (works today): the dependency list and the three
  `cmake` commands are in the README; CMake fetches and vendors `tomlplusplus`
  and `material-color-utilities` itself.
- **Arch**: `packaging/aur/PKGBUILD` is in the tree — build it with
  `makepkg -si`. The AUR package itself is not published yet.
- **AppImage / rpm / deb**: not available yet.

### Checksums

`sha256sums.txt` is attached; it holds the sha256 of the source tarball GitHub
generated for this tag. Verify with:

```sh
curl -fsLO https://github.com/iguruspain/kwal/releases/download/vX.Y.Z/sha256sums.txt
curl -fsL -o kwal-X.Y.Z.tar.gz \
  https://github.com/iguruspain/kwal/archive/refs/tags/vX.Y.Z.tar.gz
sha256sum -c sha256sums.txt
```
