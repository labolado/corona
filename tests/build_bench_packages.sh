#!/bin/bash
# build_bench_packages.sh — 編 5 個 FTL bench 包
#
# 輸出 (OUT_DIR):
#   bgfx-vulkan.apk   — bgfx + Vulkan (Android)
#   bgfx-gles.apk     — bgfx + GLES forced (Android)
#   bgfx-metal.ipa    — bgfx + Metal (iOS, FTL-ready)
#   official-android.apk — 官方 GL (Android)
#   official-ios.ipa  — 官方 GL (iOS, FTL-ready)
#
# 依賴:
#   /Applications/Corona-b3   bgfx SDK
#   /Applications/Corona       官方 SDK
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORONA_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BGFX_DEMO="$CORONA_DIR/tests/bgfx-demo"
OUT_DIR="${OUT_DIR:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds}"
LOG="$OUT_DIR/build.log"

OFFICIAL_CB="/Applications/Corona/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder"
BGF3X_CB="/Applications/Corona-b3/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder"
FTL_PROFILE="${FTL_PROFILE:-/Users/yee/ftl-resources/profiles/AdHoc_com.labolado.*.mobileprovision}"
FTL_SIGN_ID="${FTL_SIGN_ID:-61F861C8E26319F52E90AA240DE2125135BDAD80}"

mkdir -p "$OUT_DIR"
MARKER="$OUT_DIR/.build_marker_$$"
touch "$MARKER"
echo "=== bench build $(date) ===" | tee "$LOG"

log()  { echo "[bench] $*" | tee -a "$LOG"; }
fail() { echo "[FAIL] $*" | tee -a "$LOG"; exit 1; }

# ── helper: find newest APK/IPA produced after MARKER ───────
find_newest_apk() {
    find /tmp -name "*.apk" -newer "$MARKER" 2>/dev/null | sort -t'-' -k4 | tail -1
}
find_newest_ipa() {
    find /tmp -name "*.ipa" -newer "$MARKER" 2>/dev/null | sort | tail -1
}

# ── helper: build android APK via CoronaBuilder ─────────────
build_android_cb() {
    local project="$1" pkg="$2" cb="$3"
    local app_name="$(basename "$project")"
    local dst
    dst=$(mktemp -d "/tmp/bench-android-XXXX")
    cat > /tmp/bench-lua-$$.lua << LUAEOF
local params = {
    platform = 'android',
    appName = '$app_name',
    appVersion = '1.0',
    dstPath = '$dst',
    projectPath = '$project',
    androidAppPackage = '$pkg',
    keystorePath = '$HOME/.android/debug.keystore',
    keystorePassword = 'android',
    keystoreAlias = 'androiddebugkey',
    keystoreAliasPassword = 'android',
}
return params
LUAEOF
    log "  CoronaBuilder android pkg=$pkg → $dst"
    "$cb" build --lua /tmp/bench-lua-$$.lua 2>&1 | tee -a "$LOG" | grep -E "succeed|fail|error|Error" | head -5 || true
    rm -f /tmp/bench-lua-$$.lua
    local apk
    apk=$(find "$dst" -name "*.apk" 2>/dev/null | head -1 || true)
    if [ -z "$apk" ] || [ ! -f "$apk" ]; then
        log "  ERROR: no APK in $dst"
        ls "$dst" | tee -a "$LOG"
        return 1
    fi
    echo "$apk"
}

# ── helper: build iOS IPA via CoronaBuilder + resign ────────
build_ios_cb() {
    local project="$1" bundle_id="$2" cb="$3"
    local app_name="$(basename "$project")"
    local dst
    dst=$(mktemp -d "/tmp/bench-ios-XXXX")
    # Find actual provisioning profile (glob expand)
    local profile
    profile=$(ls $FTL_PROFILE 2>/dev/null | head -1 || true)
    [ -n "$profile" ] || { log "ERROR: no FTL profile at $FTL_PROFILE"; return 1; }

    cat > /tmp/bench-lua-$$.lua << LUAEOF
local params = {
    platform = 'ios',
    appName = '$app_name',
    appVersion = '1.0',
    dstPath = '$dst',
    projectPath = '$project',
    certificatePath = '$profile',
}
return params
LUAEOF
    log "  CoronaBuilder ios bundle=$bundle_id → $dst"
    "$cb" build --lua /tmp/bench-lua-$$.lua 2>&1 | tee -a "$LOG" | grep -E "succeed|fail|error|Error" | head -5 || true
    rm -f /tmp/bench-lua-$$.lua

    local app_path="$dst/$app_name.app"
    [ -d "$app_path" ] || { log "ERROR: $app_path not found"; ls "$dst" | tee -a "$LOG"; return 1; }

    # Fix bundle ID
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $bundle_id" "$app_path/Info.plist" 2>/dev/null || true

    # Inject firebase-game-loop URL scheme (FTL requirement)
    /usr/libexec/PlistBuddy -c 'Delete :CFBundleURLTypes' "$app_path/Info.plist" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c 'Add :CFBundleURLTypes array' "$app_path/Info.plist"
    /usr/libexec/PlistBuddy -c 'Add :CFBundleURLTypes:0 dict' "$app_path/Info.plist"
    /usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0:CFBundleURLName string $bundle_id" "$app_path/Info.plist"
    /usr/libexec/PlistBuddy -c 'Add :CFBundleURLTypes:0:CFBundleURLSchemes array' "$app_path/Info.plist"
    /usr/libexec/PlistBuddy -c 'Add :CFBundleURLTypes:0:CFBundleURLSchemes:0 string firebase-game-loop' "$app_path/Info.plist"

    # Resign with distribution cert
    rm -rf "$app_path/_CodeSignature"
    cp "$profile" "$app_path/embedded.mobileprovision"

    # Extract team ID from profile
    local team_id
    team_id=$(security cms -D -i "$profile" 2>/dev/null | plutil -extract TeamIdentifier.0 raw - -o - 2>/dev/null || echo "")
    if [ -z "$team_id" ]; then
        team_id=$(security cms -D -i "$profile" 2>/dev/null | grep -A1 "TeamIdentifier" | tail -1 | sed 's/.*<string>//;s/<\/string>//' || echo "UNKNOWN")
    fi

    cat > /tmp/ent-$$.plist << ENTEOF
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
    <key>get-task-allow</key><false/>
    <key>application-identifier</key><string>${team_id}.${bundle_id}</string>
</dict></plist>
ENTEOF
    \codesign -f -s "$FTL_SIGN_ID" --entitlements /tmp/ent-$$.plist "$app_path" 2>&1 | tee -a "$LOG" | tail -3
    rm -f /tmp/ent-$$.plist

    # Package IPA
    local ipa="$dst/$app_name.ipa"
    mkdir -p "$dst/Payload"
    cp -r "$app_path" "$dst/Payload/"
    (cd "$dst" && \zip -qr "$ipa" Payload/)
    echo "$ipa"
}

# ═══════════════════════════════════════════════════════════
# Build 1: bgfx-vulkan APK (full build: AAR + CoronaBuilder)
# ═══════════════════════════════════════════════════════════
log "=== 1/5: bgfx-vulkan APK ==="
touch "$MARKER"  # reset marker

bash "$SCRIPT_DIR/build_android.sh" "$BGFX_DEMO" com.labolado.bgfxdemo 2>&1 | \
    tee -a "$LOG" | grep -E "\[build\]|Step|succeed|fail|ERROR|APK:" | head -60

# Find the APK (build_android.sh puts it in /tmp/android-build-HHMMSS/bgfxdemo.apk)
VULKAN_APK=$(find /tmp -name "bgfxdemo.apk" -newer "$MARKER" 2>/dev/null | head -1 || true)
if [ -z "$VULKAN_APK" ]; then
    VULKAN_APK=$(find /tmp -name "*.apk" -newer "$MARKER" 2>/dev/null | head -1 || true)
fi
if [ -n "$VULKAN_APK" ] && [ -f "$VULKAN_APK" ]; then
    cp "$VULKAN_APK" "$OUT_DIR/bgfx-vulkan.apk"
    log "✅ bgfx-vulkan.apk ($(ls -lh "$OUT_DIR/bgfx-vulkan.apk" | awk '{print $5}'))"
else
    log "⚠️  bgfx-vulkan.apk not found — continuing"
fi

# ═══════════════════════════════════════════════════════════
# Build 2: bgfx-gles APK (reuse compiled AAR, inject force_gles)
# ═══════════════════════════════════════════════════════════
log "=== 2/5: bgfx-gles APK ==="
GLES_TMP=$(mktemp -d "/tmp/bgfx-demo-gles-XXXX")
cp -r "$BGFX_DEMO/." "$GLES_TMP/"
touch "$GLES_TMP/solar2d_force_gles.txt"

GLES_APK=$(build_android_cb "$GLES_TMP" "com.labolado.bgfxdemo.gles" "$BGF3X_CB")
if [ -n "$GLES_APK" ] && [ -f "$GLES_APK" ]; then
    cp "$GLES_APK" "$OUT_DIR/bgfx-gles.apk"
    log "✅ bgfx-gles.apk ($(ls -lh "$OUT_DIR/bgfx-gles.apk" | awk '{print $5}'))"
fi
rm -rf "$GLES_TMP"

# ═══════════════════════════════════════════════════════════
# Build 3: bgfx-metal IPA (bgfx engine compile + FTL sign)
# ═══════════════════════════════════════════════════════════
log "=== 3/5: bgfx-metal IPA ==="
touch "$MARKER"

bash "$SCRIPT_DIR/build_ios.sh" \
    "$BGFX_DEMO" \
    "com.labolado.bgfxdemo" \
    "" "" \
    --ftl 2>&1 | tee -a "$LOG" | grep -E "\[build\]|Step|succeed|fail|ERROR|\.ipa" | head -60

METAL_IPA=$(find /tmp -name "*.ipa" -newer "$MARKER" 2>/dev/null | head -1 || true)
if [ -n "$METAL_IPA" ] && [ -f "$METAL_IPA" ]; then
    cp "$METAL_IPA" "$OUT_DIR/bgfx-metal.ipa"
    log "✅ bgfx-metal.ipa ($(ls -lh "$OUT_DIR/bgfx-metal.ipa" | awk '{print $5}'))"
else
    log "⚠️  bgfx-metal.ipa not found — check log"
fi

# ═══════════════════════════════════════════════════════════
# Build 4: official-android APK
# ═══════════════════════════════════════════════════════════
log "=== 4/5: official-android APK ==="
OFFAND_TMP=$(mktemp -d "/tmp/bgfx-demo-offand-XXXX")
cp -r "$BGFX_DEMO/." "$OFFAND_TMP/"
printf 'perf' > "$OFFAND_TMP/solar2d_test.txt"

OFFAND_APK=$(build_android_cb "$OFFAND_TMP" "com.labolado.bgfxdemo.official" "$OFFICIAL_CB")
if [ -n "$OFFAND_APK" ] && [ -f "$OFFAND_APK" ]; then
    cp "$OFFAND_APK" "$OUT_DIR/official-android.apk"
    log "✅ official-android.apk ($(ls -lh "$OUT_DIR/official-android.apk" | awk '{print $5}'))"
fi
rm -rf "$OFFAND_TMP"

# ═══════════════════════════════════════════════════════════
# Build 5: official-ios IPA
# ═══════════════════════════════════════════════════════════
log "=== 5/5: official-ios IPA ==="
OFFIOS_TMP=$(mktemp -d "/tmp/bgfx-demo-offios-XXXX")
cp -r "$BGFX_DEMO/." "$OFFIOS_TMP/"
printf 'perf' > "$OFFIOS_TMP/solar2d_test.txt"

OFFIOS_IPA=$(build_ios_cb "$OFFIOS_TMP" "com.labolado.bgfxdemo.official" "$OFFICIAL_CB")
if [ -n "$OFFIOS_IPA" ] && [ -f "$OFFIOS_IPA" ]; then
    cp "$OFFIOS_IPA" "$OUT_DIR/official-ios.ipa"
    log "✅ official-ios.ipa ($(ls -lh "$OUT_DIR/official-ios.ipa" | awk '{print $5}'))"
fi
rm -rf "$OFFIOS_TMP"

# ═══════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════
rm -f "$MARKER"
echo "" | tee -a "$LOG"
log "=== Summary ==="
PASS=0; FAIL=0
for f in bgfx-vulkan.apk bgfx-gles.apk bgfx-metal.ipa official-android.apk official-ios.ipa; do
    if [ -f "$OUT_DIR/$f" ]; then
        log "  ✅ $f ($(ls -lh "$OUT_DIR/$f" | awk '{print $5}'))"
        ((PASS++))
    else
        log "  ❌ $f MISSING"
        ((FAIL++))
    fi
done
log "Results: $PASS/5 built   Log: $LOG"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
