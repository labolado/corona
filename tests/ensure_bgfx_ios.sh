#!/bin/bash
#
# ensure_bgfx_ios.sh — 確保 iOS device + simulator bgfx .a 與當前 submodule 一致
#
# 流程：同 ensure_bgfx_android.sh，下載 bgfx-ios-device.tar.gz / bgfx-ios-simulator.tar.gz
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORONA_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BGFX_DIR="$CORONA_DIR/external/bgfx"

BGFX_SHA=$(git -C "$BGFX_DIR"              rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
BX_SHA=$(git   -C "$CORONA_DIR/external/bx"   rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
BIMG_SHA=$(git -C "$CORONA_DIR/external/bimg" rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
TAG="solar2d-${BGFX_SHA}-${BX_SHA}-${BIMG_SHA}"

echo "[ensure_bgfx_ios] tag=$TAG"

_ensure_platform() {
    local platform="$1"       # ios-device | ios-simulator
    local asset="$2"          # bgfx-ios-device.tar.gz | bgfx-ios-simulator.tar.gz
    local out_dir="$3"        # .build/ios-arm64/bin | .build/ios-simulator/bin
    local make_target="$4"    # gmake-ios-arm64 | gmake-ios-simulator
    local sentinel="$out_dir/.bgfx_sha"

    mkdir -p "$out_dir"

    if [ -f "$out_dir/libbgfxRelease.a" ] && [ -f "$sentinel" ] && [ "$(cat "$sentinel")" = "$TAG" ]; then
        echo "[ensure_bgfx_ios] $platform already up to date, skipping"
        return 0
    fi

    local DL_DIR="/tmp/bgfx-dl-${platform}-$$"
    mkdir -p "$DL_DIR"
    trap 'rm -rf "$DL_DIR"' RETURN

    echo "[ensure_bgfx_ios] Downloading $asset from release $TAG ..."
    if gh release download "$TAG" --repo labolado/bgfx \
        -p "$asset" -D "$DL_DIR" 2>/dev/null; then
        tar -xzf "$DL_DIR/$asset" -C "$out_dir"
        echo "$TAG" > "$sentinel"
        echo "[ensure_bgfx_ios] ✅ $platform downloaded"
        return 0
    fi

    # 本地編譯
    echo "[ensure_bgfx_ios] Release not found, building $platform locally..."
    local sdk_flag=""
    if [ "$platform" = "ios-device" ]; then
        SDKROOT=$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null)
        sdk_flag="CFLAGS=-isysroot $SDKROOT CXXFLAGS=-isysroot $SDKROOT"
    else
        SDKROOT=$(xcrun --sdk iphonesimulator --show-sdk-path 2>/dev/null)
        sdk_flag="CFLAGS=-isysroot $SDKROOT CXXFLAGS=-isysroot $SDKROOT"
    fi
    cd "$BGFX_DIR"
    make ".build/projects/$make_target" 2>/dev/null || true
    make -C ".build/projects/$make_target" config=release bgfx bx bimg bimg_decode \
        -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)" $sdk_flag
    echo "$TAG" > "$sentinel"
    echo "[ensure_bgfx_ios] ✅ $platform built locally"
}

_ensure_platform "ios-device"    "bgfx-ios-device.tar.gz"    \
    "$BGFX_DIR/.build/ios-arm64/bin"       "gmake-ios-arm64"

_ensure_platform "ios-simulator" "bgfx-ios-simulator.tar.gz" \
    "$BGFX_DIR/.build/ios-simulator/bin"   "gmake-ios-simulator"
