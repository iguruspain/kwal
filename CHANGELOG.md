# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- On publication (consolidation in main + annotated tag), rename this
     section to [0.1.0] and stamp the release date. -->

## [Unreleased]

The Qt 6 / Kirigami rewrite of Kwal, a KDE Plasma utility that
manages the wallpaper and tints your favorite tools with palettes extracted
from it.

### Added

- Wallpapers: folder sidebar and thumbnail grid (FDO thumbnails, live folder
  watching), apply wallpapers per desktop through the Plasma D-Bus shell with
  a byte-preserving `appletsrc` fallback, optional lockscreen mirroring
  (`kscreenlockerrc`), and the grid opens positioned at the last applied
  wallpaper.
- Search: by name/tags and by 15 matugen-style color categories (computed from
  the image itself, HCT classifier); categories and user tags are editable in
  the detail panel and survive rescans.
- Detail panel: a non-overlay side panel (media preview, categories, tags,
  palettes, custom commands) that follows the grid selection; videos play
  inline muted, GIFs and animated WebP animate inline.
- Palettes: Material You through the official `material-color-utilities`
  library (nine M3 schemes, 16 swatches each, cached per wallpaper and dark
  mode), plus ImageMagick unique-color extraction; the source-color dots of
  the selected wallpaper feed the custom-color placeholder.
- Videos in the grid: first-frame thumbs (ffmpeg), inline playback, and full
  support for the Smart Video Wallpaper Reborn plugin (per-folder playlists
  kept in sync in the plugin's own config format, dead playlist entries
  pruned).
- Video resizes: apply a scaled cache copy instead of the original
  (target height and fps configurable, editable combos), generated on demand
  for the picked video or in explicit per-folder batches, with
  "Clear resizes cache" maintenance.
- Wallpaper transitions: standalone `kwal-transition` binary painting a
  full-screen Wayland layer-shell overlay (awww-style plane effects: grow,
  fade, wipe, wave, directional/center/outer presets, bezier/easing advanced
  group), performing the Plasma switch mid-animation and degrading to a
  plain switch on any setup failure.
- Custom commands: run on wallpaper apply with placeholders (`%path%`,
  `%image%`, `%scolor%`/`%sc%`, `%mode%`, `%ccolor%`), enabled toggles and
  drag-reorder in the detail panel.
- AI tagging: tag wallpapers through any OpenAI-compatible vision chat API
  (a local `llama-server` works), image-only model filtering, editable system
  prompt, merged non-destructive results, "Clear all tags" maintenance.
- Fastfetch tab: JSON config management with live current/selected/tinted
  previews and logo tinting via ImageMagick — animated GIFs tint frame by
  frame keeping delay and loop count.
- Starship tab: faithful terminal prompt preview (live draft, blinking
  cursor) and tinted-palette template workflow with bundled templates.
- Ulauncher tab: theme writing plus a live preview mockup (window width,
  typed query, sample rows) configurable in Settings.
- Tool tabs and their settings sections appear only when the tool's binary is
  on PATH.
- Settings window: General (window size, transparency + level), Wallpapers
  (matugen dots, color categories, lockscreen, video resizes group, AI
  tagging group, transitions group), Ulauncher (preview mockup) and About
  (standard `Kirigami.AboutPage`).
- Settings → Wallpapers diagnostics: two inline warnings for the two failures
  that are otherwise invisible — image formats this Qt build cannot decode
  (the accepted wallpaper extensions diffed against
  `QImageReader::supportedImageFormats()`, naming the Arch package that owns
  each missing handler), and a folder holding videos while Plasma's Smart Video
  Wallpaper Reborn plugin is absent, re-checked every time the page opens so
  installing it needs no restart.
- Keyboard-first browsing: the grid, folder list and swatch groups are single
  Tab stops with arrow-key cursors; Space opens the detail panel, Escape
  closes it and returns focus to the grid.
- Branding: animated jellyfish SVG app icon (SMIL), desktop entry
  `org.kde.kwal`.

### Fixed / hardened (worth calling out)

- Gifs animate in the Fastfetch current and selected previews.
- Sane Tab focus chain: color swatches are mouse-only and never trap the
  keyboard.
- Raw `file://` URLs kept unescaped in the video plugin config so the
  plugin's own config page displays and matches them.
- Per-wallpaper source-color selection persists only for the applied
  wallpaper; `.bak` config backups refresh before every write.
- Cache hygiene: orphaned first frames / resized videos swept at startup,
  dead playlist URLs pruned from the desktop config, "Clear resizes cache"
  keeps `appletsrc` consistent.
- Arch's split Qt image-format plugins declared as dependencies (AUR package, CI
  and AppImage containers): `qt6-imageformats` carries the WebP handler
  `libqwebp.so` and `qt6-svg` carries `libqsvg.so` — neither is pulled in by
  `qt6-base`/`qt6-declarative`, and `svg`/`webp` are accepted wallpaper types.
  Without the handler Qt cannot read the file at all (`QImageReader` answers
  "Unsupported image format"), so every `.webp` probed as static and the
  animated-WebP test in `tst_wallpapersinfo` failed with a bare `FALSE`; the
  test now names the missing handler and the fixture checks its own frame count.
- QML diagnostics reach the terminal again: `QtWebEngineQuick::initialize()`
  installs its own message handler (it forwards Qt logging into Chromium's
  logging system), which silently swallowed every `qWarning` — including
  `QQmlApplicationEngine failed to load component`, the first diagnostic for a
  window that never appears. `main.cpp` had a handler written for exactly this
  but never installed it; it is now installed after the application object
  exists, so the last installer wins.
