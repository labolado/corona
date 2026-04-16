#!/bin/bash
# Full regression test: GL vs bgfx, Debug vs Release
# Usage: bash tests/run_all_tests.sh [debug|release|features|perf|all]
# debug     - 回归测试 (GL+bgfx)
# release   - Release 构建回归+性能
# features  - 所有功能专项测试 (atlas/batch/sdf/fallback/texcomp/dirty/culling)
# perf      - 性能基准 (bench/drawcall/instancing/sdf)
# all       - 全部

set -e
cd "$(dirname "$0")/.."

# Check shader binary sync before any tests
echo "=== Pre-flight: shader binary sync check ==="
if ! bash tests/compile_shaders.sh --check; then
    echo "ERROR: Shader binaries out of sync. Run: bash tests/compile_shaders.sh"
    exit 1
fi
echo ""

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

run_feature_test() {
    local config="$1"    # debug or release
    local backend="$2"   # gl or bgfx
    local test="$3"      # atlas, batch, sdf, etc.
    local timeout="$4"   # seconds
    local sim

    if [ "$config" = "release" ]; then sim="$RELEASE_SIM"; else sim="$DEBUG_SIM"; fi

    echo "--- $config $backend $test ---"
    killall "Corona Simulator" 2>/dev/null || true; sleep 1

    local logfile="$RESULTS_DIR/${config}_${backend}_${test}.log"

    SOLAR2D_TEST="$test" SOLAR2D_BACKEND="$backend" "$sim" -no-console YES "$DEMO" > "$logfile" 2>&1 &
    local pid=$!
    sleep "$timeout"
    killall "Corona Simulator" 2>/dev/null || true; sleep 1

    # Extract results
    local pass=$(grep -c "\[PASS\]" "$logfile" 2>/dev/null || echo 0)
    local fail=$(grep -c "\[FAIL\]" "$logfile" 2>/dev/null || echo 0)
    echo "  PASS: $pass | FAIL: $fail"
    grep -E "RESULTS|Error|FAIL" "$logfile" | tail -5
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

# Feature tests (atlas, batch, fallback, texcomp, dirty, culling)
if [ "$MODE" = "features" ] || [ "$MODE" = "all" ]; then
    echo "=== Feature Tests ==="
    for backend in gl bgfx; do
        run_feature_test debug $backend atlas 15
        run_feature_test debug $backend batch 15
        run_feature_test debug $backend fallback 35
        run_feature_test debug $backend texcomp 15
        run_feature_test debug $backend dirty 25
        run_feature_test debug $backend culling 25
    done
fi

# Performance tests (bgfx only)
if [ "$MODE" = "perf" ] || [ "$MODE" = "all" ]; then
    echo "=== Performance Tests (bgfx only) ==="
    run_feature_test debug bgfx sdf 60
    run_feature_test debug bgfx drawcall 40
    run_feature_test debug bgfx instancing 30
    run_feature_test debug bgfx bench 50
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

# Test Summary
echo ""
echo "=== Test Summary ==="
echo "Results directory: $RESULTS_DIR"
total_pass=0; total_fail=0
for logfile in "$RESULTS_DIR"/*.log; do
    [ -f "$logfile" ] || continue
    p=$(grep -c "\[PASS\]" "$logfile" 2>/dev/null | tr -d '\n' || echo 0)
    f=$(grep -c "\[FAIL\]" "$logfile" 2>/dev/null | tr -d '\n' || echo 0)
    total_pass=$((total_pass + p))
    total_fail=$((total_fail + f))
done
echo "Total: $total_pass PASS / $total_fail FAIL"
if [ "$total_fail" -gt 0 ]; then
    echo "FAILED TESTS:"
    grep -l "\[FAIL\]" "$RESULTS_DIR"/*.log 2>/dev/null
fi
