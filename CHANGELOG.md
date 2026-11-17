# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- scripts/release.sh stamps the [Unreleased] content as [<version>] with the
     release date and opens a fresh empty [Unreleased] section above it, in the
     release commit. The release page publishes this section verbatim, so write
     it for someone who is installing the app, not for whoever maintains it:
     internals belong in the commit messages. -->

## [Unreleased]

## [0.1.0] - 2026-11-17

First release: the Qt 6 / Kirigami rewrite of Kwal. Kwal sets your KDE Plasma
wallpaper and tints your tools with the colours it takes from that wallpaper.

### Added

- **Wallpapers** — browse your folders as a thumbnail grid, search by name, tag
  or colour, apply to one desktop or to all of them, and mirror the result to
  the lockscreen if you want. The grid opens on the wallpaper you last applied.
- **Video and animated wallpapers** — videos and GIFs play in the grid, video
  wallpapers work with Plasma's Smart Video Wallpaper Reborn plugin, and Kwal
  keeps a smaller copy of a video so switching stays fast.
- **Colours** — every wallpaper gets a Material You palette (nine light and
  dark schemes). The colour you pick drives the tools and your own commands.
- **Transitions** — animated switches between wallpapers: grow, fade, wipe,
  wave and friends.
- **Tools** — Fastfetch logo tinting with a preview of the result, Starship
  prompt themes with a live preview, Ulauncher themes with a live preview. A
  tab appears only when that tool is installed, and it picks up changes you
  make outside Kwal.
- **Commands** — run your own commands when a wallpaper is applied, with
  placeholders for the image, the chosen colour, dark or light mode and more.
- **AI tagging** — tag wallpapers with any OpenAI-compatible vision API, a
  local llama.cpp server included.
