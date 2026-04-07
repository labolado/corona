#!/bin/bash
# Matrix test script: run bgfx demo on multiple iOS simulators + Android emulators
# Usage: bash tests/run_matrix_test.sh [ios|android|all]
#
# Prerequisites:
# - macOS build: xcodebuild ... -sdk iphonesimulator
# - Android build: gradlew assembleDebug
# - gemma4-ask in PATH (for screenshot analysis)

set -e
CORONA_DIR="/Users/yee/data/dev/app/labo/corona"
RESULTS_DIR="/tmp/solar2d_matrix_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

# Test modes to run
TEST_MODES=("" "stress_api")  # default demo + stress API test

# iOS Simulator devices to test
IOS_DEVICES=(
    "iPhone 17 Pro"
    "iPhone 16e"
    "iPad Pro 13-inch (M5)"
    "iPad mini (A17 Pro)"
)

# Android emulator AVDs to test
ANDROID_AVDS=(
    "bgfx_api24"
    "bgfx_api34"
)

log() {
    echo "[$(date '+%H:%M:%S')] $1" | tee -a "$RESULTS_DIR/test.log"
}

# ============================================================
# iOS Simulator Testing
# ============================================================
run_ios_tests() {
    log "=== iOS Simulator Tests ==="

    local APP="$CORONA_DIR/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
    local SIM_APP="$CORONA_DIR/platform/iphone/build/Debug-iphonesimulator/template.app"

    # Check if simulator app exists
    if [ ! -d "$SIM_APP" ]; then
        log "ERROR: iOS Simulator app not found. Build first:"
        log "  xcodebuild -project platform/iphone/ratatouille.xcodeproj -target template -configuration Debug -sdk iphonesimulator -arch arm64"
        return 1
    fi

    for device in "${IOS_DEVICES[@]}"; do
        log "--- Testing: $device ---"
        local device_dir="$RESULTS_DIR/ios_$(echo "$device" | tr ' ()' '___')"
        mkdir -p "$device_dir"

        # Boot simulator
        local udid=$(xcrun simctl list devices available | grep "$device" | grep -oE '[A-F0-9-]{36}' | head -1)
        if [ -z "$udid" ]; then
            log "  SKIP: Device '$device' not found"
            continue
        fi

        xcrun simctl boot "$udid" 2>/dev/null || true
        sleep 3

        for test_mode in "${TEST_MODES[@]}"; do
            local test_name="${test_mode:-default_demo}"
            log "  Running test: $test_name"

            # Uninstall previous
            xcrun simctl uninstall "$udid" com.coronalabs.template 2>/dev/null || true

            # Install
            xcrun simctl install "$udid" "$SIM_APP"

            # Launch with environment
            xcrun simctl terminate "$udid" com.coronalabs.template 2>/dev/null || true
            sleep 1

            if [ -n "$test_mode" ]; then
                SIMCTL_CHILD_SOLAR2D_BACKEND=bgfx SIMCTL_CHILD_SOLAR2D_TEST="$test_mode" \
                    xcrun simctl launch "$udid" com.coronalabs.template 2>/dev/null
            else
                SIMCTL_CHILD_SOLAR2D_BACKEND=bgfx \
                    xcrun simctl launch "$udid" com.coronalabs.template 2>/dev/null
            fi

            # Wait for test to run
            if [ "$test_mode" = "stress_api" ]; then
                sleep 60  # stress test takes longer
            else
                sleep 8
            fi

            # Check for crashes
            local crash_check=$(xcrun simctl spawn "$udid" log show --predicate 'process == "template"' --last 30s --style compact 2>/dev/null | grep -ci "crash\|fatal\|abort\|signal" || true)

            # Screenshot
            local screenshot="$device_dir/${test_name}_bgfx.png"
            xcrun simctl io "$udid" screenshot "$screenshot" 2>/dev/null

            # Get logs
            xcrun simctl spawn "$udid" log show --predicate 'process == "template"' --last 30s --style compact 2>/dev/null > "$device_dir/${test_name}_bgfx.log" || true

            # Check results
            if [ "$crash_check" -gt 0 ]; then
                log "  [$test_name] CRASH detected! See $device_dir/${test_name}_bgfx.log"
                echo "CRASH" > "$device_dir/${test_name}_bgfx.result"
            elif [ -f "$screenshot" ]; then
                log "  [$test_name] Screenshot captured: $screenshot"

                # gemma4 analysis
                if command -v gemma4-ask &>/dev/null; then
                    local analysis=$(gemma4-ask "Analyze this Solar2D rendering test screenshot. Check for: 1) Any rendering errors (missing shapes, wrong colors, artifacts) 2) Is the FPS display visible at top? 3) Are shapes/text rendering correctly? 4) Any black screen or blank areas that shouldn't be there? Report PASS if everything looks normal, or FAIL with specific issues." "$screenshot" 2>/dev/null || echo "gemma4 unavailable")
                    echo "$analysis" > "$device_dir/${test_name}_bgfx.analysis"
                    log "  [$test_name] gemma4: $(echo "$analysis" | head -1)"
                fi

                echo "OK" > "$device_dir/${test_name}_bgfx.result"
            else
                log "  [$test_name] No screenshot captured"
                echo "NO_SCREENSHOT" > "$device_dir/${test_name}_bgfx.result"
            fi

            xcrun simctl terminate "$udid" com.coronalabs.template 2>/dev/null || true
        done

        # Shutdown non-primary simulators to save resources
        if [ "$device" != "iPhone 17 Pro" ]; then
            xcrun simctl shutdown "$udid" 2>/dev/null || true
        fi
    done
}

# ============================================================
# Android Emulator Testing
# ============================================================
run_android_tests() {
    log "=== Android Emulator Tests ==="

    local APK="$CORONA_DIR/platform/android/app/build/outputs/apk/debug/app-debug.apk"

    if [ ! -f "$APK" ]; then
        log "ERROR: Android APK not found. Build first."
        return 1
    fi

    for avd in "${ANDROID_AVDS[@]}"; do
        log "--- Testing: $avd ---"
        local device_dir="$RESULTS_DIR/android_$avd"
        mkdir -p "$device_dir"

        # Check if AVD exists
        if ! ~/Library/Android/sdk/emulator/emulator -list-avds 2>/dev/null | grep -q "$avd"; then
            log "  SKIP: AVD '$avd' not found"
            continue
        fi

        # Start emulator
        ~/Library/Android/sdk/emulator/emulator -avd "$avd" -no-window -no-audio -no-boot-anim -gpu swiftshader_indirect &
        local EMU_PID=$!

        # Wait for boot
        log "  Waiting for emulator boot..."
        adb wait-for-device 2>/dev/null
        local boot_complete=""
        for i in $(seq 1 60); do
            boot_complete=$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')
            if [ "$boot_complete" = "1" ]; then
                break
            fi
            sleep 2
        done

        if [ "$boot_complete" != "1" ]; then
            log "  SKIP: Emulator failed to boot"
            kill $EMU_PID 2>/dev/null
            continue
        fi
        log "  Emulator booted"

        # Install APK
        adb install -r "$APK" 2>/dev/null

        for test_mode in "${TEST_MODES[@]}"; do
            local test_name="${test_mode:-default_demo}"
            log "  Running test: $test_name"

            adb shell am force-stop com.corona.app 2>/dev/null
            adb logcat -c 2>/dev/null
            sleep 1

            # Launch
            adb shell monkey -p com.corona.app -c android.intent.category.LAUNCHER 1 2>/dev/null

            # Wait
            if [ "$test_mode" = "stress_api" ]; then
                sleep 60
            else
                sleep 10
            fi

            # Check for crashes
            local crash_log=$(adb logcat -d 2>/dev/null | grep -iE "FATAL|signal.*SIGABRT|signal.*SIGSEGV" | head -5)

            # Screenshot
            local screenshot="$device_dir/${test_name}_bgfx.png"
            adb exec-out screencap -p > "$screenshot" 2>/dev/null

            # Get logs
            adb logcat -d 2>/dev/null | grep -iE "Corona|bgfx|Backend|FPS|error|fatal" > "$device_dir/${test_name}_bgfx.log"

            # Backend verification
            local backend=$(adb logcat -d 2>/dev/null | grep "Backend:" | tail -1)
            log "  [$test_name] $backend"

            if [ -n "$crash_log" ]; then
                log "  [$test_name] CRASH: $crash_log"
                echo "CRASH" > "$device_dir/${test_name}_bgfx.result"
            elif [ -f "$screenshot" ] && [ -s "$screenshot" ]; then
                # gemma4 analysis
                if command -v gemma4-ask &>/dev/null; then
                    local analysis=$(gemma4-ask "Analyze this Android Solar2D rendering test. Check for rendering errors, missing shapes, artifacts, black screen. Report PASS or FAIL with details." "$screenshot" 2>/dev/null || echo "gemma4 unavailable")
                    echo "$analysis" > "$device_dir/${test_name}_bgfx.analysis"
                    log "  [$test_name] gemma4: $(echo "$analysis" | head -1)"
                fi
                echo "OK" > "$device_dir/${test_name}_bgfx.result"
            else
                log "  [$test_name] No screenshot"
                echo "NO_SCREENSHOT" > "$device_dir/${test_name}_bgfx.result"
            fi
        done

        # Shutdown emulator
        adb emu kill 2>/dev/null
        wait $EMU_PID 2>/dev/null
        log "  Emulator shutdown"
    done
}

# ============================================================
# Summary Report
# ============================================================
generate_report() {
    log ""
    log "============================================"
    log "         TEST MATRIX RESULTS"
    log "============================================"

    local total=0
    local passed=0
    local failed=0
    local crashed=0

    for result_file in "$RESULTS_DIR"/*/*.result; do
        if [ ! -f "$result_file" ]; then continue; fi
        total=$((total + 1))
        local result=$(cat "$result_file")
        local test_path=$(dirname "$result_file" | xargs basename)
        local test_name=$(basename "$result_file" .result)

        case "$result" in
            OK) passed=$((passed + 1)); log "  PASS: $test_path / $test_name" ;;
            CRASH) crashed=$((crashed + 1)); log "  CRASH: $test_path / $test_name" ;;
            *) failed=$((failed + 1)); log "  FAIL: $test_path / $test_name ($result)" ;;
        esac
    done

    log ""
    log "Total: $total | Pass: $passed | Fail: $failed | Crash: $crashed"
    log "Results: $RESULTS_DIR"
    log "============================================"
}

# ============================================================
# Main
# ============================================================
MODE="${1:-all}"

case "$MODE" in
    ios) run_ios_tests ;;
    android) run_android_tests ;;
    all) run_ios_tests; run_android_tests ;;
    *) echo "Usage: $0 [ios|android|all]" ;;
esac

generate_report
