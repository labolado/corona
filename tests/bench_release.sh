#!/bin/bash
# Release vs Debug 性能对比
# 用法: bash tests/bench_release.sh

CORONA_DIR="/Users/yee/data/dev/app/labo/corona"
RESULTS="/tmp/release_bench_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS"

SIM_DEBUG="$CORONA_DIR/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
SIM_RELEASE="$CORONA_DIR/platform/mac/build/Release/Corona Simulator.app/Contents/MacOS/Corona Simulator"
DEMO="$CORONA_DIR/tests/bgfx-demo"

run_bench() {
    local label="$1"
    local backend="$2"
    local sim="$3"
    local logfile="$RESULTS/${label}.log"
    local fpsfile="$RESULTS/${label}_fps.txt"

    echo "=== $label Benchmark ==="
    if [ ! -f "$sim" ]; then
        echo "SKIP: $sim not found"
        echo "SKIP: binary not found" > "$fpsfile"
        return
    fi

    SOLAR2D_TEST=bench SOLAR2D_BACKEND="$backend" "$sim" -no-console YES "$DEMO" > "$logfile" 2>&1 &
    local PID=$!
    sleep 30
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    grep -i "FPS\|fps\|objects\|bench" "$logfile" | tail -20 > "$fpsfile"
}

run_bench "debug_bgfx" bgfx "$SIM_DEBUG"
run_bench "release_bgfx" bgfx "$SIM_RELEASE"
run_bench "debug_gl" gl "$SIM_DEBUG"
run_bench "release_gl" gl "$SIM_RELEASE"

echo ""
echo "=== RESULTS ==="
for label in debug_bgfx release_bgfx debug_gl release_gl; do
    echo ""
    echo "--- $label ---"
    cat "$RESULTS/${label}_fps.txt"
done
echo ""
echo "Results saved to: $RESULTS"
