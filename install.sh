#!/usr/bin/env sh
# Kwal installer (user-level, no root required).
#
# Usage:
#   ./install.sh    build (configuring if needed) + install to ~/.local
#
# Installs:
#   ~/.local/bin/kwal
#   ~/.local/bin/kwal-transition
#   ~/.local/share/applications/org.kde.kwal.desktop
#   ~/.local/share/icons/hicolor/{8x8,16x16,22x22,32x32,128x128,scalable}/apps/kwal.svg

set -eu

# Run from the project root (the directory holding this script).
cd "$(dirname "$0")"

./build.sh build

LOCAL_BIN="$HOME/.local/bin"
LOCAL_SHARE="$HOME/.local/share"

# Both binaries land side by side: AppController finds kwal-transition next
# to the app first (applicationDirPath), then on PATH.
install -Dm755 build/kwal "$LOCAL_BIN/kwal"
install -Dm755 build/kwal-transition "$LOCAL_BIN/kwal-transition"
install -Dm644 org.kde.kwal.desktop "$LOCAL_SHARE/applications/org.kde.kwal.desktop"
install -Dm644 icons/hicolor/8x8/apps/kwal.svg "$LOCAL_SHARE/icons/hicolor/8x8/apps/kwal.svg"
install -Dm644 icons/hicolor/16x16/apps/kwal.svg "$LOCAL_SHARE/icons/hicolor/16x16/apps/kwal.svg"
install -Dm644 icons/hicolor/22x22/apps/kwal.svg "$LOCAL_SHARE/icons/hicolor/22x22/apps/kwal.svg"
install -Dm644 icons/hicolor/32x32/apps/kwal.svg "$LOCAL_SHARE/icons/hicolor/32x32/apps/kwal.svg"
install -Dm644 icons/hicolor/128x128/apps/kwal.svg "$LOCAL_SHARE/icons/hicolor/128x128/apps/kwal.svg"
install -Dm644 icons/hicolor/scalable/apps/kwal.svg "$LOCAL_SHARE/icons/hicolor/scalable/apps/kwal.svg"

echo "Installed:"
echo "  $LOCAL_BIN/kwal"
echo "  $LOCAL_BIN/kwal-transition"
echo "  $LOCAL_SHARE/applications/org.kde.kwal.desktop"
echo "  $LOCAL_SHARE/icons/hicolor/{8x8,16x16,22x22,32x32,128x128,scalable}/apps/kwal.svg"

if command -v kwal >/dev/null 2>&1; then
    echo "Done. Launch it from the application menu (Kwal) or with 'kwal'."
else
    echo "Warning: $LOCAL_BIN is not on PATH; add it to your shell profile."
fi
