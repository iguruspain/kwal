<!--
  .github/workflows/release.yml appends this file to the [X.Y.Z] section it
  takes from CHANGELOG.md, after stripping this leading comment: what you write
  here is the release text, so keep it for people installing the app. It must
  stay version-neutral (the workflow stamps vX.Y.Z in the snippets); anything
  that does not exist yet stays out until it does.
-->

### Requirements

- KDE Plasma 6 on Wayland.
- Qt ≥ 6.8 and KF6 ≥ 6.31 — provided by the distro packages on rolling distros
  and recent Fedora/openSUSE; older stable releases trail behind.
- ImageMagick and ffmpeg.
- Optional: [Smart Video Wallpaper Reborn](https://github.com/luisbocanegra/plasma-smart-video-wallpaper-reborn) for video wallpapers, and `fastfetch` / `starship` / `ulauncher` for the tool tabs.

### Install

- **From source**: the README lists the dependencies and the three `cmake`
  commands; CMake fetches and vendors its two build-time dependencies itself.
- **Arch**: `packaging/aur/PKGBUILD` in this release, `makepkg -si`. The AUR
  package is not published yet.
- **AppImage, rpm, deb**: not available yet.

### Verify what you downloaded

Every release here is cut from a signed tag. With the key that ships in the
repository:

```sh
git clone --branch vX.Y.Z https://github.com/iguruspain/kwal.git
cd kwal
git -c gpg.ssh.allowedSignersFile=docs/kwal_signers verify-tag vX.Y.Z
```

`Good "git" signature for …` means the code is ours. The
`Source code (zip / tar.gz)` buttons below are that same tree without the
signature: fine to use, but the clone above is the one you can check.
