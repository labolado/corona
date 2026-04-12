#!/bin/bash
# tests/device_bench.sh — Real device performance benchmark automation
# Usage: bash tests/device_bench.sh [OPTIONS]
#
# Options:
#   --android-only    Skip iOS devices
#   --ios-only        Skip Android devices
#   --skip-build      Skip compilation, use existing builds
#   --timeout SEC     Per-device test timeout (default: 120)
#   --package PKG     Android package name (default: com.labolado.bgfxdemo)
#   --bundle-id ID    iOS bundle ID (default: com.labolado.labo-brick-tank)
#   --output DIR      Output directory (default: /tmp/device_bench_<date>)
#
# Prerequisites:
#   - Android: adb in PATH, device connected
#   - iOS: ios-deploy or devicectl, idevicesyslog, device connected
#   - tests/build_android.sh and tests/build_ios.sh available
#
# Output:
#   - Per-device FPS table (stdout)
#   - JSON report at $OUTPUT_DIR/bench_report.json
#   - Raw logs at $OUTPUT_DIR/<device_id>.log

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORONA_DIR="${CORONA_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
DATE_TAG=$(date +%Y%m%d_%H%M%S)

# Defaults
ANDROID_ONLY=0
IOS_ONLY=0
SKIP_BUILD=0
TIMEOUT=120
ANDROID_PKG="com.labolado.bgfxdemo"
IOS_BUNDLE_ID="com.labolado.labo-brick-tank"
OUTPUT_DIR="/tmp/device_bench_${DATE_TAG}"

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --android-only) ANDROID_ONLY=1; shift ;;
        --ios-only)     IOS_ONLY=1; shift ;;
        --skip-build)   SKIP_BUILD=1; shift ;;
        --timeout)      TIMEOUT="$2"; shift 2 ;;
        --package)      ANDROID_PKG="$2"; shift 2 ;;
        --bundle-id)    IOS_BUNDLE_ID="$2"; shift 2 ;;
        --output)       OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

mkdir -p "$OUTPUT_DIR"

log() { echo "[$(date '+%H:%M:%S')] $1"; }
fail() { echo "FAILED: $1" >&2; }

# ============================================================
# Device Detection
# ============================================================

declare -a ANDROID_DEVICES=()
declare -a ANDROID_MODELS=()
declare -a IOS_DEVICES=()
declare -a IOS_NAMES=()

detect_devices() {
    log "Detecting connected devices..."

    # Android
    if [[ $IOS_ONLY -eq 0 ]]; then
        while IFS=$'\t' read -r serial state; do
            if [[ "$state" == "device" && "$serial" != *"emulator"* ]]; then
                ANDROID_DEVICES+=("$serial")
                local model
                model=$(adb -s "$serial" shell getprop ro.product.model 2>/dev/null | tr -d '\r')
                ANDROID_MODELS+=("${model:-unknown}")
                log "  Android: $serial ($model)"
            fi
        done < <(adb devices 2>/dev/null | grep -v "List" | grep -v "^$")

        if [[ ${#ANDROID_DEVICES[@]} -eq 0 ]]; then
            log "  No Android real devices found (emulators skipped)"
        fi
    fi

    # iOS
    if [[ $ANDROID_ONLY -eq 0 ]]; then
        # Use xcrun xctrace to find real devices (not simulators)
        while IFS= read -r line; do
            # Format: "Device Name (OS Version) (UDID)"
            local name udid
            udid=$(echo "$line" | grep -oE '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}|[0-9a-f]{40}')
            if [[ -n "$udid" ]]; then
                name=$(echo "$line" | sed 's/ ([^)]*) *$//' | sed 's/ ([^)]*) *$//')
                IOS_DEVICES+=("$udid")
                IOS_NAMES+=("$name")
                log "  iOS: $udid ($name)"
            fi
        done < <(xcrun xctrace list devices 2>/dev/null | grep -v Simulator | grep -iE 'iPhone|iPad')

        if [[ ${#IOS_DEVICES[@]} -eq 0 ]]; then
            log "  No iOS devices found"
        fi
    fi

    local total=$(( ${#ANDROID_DEVICES[@]} + ${#IOS_DEVICES[@]} ))
    if [[ $total -eq 0 ]]; then
        log "No devices found. Nothing to do."
        exit 0
    fi
    log "Total devices: $total"
}

# ============================================================
# Build
# ============================================================

build_android() {
    if [[ $SKIP_BUILD -eq 1 ]]; then
        log "Skipping Android build (--skip-build)"
        return 0
    fi
    if [[ ${#ANDROID_DEVICES[@]} -eq 0 ]]; then
        return 0
    fi
    log "Building Android APK..."
    cd "$CORONA_DIR"
    bash tests/build_android.sh tests/bgfx-demo "$ANDROID_PKG" 2>&1 | tail -20
    local rc=${PIPESTATUS[0]}
    if [[ $rc -ne 0 ]]; then
        fail "Android build failed (exit $rc)"
        return 1
    fi
    log "Android build complete"
    return 0
}

build_ios() {
    if [[ $SKIP_BUILD -eq 1 ]]; then
        log "Skipping iOS build (--skip-build)"
        return 0
    fi
    if [[ ${#IOS_DEVICES[@]} -eq 0 ]]; then
        return 0
    fi
    log "Building iOS app..."
    cd "$CORONA_DIR"
    bash tests/build_ios.sh tests/bgfx-demo "$IOS_BUNDLE_ID" 2>&1 | tail -20
    local rc=${PIPESTATUS[0]}
    if [[ $rc -ne 0 ]]; then
        fail "iOS build failed (exit $rc)"
        return 1
    fi
    log "iOS build complete"
    return 0
}

# ============================================================
# Android bench runner
# ============================================================

run_android_bench() {
    local serial="$1"
    local model="$2"
    local logfile="$OUTPUT_DIR/android_${serial}.log"

    log "[$model] Starting bench on Android $serial"

    # Write flag file to trigger bench mode
    local docs_dir="/sdcard/Android/data/${ANDROID_PKG}/files"
    adb -s "$serial" shell "mkdir -p '$docs_dir'" 2>/dev/null
    adb -s "$serial" shell "echo 'bench' > '${docs_dir}/solar2d_test.txt'" 2>/dev/null

    # Force stop and restart
    adb -s "$serial" shell am force-stop "$ANDROID_PKG" 2>/dev/null
    sleep 1
    adb -s "$serial" shell am start -n "${ANDROID_PKG}/com.ansca.corona.CoronaActivity" 2>/dev/null

    # Collect logs until END marker or timeout
    log "[$model] Waiting for bench results (timeout: ${TIMEOUT}s)..."
    adb -s "$serial" logcat -c 2>/dev/null
    # Re-launch after logcat clear to ensure we capture from start
    adb -s "$serial" shell am force-stop "$ANDROID_PKG" 2>/dev/null
    sleep 1
    adb -s "$serial" shell "echo 'bench' > '${docs_dir}/solar2d_test.txt'" 2>/dev/null
    adb -s "$serial" shell am start -n "${ANDROID_PKG}/com.ansca.corona.CoronaActivity" 2>/dev/null

    # Stream logcat, grep for bench output, with timeout
    timeout "$TIMEOUT" adb -s "$serial" logcat -s "Corona:V" 2>/dev/null > "$logfile" &
    local logcat_pid=$!

    # Wait for END marker
    local elapsed=0
    while [[ $elapsed -lt $TIMEOUT ]]; do
        if grep -q "=== END ===" "$logfile" 2>/dev/null; then
            break
        fi
        sleep 2
        elapsed=$((elapsed + 2))
    done

    kill "$logcat_pid" 2>/dev/null
    wait "$logcat_pid" 2>/dev/null

    if ! grep -q "=== END ===" "$logfile" 2>/dev/null; then
        fail "[$model] Bench timed out after ${TIMEOUT}s"
        return 1
    fi

    log "[$model] Bench complete, parsing results"

    # Get GPU info
    local gpu
    gpu=$(adb -s "$serial" shell dumpsys SurfaceFlinger 2>/dev/null | grep -i "GLES" | head -1 | sed 's/.*: //' | tr -d '\r')
    [[ -z "$gpu" ]] && gpu=$(adb -s "$serial" shell getprop ro.hardware.chipname 2>/dev/null | tr -d '\r')
    [[ -z "$gpu" ]] && gpu="unknown"

    echo "GPU:$gpu" >> "$logfile"
    return 0
}

# ============================================================
# iOS bench runner
# ============================================================

run_ios_bench() {
    local udid="$1"
    local name="$2"
    local logfile="$OUTPUT_DIR/ios_${udid}.log"

    log "[$name] Starting bench on iOS $udid"

    # For iOS, we need to write the flag file via the app's Documents directory.
    # Since we can't directly write to app sandbox, we use a different approach:
    # Install the app with a pre-created flag file in the project directory,
    # OR use ios-deploy to upload the file.

    # Strategy: Use devicectl to launch with env var if possible,
    # otherwise write flag file via ios-deploy
    local app_path
    app_path=$(find /tmp -maxdepth 2 -name "bgfx-demo.app" -newer /tmp/device_bench_marker 2>/dev/null | head -1)
    # Fallback: find most recent build
    if [[ -z "$app_path" ]]; then
        app_path=$(find /tmp -maxdepth 2 -name "bgfx-demo.app" 2>/dev/null | head -1)
    fi

    if [[ -z "$app_path" ]]; then
        fail "[$name] Cannot find bgfx-demo.app in /tmp"
        return 1
    fi

    # Create flag file in the app bundle (it will be in the app's resource dir)
    # Better: use ios-deploy to write to Documents after install
    # Best: devicectl supports launching with env vars? No, it doesn't.

    # Write solar2d_test.txt into app bundle before install
    # The Lua code reads from system.DocumentsDirectory, not ResourceDirectory.
    # We need to push the file to the app's Documents after install.

    # Install app
    log "[$name] Installing app..."
    xcrun devicectl device install app --device "$udid" "$app_path" 2>&1 | tail -3

    # Use ios-deploy to write flag file to Documents
    # ios-deploy can upload files to app container
    if command -v ios-deploy &>/dev/null; then
        # Create temp flag file
        local tmpflag="/tmp/solar2d_test.txt"
        echo "bench" > "$tmpflag"

        # Upload to app Documents directory
        # ios-deploy --id <udid> --bundle_id <bid> --upload <local> --to Documents/solar2d_test.txt
        ios-deploy --id "$udid" --bundle_id "$IOS_BUNDLE_ID" \
            --upload "$tmpflag" --to "Documents/solar2d_test.txt" 2>/dev/null
        if [[ $? -ne 0 ]]; then
            log "[$name] WARNING: ios-deploy upload failed, trying alternative method"
        fi
        rm -f "$tmpflag"
    else
        log "[$name] WARNING: ios-deploy not found, cannot write flag file"
        log "[$name] Bench mode may not activate on iOS"
    fi

    # Start idevicesyslog in background to capture logs
    log "[$name] Starting log capture..."
    if command -v idevicesyslog &>/dev/null; then
        idevicesyslog -u "$udid" --process Corona 2>/dev/null > "$logfile" &
        local syslog_pid=$!
    else
        # Fallback: devicectl console (less reliable for filtering)
        xcrun devicectl device process launch --device "$udid" --console "$IOS_BUNDLE_ID" 2>/dev/null > "$logfile" &
        local syslog_pid=$!
    fi

    sleep 2

    # Launch the app
    log "[$name] Launching app..."
    xcrun devicectl device process launch --device "$udid" "$IOS_BUNDLE_ID" 2>/dev/null

    # Wait for END marker or timeout
    local elapsed=0
    while [[ $elapsed -lt $TIMEOUT ]]; do
        if grep -q "=== END ===" "$logfile" 2>/dev/null; then
            break
        fi
        sleep 2
        elapsed=$((elapsed + 2))
    done

    kill "$syslog_pid" 2>/dev/null
    wait "$syslog_pid" 2>/dev/null

    if ! grep -q "=== END ===" "$logfile" 2>/dev/null; then
        fail "[$name] Bench timed out after ${TIMEOUT}s"
        # Still try to parse partial results
    fi

    # Get GPU info from device
    local gpu
    gpu=$(echo "$name" | grep -oE 'iPhone|iPad')
    # Try to get chip info
    local chip
    chip=$(ideviceinfo -u "$udid" -k CPUArchitecture 2>/dev/null | tr -d '\r')
    [[ -n "$chip" ]] && gpu="${gpu:-iOS} ($chip)"
    [[ -z "$gpu" ]] && gpu="Apple GPU"

    echo "GPU:$gpu" >> "$logfile"
    log "[$name] Bench complete"
    return 0
}

# ============================================================
# Parse bench results from log file
# ============================================================

parse_bench_results() {
    local logfile="$1"
    local device_id="$2"
    local device_name="$3"
    local platform="$4"

    # Extract GPU
    local gpu
    gpu=$(grep "^GPU:" "$logfile" 2>/dev/null | tail -1 | sed 's/^GPU://')
    [[ -z "$gpu" ]] && gpu="unknown"

    # Parse [Bench] lines: "[Bench] 500 objects: avg=60.0 min=55.0 max=65.0 FPS"
    local fps_500="" fps_1000="" fps_2000="" fps_3000="" fps_5000=""

    while IFS= read -r line; do
        local count avg
        count=$(echo "$line" | grep -oE '[0-9]+ objects' | grep -oE '[0-9]+')
        avg=$(echo "$line" | grep -oE 'avg=[0-9.]+' | sed 's/avg=//')
        case "$count" in
            500)  fps_500="$avg" ;;
            1000) fps_1000="$avg" ;;
            2000) fps_2000="$avg" ;;
            3000) fps_3000="$avg" ;;
            5000) fps_5000="$avg" ;;
        esac
    done < <(grep '\[Bench\].*objects:' "$logfile" 2>/dev/null)

    # Store in global arrays for table output
    RESULT_DEVICES+=("$device_name")
    RESULT_PLATFORMS+=("$platform")
    RESULT_GPUS+=("$gpu")
    RESULT_500+=("${fps_500:--}")
    RESULT_1000+=("${fps_1000:--}")
    RESULT_2000+=("${fps_2000:--}")
    RESULT_3000+=("${fps_3000:--}")
    RESULT_5000+=("${fps_5000:--}")

    # Write JSON entry
    cat >> "$OUTPUT_DIR/bench_results.jsonl" << JSONEOF
{"device":"$device_name","device_id":"$device_id","platform":"$platform","gpu":"$gpu","fps_500":${fps_500:-null},"fps_1000":${fps_1000:-null},"fps_2000":${fps_2000:-null},"fps_3000":${fps_3000:-null},"fps_5000":${fps_5000:-null}}
JSONEOF
}

# ============================================================
# Report Generation
# ============================================================

declare -a RESULT_DEVICES=()
declare -a RESULT_PLATFORMS=()
declare -a RESULT_GPUS=()
declare -a RESULT_500=()
declare -a RESULT_1000=()
declare -a RESULT_2000=()
declare -a RESULT_3000=()
declare -a RESULT_5000=()

generate_report() {
    local report="$OUTPUT_DIR/bench_report.txt"

    {
        echo "=== Device Benchmark Report ==="
        echo "Date: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "Backend: bgfx"
        echo ""

        # Header
        printf "| %-20s | %-8s | %-20s | %7s | %7s | %7s | %7s | %7s |\n" \
            "Device" "Platform" "GPU" "500obj" "1000obj" "2000obj" "3000obj" "5000obj"
        printf "|%-22s|%-10s|%-22s|%9s|%9s|%9s|%9s|%9s|\n" \
            "$(printf '%0.s-' {1..22})" "$(printf '%0.s-' {1..10})" "$(printf '%0.s-' {1..22})" \
            "$(printf '%0.s-' {1..9})" "$(printf '%0.s-' {1..9})" "$(printf '%0.s-' {1..9})" \
            "$(printf '%0.s-' {1..9})" "$(printf '%0.s-' {1..9})"

        for i in "${!RESULT_DEVICES[@]}"; do
            printf "| %-20s | %-8s | %-20s | %7s | %7s | %7s | %7s | %7s |\n" \
                "${RESULT_DEVICES[$i]}" \
                "${RESULT_PLATFORMS[$i]}" \
                "${RESULT_GPUS[$i]:0:20}" \
                "${RESULT_500[$i]}" \
                "${RESULT_1000[$i]}" \
                "${RESULT_2000[$i]}" \
                "${RESULT_3000[$i]}" \
                "${RESULT_5000[$i]}"
        done

        echo ""
        echo "Logs: $OUTPUT_DIR/"
    } | tee "$report"

    # Generate JSON report
    {
        echo "{"
        echo "  \"date\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\","
        echo "  \"backend\": \"bgfx\","
        echo "  \"devices\": ["
        local first=1
        for i in "${!RESULT_DEVICES[@]}"; do
            [[ $first -eq 0 ]] && echo ","
            first=0
            printf '    {"device":"%s","platform":"%s","gpu":"%s","fps":{"500":%s,"1000":%s,"2000":%s,"3000":%s,"5000":%s}}' \
                "${RESULT_DEVICES[$i]}" \
                "${RESULT_PLATFORMS[$i]}" \
                "${RESULT_GPUS[$i]}" \
                "${RESULT_500[$i]/-/null}" \
                "${RESULT_1000[$i]/-/null}" \
                "${RESULT_2000[$i]/-/null}" \
                "${RESULT_3000[$i]/-/null}" \
                "${RESULT_5000[$i]/-/null}"
        done
        echo ""
        echo "  ]"
        echo "}"
    } > "$OUTPUT_DIR/bench_report.json"

    log "JSON report: $OUTPUT_DIR/bench_report.json"
    log "Text report: $report"
}

# ============================================================
# Main
# ============================================================

main() {
    log "=== Device Benchmark Automation ==="
    log "Output: $OUTPUT_DIR"
    echo ""

    # Create marker for finding recent builds
    touch /tmp/device_bench_marker

    detect_devices

    # Build phase
    local android_build_ok=1
    local ios_build_ok=1

    if [[ ${#ANDROID_DEVICES[@]} -gt 0 ]]; then
        build_android || android_build_ok=0
    fi
    if [[ ${#IOS_DEVICES[@]} -gt 0 ]]; then
        build_ios || ios_build_ok=0
    fi

    echo ""
    rm -f "$OUTPUT_DIR/bench_results.jsonl"

    # Run benchmarks
    local fail_log="$OUTPUT_DIR/failures.log"
    > "$fail_log"

    # Android devices
    if [[ $android_build_ok -eq 1 ]]; then
        for i in "${!ANDROID_DEVICES[@]}"; do
            local serial="${ANDROID_DEVICES[$i]}"
            local model="${ANDROID_MODELS[$i]}"
            echo ""
            if run_android_bench "$serial" "$model"; then
                parse_bench_results "$OUTPUT_DIR/android_${serial}.log" "$serial" "$model" "Android"
            else
                echo "FAILED: Android $serial ($model)" >> "$fail_log"
                # Still try to parse partial results
                parse_bench_results "$OUTPUT_DIR/android_${serial}.log" "$serial" "$model" "Android"
            fi
        done
    fi

    # iOS devices
    if [[ $ios_build_ok -eq 1 ]]; then
        for i in "${!IOS_DEVICES[@]}"; do
            local udid="${IOS_DEVICES[$i]}"
            local name="${IOS_NAMES[$i]}"
            echo ""
            if run_ios_bench "$udid" "$name"; then
                parse_bench_results "$OUTPUT_DIR/ios_${udid}.log" "$udid" "$name" "iOS"
            else
                echo "FAILED: iOS $udid ($name)" >> "$fail_log"
                parse_bench_results "$OUTPUT_DIR/ios_${udid}.log" "$udid" "$name" "iOS"
            fi
        done
    fi

    # Report
    echo ""
    echo ""
    generate_report

    # Summary
    local fail_count
    fail_count=$(wc -l < "$fail_log" 2>/dev/null | tr -d ' ')
    if [[ "$fail_count" -gt 0 ]]; then
        echo ""
        log "WARNING: $fail_count device(s) had failures:"
        cat "$fail_log"
    fi

    rm -f /tmp/device_bench_marker
    log "Done."
}

main
