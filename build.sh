#!/usr/bin/env sh
# Kwal build helper.
#
# Usage:
#   ./build.sh            configure (if needed) + build
#   ./build.sh build      same as above
#   ./build.sh test       build + run tests
#   ./build.sh run        build + launch the app
#   ./build.sh config     (re)configure only
#   ./build.sh clean      remove the build directory
#
# Extra args after the command are passed through to cmake/cmake --build,
# e.g. ./build.sh build -j16

set -eu

# Run from the project root (the directory holding this script).
cd "$(dirname "$0")"

PRESET="default"

# Reconfigure if there is no build dir yet, or the cache points at a stale source tree.
ensure_configured() {
    if [ ! -f "build/CMakeCache.txt" ]; then
        echo ">> configuring (preset: $PRESET)"
        cmake --preset "$PRESET" "$@"
    fi
}

cmd="${1:-build}"
shift || true

case "$cmd" in
    build|"")
        ensure_configured
        cmake --build --preset "$PRESET" "$@"
        ;;
    test)
        ensure_configured
        cmake --build --preset "$PRESET" "$@"
        ctest --test-dir build
        ;;
    run)
        ensure_configured
        cmake --build --preset "$PRESET"
        exec ./build/kwal
        ;;
    config)
        cmake --preset "$PRESET" "$@"
        ;;
    clean)
        rm -rf build
        ;;
    *)
        echo "unknown command: $cmd" >&2
        echo "usage: $0 [build|test|run|config|clean]" >&2
        exit 2
        ;;
esac
