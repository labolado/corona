#!/bin/bash
# Full regression test: GL vs bgfx, Debug vs Release
# Usage: bash tests/run_all_tests.sh [debug|release|all]

set -e
cd "$(dirname "$0")/.."

MODE="${1:-all}"
DEMO="tests/bgfx-demo"
RESULTS_DIR="/tmp/solar2d_regression_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

DEBUG_SIM="./platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
RELEASE_SIM="./platform/mac/build/Release/Corona Simulator.app/Contents/MacOS/Corona Simulator"

capture_screenshot() {
    local output="$1"
    python3 << PYEOF
import Quartz
from Cocoa import NSBitmapImageRep
wl = Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID)
for w in wl:
    if 'Corona' in str(w.get('kCGWindowOwnerName', '')):
        wid = w['kCGWindowNumber']
        img = Quartz.CGWindowListCreateImage(Quartz.CGRectNull, Quartz.kCGWindowListOptionIncludingWindow, wid, Quartz.kCGWindowImageDefault)
        if img:
            rep = NSBitmapImageRep.alloc().initWithCGImage_(img)
            data = rep.representationUsingType_properties_(4, None)
            data.writeToFile_atomically_('${output}', True)
        break
PYEOF
}

run_test() {
    local config="$1"    # debug or release
    local backend="$2"   # gl or bgfx
    local test="$3"      # regression or bench
    local sim

    if [ "$config" = "release" ]; then sim="$RELEASE_SIM"; else sim="$DEBUG_SIM"; fi

    echo "--- $config $backend $test ---"
    killall "Corona Simulator" 2>/dev/null || true; sleep 1

    local logfile="$RESULTS_DIR/${config}_${backend}_${test}.log"

    SOLAR2D_TEST="$test" SOLAR2D_BACKEND="$backend" "$sim" -no-console YES "$DEMO" > "$logfile" 2>&1 &
    local pid=$!

    if [ "$test" = "regression" ]; then
        sleep 25  # 10 scenes * 2s + overhead
        capture_screenshot "$RESULTS_DIR/${config}_${backend}_final.png"
    elif [ "$test" = "bench" ]; then
        sleep 55  # 5 levels * ~10s
    fi

    killall "Corona Simulator" 2>/dev/null || true; sleep 1

    # Extract results
    if [ "$test" = "regression" ]; then
        grep -E "PASS|FAIL|RESULTS" "$logfile" | tail -15
    elif [ "$test" = "bench" ]; then
        grep "\[Bench\]" "$logfile"
    fi
    echo ""
}

run_scene_screenshots() {
    local config="$1"
    local backend="$2"
    local sim
    local scenes=(shapes images text transforms blend animation groups physics masks stress)

    if [ "$config" = "release" ]; then sim="$RELEASE_SIM"; else sim="$DEBUG_SIM"; fi

    echo "--- $config $backend screenshots ---"
    for scene in "${scenes[@]}"; do
        killall "Corona Simulator" 2>/dev/null || true; sleep 1
        SOLAR2D_TEST=scene SOLAR2D_SCENE="$scene" SOLAR2D_BACKEND="$backend" "$sim" -no-console YES "$DEMO" > /dev/null 2>&1 &
        sleep 4
        capture_screenshot "$RESULTS_DIR/${config}_${backend}_${scene}.png"
    done
    killall "Corona Simulator" 2>/dev/null || true
    echo "  Screenshots saved to $RESULTS_DIR/"
}

echo "=== Solar2D Regression Test Suite ==="
echo "Results: $RESULTS_DIR"
echo ""

if [ "$MODE" = "debug" ] || [ "$MODE" = "all" ]; then
    run_test debug gl regression
    run_test debug bgfx regression
    run_test debug gl bench
    run_test debug bgfx bench
fi

if [ "$MODE" = "release" ] || [ "$MODE" = "all" ]; then
    run_test release gl regression
    run_test release bgfx regression
    run_test release gl bench
    run_test release bgfx bench
fi

# Scene-by-scene screenshots (debug bgfx vs gl)
if [ "$MODE" = "screenshots" ] || [ "$MODE" = "all" ]; then
    run_scene_screenshots debug gl
    run_scene_screenshots debug bgfx
fi

echo ""
echo "=== All tests complete ==="
echo "Results directory: $RESULTS_DIR"
ls -la "$RESULTS_DIR/" | head -20
