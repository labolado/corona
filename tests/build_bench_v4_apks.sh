#!/usr/bin/env bash
# build_bench_v4_apks.sh — build ONE variant's vulkan + gles bench APK with a
# GUARANTEED-clean native recompile, then verify the packaged libcorona.so SHA.
#
# Why this exists: gradle :Corona:clean does NOT purge the .cxx ninja cache, and a
# dangling Corona Native symlink let build_android.sh reuse a stale AAR — both caused
# v3 to ship a baseline binary mislabeled "optimized". This script nukes .cxx every
# time and asserts the resulting .so hash, so baseline vs optimized can't silently alias.
#
# Usage: bash tests/build_bench_v4_apks.sh <git-ref> <label> <expected_so_sha256>
#   e.g. bash tests/build_bench_v4_apks.sh c0d30547 baseline  530b51d2...d802687
#        bash tests/build_bench_v4_apks.sh bgfx-solar2d optimized ce1322fd...1709922
set -uo pipefail

REF="$1"; LABEL="$2"; EXPECT="$3"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORONA_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BGFX_DEMO="$CORONA_DIR/tests/bgfx-demo"
OUT_DIR="/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds"
BGF3X_CB="/Applications/Corona-b3/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder"
LOG="$OUT_DIR/v4-build-$LABEL.log"
mkdir -p "$OUT_DIR"
: > "$LOG"
say() { echo "[$LABEL] $*" | tee -a "$LOG"; }

so_sha() { # apk -> sha256 of lib/arm64-v8a/libcorona.so
    local apk="$1" t; t=$(mktemp -d)
    unzip -q -o "$apk" "lib/arm64-v8a/libcorona.so" -d "$t" 2>/dev/null
    shasum -a256 "$t/lib/arm64-v8a/libcorona.so" 2>/dev/null | awk '{print $1}'
    rm -rf "$t"
}

cd "$CORONA_DIR"
say "=== checkout $REF ==="
git checkout "$REF" >>"$LOG" 2>&1 || { say "checkout failed"; exit 1; }
say "HEAD=$(git rev-parse --short HEAD)"

say "=== purge native cache (critical: forces true recompile) ==="
rm -rf platform/android/sdk/.cxx \
       platform/android/sdk/build/intermediates/cxx \
       platform/android/sdk/build/intermediates/merged_native_libs \
       platform/android/sdk/build/intermediates/stripped_native_libs \
       platform/android/sdk/build/outputs/aar/Corona-release.aar

say "=== build + install AAR, package vulkan APK ==="
BUILD_LOG="$OUT_DIR/v4-android-$LABEL.log"
FORCE_BUILD=1 bash "$SCRIPT_DIR/build_android.sh" "$BGFX_DEMO" com.labolado.bgfxdemo > "$BUILD_LOG" 2>&1
# build_android.sh prints:  APK: /tmp/android-build-XXXX/bgfx-demo.apk
VK_APK=$(grep -oE '/tmp/android-build-[0-9]+/bgfx-demo\.apk' "$BUILD_LOG" | tail -1 || true)
[ -n "$VK_APK" ] && [ -f "$VK_APK" ] || { say "ERROR: vulkan apk not found after build (see $BUILD_LOG)"; tail -8 "$BUILD_LOG" | sed 's/^/    /' | tee -a "$LOG"; exit 1; }
cp "$VK_APK" "$OUT_DIR/${LABEL}-vulkan.apk"
say "vulkan apk: $VK_APK -> ${LABEL}-vulkan.apk"

say "=== package gles APK (force_gles, reuse installed AAR) ==="
GLES_TMP=$(mktemp -d "/tmp/v4-gles-${LABEL}-XXXX")
cp -r "$BGFX_DEMO/." "$GLES_TMP/"
touch "$GLES_TMP/solar2d_force_gles.txt"
GDST=$(mktemp -d "/tmp/v4-gles-dst-${LABEL}-XXXX")
LUA=/tmp/v4-gles-${LABEL}-$$.lua
cat > "$LUA" <<LUAEOF
local params = {
    platform = 'android',
    appName = 'bgfx-demo-gles',
    appVersion = '1.0',
    dstPath = '$GDST',
    projectPath = '$GLES_TMP',
    androidAppPackage = 'com.labolado.bgfxdemo.gles',
    keystorePath = '$HOME/.android/debug.keystore',
    keystorePassword = 'android',
    keystoreAlias = 'androiddebugkey',
    keystoreAliasPassword = 'android',
}
return params
LUAEOF
"$BGF3X_CB" build --lua "$LUA" >>"$LOG" 2>&1
GLES_APK=$(find "$GDST" -name "*.apk" 2>/dev/null | head -1 || true)
[ -n "$GLES_APK" ] && [ -f "$GLES_APK" ] || { say "ERROR: gles apk not found"; rm -rf "$GLES_TMP" "$GDST"; exit 1; }
cp "$GLES_APK" "$OUT_DIR/${LABEL}-gles.apk"
say "gles apk: $GLES_APK -> ${LABEL}-gles.apk"
rm -rf "$GLES_TMP" "$GDST" "$LUA"

say "=== verify packaged libcorona.so SHA (expect $EXPECT) ==="
VKS=$(so_sha "$OUT_DIR/${LABEL}-vulkan.apk")
GLS=$(so_sha "$OUT_DIR/${LABEL}-gles.apk")
say "vulkan .so sha: $VKS"
say "gles   .so sha: $GLS"
RC=0
[ "$VKS" = "$EXPECT" ] || { say "❌ vulkan .so sha MISMATCH"; RC=1; }
[ "$GLS" = "$EXPECT" ] || { say "❌ gles .so sha MISMATCH"; RC=1; }
[ "$RC" = 0 ] && say "✅ both APKs verified == $EXPECT"
exit $RC
