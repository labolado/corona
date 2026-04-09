#!/bin/bash
# Android multi-version compatibility test
# Usage: bash tests/android_compat_test.sh [api24|api34|all]
# Prerequisites: Android SDK, adb, emulator AVDs (bgfx_api24, bgfx_api34)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORONA_DIR="${CORONA_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
EMULATOR="$HOME/Library/Android/sdk/emulator/emulator"
ADB="$(which adb 2>/dev/null || echo "$HOME/Library/Android/sdk/platform-tools/adb")"
PACKAGE="com.labolado.test.compat"
ACTIVITY="com.ansca.corona.CoronaActivity"
RESULT_DIR="/tmp/android_compat_$(date +%Y%m%d_%H%M%S)"
TIMEOUT=120  # seconds to wait for test completion
TARGET="${1:-all}"

log() { echo "[$(date '+%H:%M:%S')] $1"; }
fail() { echo "FAILED: $1"; exit 1; }

# ============================================================
# Step 0: Validate environment
# ============================================================
log "Step 0: Validate environment"
[ -f "$ADB" ] || [ -x "$(which adb 2>/dev/null)" ] || fail "adb not found"
[ -f "$EMULATOR" ] || fail "emulator not found at $EMULATOR"

mkdir -p "$RESULT_DIR"

# Determine which APIs to test
APIS=()
case "$TARGET" in
    api24) APIS=(24) ;;
    api34) APIS=(34) ;;
    all)   APIS=(24 34) ;;
    *)     fail "Unknown target: $TARGET (use api24, api34, or all)" ;;
esac
log "  Testing APIs: ${APIS[*]}"

# ============================================================
# Step 1: Build APK (if not already built)
# ============================================================
APK_PATH=""
# Check for pre-built APK
for candidate in /tmp/android-build-*/bgfx-demo.apk /tmp/android-build-*/test_android_compat.apk; do
    if [ -f "$candidate" ]; then
        APK_PATH="$candidate"
        break
    fi
done

if [ -z "$APK_PATH" ]; then
    log "Step 1: Build test APK"
    # Create standalone project
    PROJ="/tmp/android_compat_project"
    rm -rf "$PROJ" && mkdir -p "$PROJ"
    cp "$CORONA_DIR/tests/bgfx-demo/test_android_compat.lua" "$PROJ/main.lua"

    # Copy test assets
    for f in test_red.png test_blue.png test_green.png; do
        [ -f "$CORONA_DIR/tests/bgfx-demo/$f" ] && cp "$CORONA_DIR/tests/bgfx-demo/$f" "$PROJ/"
    done

    cat > "$PROJ/config.lua" << 'EOF'
application = {
    content = { width = 480, height = 320, scale = "letterbox", fps = 60 }
}
EOF

    cat > "$PROJ/build.settings" << 'EOF'
settings = {
    orientation = {
        default = "landscapeRight",
        supported = { "landscapeLeft", "landscapeRight" },
    },
    android = {
        usesPermissions = {
            "android.permission.INTERNET",
            "android.permission.WRITE_EXTERNAL_STORAGE",
        },
    },
}
EOF

    # Build using existing script
    if [ -f "$CORONA_DIR/tests/build_android.sh" ]; then
        bash "$CORONA_DIR/tests/build_android.sh" "$PROJ" "$PACKAGE" 2>&1 | tail -20
        # Find built APK
        APK_PATH=$(find /tmp/android-build-* -name "*.apk" -newer "$PROJ/main.lua" 2>/dev/null | head -1)
    fi

    [ -n "$APK_PATH" ] && [ -f "$APK_PATH" ] || fail "APK build failed"
fi

log "  APK: $APK_PATH"

# ============================================================
# Step 2: Run tests on each API level
# ============================================================
run_test_on_emulator() {
    local API=$1
    local AVD="bgfx_api${API}"
    local OUTDIR="$RESULT_DIR/api${API}"
    local EMU_SERIAL=""

    mkdir -p "$OUTDIR"
    log "--- Testing API $API (AVD: $AVD) ---"

    # Check if AVD exists
    if ! "$EMULATOR" -list-avds 2>/dev/null | grep -q "^${AVD}$"; then
        log "  WARNING: AVD '$AVD' not found, skipping"
        echo "SKIP: AVD not found" > "$OUTDIR/result.txt"
        return 1
    fi

    # Start emulator
    log "  Starting emulator..."
    "$EMULATOR" -avd "$AVD" -no-window -no-audio -no-boot-anim -gpu swiftshader_indirect &
    local EMU_PID=$!

    # Wait for boot
    local WAIT=0
    while [ $WAIT -lt 90 ]; do
        if "$ADB" devices 2>/dev/null | grep -q "emulator.*device$"; then
            EMU_SERIAL=$("$ADB" devices 2>/dev/null | grep "emulator.*device$" | head -1 | awk '{print $1}')
            break
        fi
        sleep 2
        WAIT=$((WAIT + 2))
    done

    if [ -z "$EMU_SERIAL" ]; then
        log "  FAIL: Emulator did not boot in 90s"
        kill "$EMU_PID" 2>/dev/null
        echo "FAIL: Emulator boot timeout" > "$OUTDIR/result.txt"
        return 1
    fi
    log "  Emulator ready: $EMU_SERIAL"

    # Wait for full boot
    "$ADB" -s "$EMU_SERIAL" wait-for-device
    "$ADB" -s "$EMU_SERIAL" shell "while [[ -z \$(getprop sys.boot_completed) ]]; do sleep 1; done" 2>/dev/null || sleep 5

    # Install APK
    log "  Installing APK..."
    if ! "$ADB" -s "$EMU_SERIAL" install -r "$APK_PATH" 2>&1 | grep -q "Success"; then
        log "  FAIL: APK install failed"
        kill "$EMU_PID" 2>/dev/null
        echo "FAIL: Install failed" > "$OUTDIR/result.txt"
        return 1
    fi

    # Clear logcat and start app
    "$ADB" -s "$EMU_SERIAL" logcat -c 2>/dev/null
    log "  Starting test app..."
    "$ADB" -s "$EMU_SERIAL" shell am start -n "$PACKAGE/$ACTIVITY" 2>/dev/null

    # Wait for test completion
    log "  Waiting for test completion (max ${TIMEOUT}s)..."
    local ELAPSED=0
    local COMPLETE=0
    while [ $ELAPSED -lt $TIMEOUT ]; do
        if "$ADB" -s "$EMU_SERIAL" logcat -d 2>/dev/null | grep -q "ALL SCENES COMPLETE"; then
            COMPLETE=1
            break
        fi
        sleep 3
        ELAPSED=$((ELAPSED + 3))
    done

    # Save logcat
    "$ADB" -s "$EMU_SERIAL" logcat -d > "$OUTDIR/logcat.txt" 2>/dev/null

    if [ $COMPLETE -eq 1 ]; then
        log "  Test completed!"
    else
        log "  WARNING: Test did not complete within ${TIMEOUT}s"
    fi

    # Pull screenshots
    log "  Pulling screenshots..."
    local APP_DATA="/sdcard/Android/data/$PACKAGE/files"
    "$ADB" -s "$EMU_SERIAL" pull "$APP_DATA/screenshots/" "$OUTDIR/screenshots/" 2>/dev/null || true
    "$ADB" -s "$EMU_SERIAL" pull "$APP_DATA/Documents/screenshots/" "$OUTDIR/screenshots/" 2>/dev/null || true

    # Pull results file
    "$ADB" -s "$EMU_SERIAL" pull "$APP_DATA/Documents/compat_results.txt" "$OUTDIR/compat_results.txt" 2>/dev/null || true

    # Extract test results from logcat
    grep -E "\[(PASS|FAIL|SCREENSHOT|OK)\]" "$OUTDIR/logcat.txt" > "$OUTDIR/test_log.txt" 2>/dev/null || true
    grep "ANDROID COMPAT RESULTS" -A 30 "$OUTDIR/logcat.txt" > "$OUTDIR/result.txt" 2>/dev/null || true

    local PASS_COUNT=$(grep -c "\[OK\]" "$OUTDIR/result.txt" 2>/dev/null || echo "0")
    local FAIL_COUNT=$(grep -c "\[FAIL\]" "$OUTDIR/result.txt" 2>/dev/null || echo "0")
    log "  API $API results: $PASS_COUNT pass, $FAIL_COUNT fail"

    local SS_COUNT=$(find "$OUTDIR/screenshots/" -name "*.png" 2>/dev/null | wc -l | tr -d ' ')
    log "  Screenshots pulled: $SS_COUNT"

    # Cleanup
    log "  Stopping emulator..."
    "$ADB" -s "$EMU_SERIAL" emu kill 2>/dev/null || true
    sleep 2
    kill "$EMU_PID" 2>/dev/null || true

    return 0
}

for API in "${APIS[@]}"; do
    run_test_on_emulator "$API" || true
done

# ============================================================
# Step 3: Cross-version comparison (if both APIs tested)
# ============================================================
if [ ${#APIS[@]} -eq 2 ]; then
    log "Step 3: Cross-version screenshot comparison"
    COMPARE_DIR="$RESULT_DIR/comparison"
    mkdir -p "$COMPARE_DIR"

    DIR_A="$RESULT_DIR/api${APIS[0]}/screenshots"
    DIR_B="$RESULT_DIR/api${APIS[1]}/screenshots"

    if [ -d "$DIR_A" ] && [ -d "$DIR_B" ]; then
        DIFF_COUNT=0
        TOTAL=0

        for img_a in "$DIR_A"/*.png; do
            [ -f "$img_a" ] || continue
            local_name=$(basename "$img_a")
            img_b="$DIR_B/$local_name"
            TOTAL=$((TOTAL + 1))

            if [ ! -f "$img_b" ]; then
                echo "MISSING in API ${APIS[1]}: $local_name" >> "$COMPARE_DIR/report.txt"
                DIFF_COUNT=$((DIFF_COUNT + 1))
                continue
            fi

            # Pixel comparison using sips + diff
            SIZE_A=$(stat -f "%z" "$img_a" 2>/dev/null || echo "0")
            SIZE_B=$(stat -f "%z" "$img_b" 2>/dev/null || echo "0")

            if command -v compare >/dev/null 2>&1; then
                # ImageMagick available
                METRIC=$(compare -metric AE "$img_a" "$img_b" "$COMPARE_DIR/diff_$local_name" 2>&1 || true)
                if [ "$METRIC" != "0" ] && [ -n "$METRIC" ]; then
                    echo "DIFF ($METRIC px): $local_name" >> "$COMPARE_DIR/report.txt"
                    DIFF_COUNT=$((DIFF_COUNT + 1))
                else
                    echo "MATCH: $local_name" >> "$COMPARE_DIR/report.txt"
                fi
            else
                # Fallback: file size comparison
                if [ "$SIZE_A" != "$SIZE_B" ]; then
                    echo "SIZE_DIFF: $local_name (${SIZE_A} vs ${SIZE_B})" >> "$COMPARE_DIR/report.txt"
                    DIFF_COUNT=$((DIFF_COUNT + 1))
                else
                    echo "SIZE_MATCH: $local_name" >> "$COMPARE_DIR/report.txt"
                fi
            fi
        done

        log "  Comparison: $TOTAL images, $DIFF_COUNT differences"
        [ -f "$COMPARE_DIR/report.txt" ] && cat "$COMPARE_DIR/report.txt"
    else
        log "  Skipping comparison (missing screenshot dirs)"
    fi
fi

# ============================================================
# Summary
# ============================================================
echo ""
echo "============================================"
echo "  Android Compat Test Complete"
echo "  Results: $RESULT_DIR"
for API in "${APIS[@]}"; do
    OUTDIR="$RESULT_DIR/api${API}"
    if [ -f "$OUTDIR/result.txt" ]; then
        PASS=$(grep -c "\[OK\]" "$OUTDIR/result.txt" 2>/dev/null || echo "?")
        FAIL=$(grep -c "\[FAIL\]" "$OUTDIR/result.txt" 2>/dev/null || echo "?")
        echo "  API $API: $PASS pass, $FAIL fail"
    else
        echo "  API $API: no results"
    fi
done
echo "============================================"
