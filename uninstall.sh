#!/usr/bin/env sh
# Kwal uninstaller.
#
# Installed files (binaries, desktop entry, icon) are removed without
# asking. User data is only removed after an explicit per-item
# confirmation (default: no).

set -eu

# Run from the project root (the directory holding this script).
cd "$(dirname "$0")"

remove_installed() {
    for path in \
        "$HOME/.local/bin/kwal" \
        "$HOME/.local/bin/kwal-transition" \
        "$HOME/.local/share/applications/org.kde.kwal.desktop" \
        "$HOME/.local/share/icons/hicolor/8x8/apps/kwal.svg" \
        "$HOME/.local/share/icons/hicolor/16x16/apps/kwal.svg" \
        "$HOME/.local/share/icons/hicolor/22x22/apps/kwal.svg" \
        "$HOME/.local/share/icons/hicolor/32x32/apps/kwal.svg" \
        "$HOME/.local/share/icons/hicolor/128x128/apps/kwal.svg" \
        "$HOME/.local/share/icons/hicolor/scalable/apps/kwal.svg" \
        "$HOME/.local/share/icons/hicolor/scalable/apps/org.kde.kwal.svg"
    do
        if [ -e "$path" ]; then
            rm -rf "$path"
            echo "removed: $path"
        else
            echo "skipping (not present): $path"
        fi
    done
}

remove_data_if_confirmed() {
    path="$1"
    if [ ! -e "$path" ]; then
        echo "skipping (not present): $path"
        return
    fi
    printf 'Delete %s? [y/N] ' "$path"
    read -r answer
    case "$answer" in
        [yY]|[yY][eE][sS])
            rm -rf "$path"
            echo "removed: $path"
            ;;
        *)
            echo "kept: $path"
            ;;
    esac
}

remove_installed

# User data: only removed on explicit confirmation. Note that
# ~/.cache/kwal is shared with the legacy Python app.
remove_data_if_confirmed "$HOME/.config/kwalrc"
remove_data_if_confirmed "$HOME/.config/kwal"
remove_data_if_confirmed "$HOME/.cache/kwal"

echo "Done."
