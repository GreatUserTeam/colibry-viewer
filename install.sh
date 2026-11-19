#!/usr/bin/env bash
# Build and install a Colibry OS app on Raven.
# Usage: ./install.sh [--prefix DIR] [--raven-dir DIR] [--uninstall]
# Raven: --raven-dir points at a Raven checkout (default: installed package).
set -euo pipefail

PREFIX="/usr/local"
RAVEN_DIR=""
UNINSTALL=0
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --raven-dir) RAVEN_DIR="$2"; shift 2 ;;
        --uninstall) UNINSTALL=1; shift ;;
        -h|--help) sed -n '2,4p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="$HERE/build"
MANIFEST="$BUILD/install_manifest.txt"

if [ "$UNINSTALL" -eq 1 ]; then
    [ -f "$MANIFEST" ] || { echo "no install manifest at $MANIFEST" >&2; exit 1; }
    while IFS= read -r f; do rm -f -- "$f" && echo "removed $f"; done < <(cat "$MANIFEST"; echo)
    exit 0
fi

command -v cmake >/dev/null || { echo "cmake is required" >&2; exit 1; }
ARGS=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX")
[ -n "$RAVEN_DIR" ] && ARGS+=(-DRAVEN_DIR="$RAVEN_DIR")
cmake -S "$HERE" -B "$BUILD" "${ARGS[@]}"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 2)"
cmake --install "$BUILD"
echo "installed to $PREFIX"
