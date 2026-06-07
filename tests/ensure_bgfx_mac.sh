#!/bin/bash
#
# ensure_bgfx_mac.sh — rebuild the bgfx static libraries if the bgfx submodule
# sources changed since the last build.
#
# Why this exists:
#   The Xcode project (ratatouille.xcodeproj) links
#   external/bgfx/.build/projects/xcode15/libbgfxRelease.a (a symlink to the
#   osx-arm64 Release lib) as an opaque file. Xcode has NO dependency on
#   external/bgfx/src, so editing bgfx sources (e.g. renderer_mtl.cpp) does NOT
#   trigger a relink — you silently keep linking the stale .a. tests/build.sh
#   calls this script before xcodebuild so any bgfx source change is rebuilt
#   automatically (both the Debug and Release Solar2D configs link the Release
#   bgfx lib, so Release is all we need).
#
#   `make osx-arm64-release` does its own .cpp -> .o -> .a dependency tracking, so
#   a no-op run is cheap; we additionally short-circuit with a find() check so the
#   common "nothing changed" case adds well under a second to the build.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORONA_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BGFX_DIR="$CORONA_DIR/external/bgfx"
LIB="$BGFX_DIR/.build/osx-arm64/bin/libbgfxRelease.a"

if [ ! -d "$BGFX_DIR/src" ]; then
    echo "[ensure_bgfx] $BGFX_DIR/src not found — submodule not initialized? skipping"
    exit 0
fi

if [ ! -f "$LIB" ]; then
    echo "[ensure_bgfx] $LIB missing → building bgfx (osx-arm64)"
    ( cd "$BGFX_DIR" && make osx-arm64-release )
    exit 0
fi

# Is any bgfx source newer than the built library?
newer="$(find "$BGFX_DIR/src" "$BGFX_DIR/include" \
    \( -name '*.cpp' -o -name '*.h' -o -name '*.sc' -o -name '*.inl' \) \
    -newer "$LIB" -print 2>/dev/null | head -n1 || true)"

if [ -n "$newer" ]; then
    echo "[ensure_bgfx] bgfx source changed (e.g. ${newer#$BGFX_DIR/}) → rebuilding osx-arm64"
    ( cd "$BGFX_DIR" && make osx-arm64-release )
else
    echo "[ensure_bgfx] bgfx libraries up to date"
fi
