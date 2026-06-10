#!/bin/bash
#
# ensure_bgfx_android.sh — 確保 Android arm64 bgfx .a 與當前 submodule 一致
#
# 流程：
#   1. 計算 expected release tag（solar2d-{bgfx}-{bx}-{bimg}）
#   2. 比對 sentinel SHA 檔，已匹配就跳過
#   3. 嘗試從 GitHub release 下載預編譯 .a（CI 已編好）
#   4. 找不到 release → 本地交叉編譯（需要 ANDROID_NDK_ROOT）
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORONA_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BGFX_DIR="$CORONA_DIR/external/bgfx"
OUT_DIR="$BGFX_DIR/.build/android-arm64/bin"
LIB="$OUT_DIR/libbgfxRelease.a"
SENTINEL="$OUT_DIR/.bgfx_sha"

BGFX_SHA=$(git -C "$BGFX_DIR"              rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
BX_SHA=$(git   -C "$CORONA_DIR/external/bx"   rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
BIMG_SHA=$(git -C "$CORONA_DIR/external/bimg" rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
TAG="solar2d-${BGFX_SHA}-${BX_SHA}-${BIMG_SHA}"

echo "[ensure_bgfx_android] tag=$TAG"

# 已是最新，跳過
if [ -f "$LIB" ] && [ -f "$SENTINEL" ] && [ "$(cat "$SENTINEL")" = "$TAG" ]; then
    echo "[ensure_bgfx_android] Already up to date, skipping"
    exit 0
fi

mkdir -p "$OUT_DIR"
DL_DIR="/tmp/bgfx-dl-android-$$"
mkdir -p "$DL_DIR"
trap 'rm -rf "$DL_DIR"' EXIT

# 嘗試從 GitHub release 下載
echo "[ensure_bgfx_android] Downloading bgfx-android.tar.gz from release $TAG ..."
if gh release download "$TAG" --repo labolado/bgfx \
    -p "bgfx-android.tar.gz" -D "$DL_DIR" 2>/dev/null; then
    tar -xzf "$DL_DIR/bgfx-android.tar.gz" -C "$OUT_DIR"
    echo "$TAG" > "$SENTINEL"
    echo "[ensure_bgfx_android] ✅ Downloaded from release"
    exit 0
fi

# Release 不存在（CI 還沒跑完？）→ 本地交叉編譯
echo "[ensure_bgfx_android] Release not found, building locally..."
NDK="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-}}"
if [ -z "$NDK" ]; then
    echo "[ensure_bgfx_android] ERROR: ANDROID_NDK_ROOT not set; cannot build locally"
    echo "  Either wait for CI to publish the release, or set ANDROID_NDK_ROOT"
    exit 1
fi
export ANDROID_NDK_ROOT="$NDK"
cd "$BGFX_DIR"
make .build/projects/gmake-android-arm64 2>/dev/null || true
make -C .build/projects/gmake-android-arm64 config=release bgfx bx bimg bimg_decode \
    -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
echo "$TAG" > "$SENTINEL"
echo "[ensure_bgfx_android] ✅ Built locally"
