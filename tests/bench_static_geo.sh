#!/bin/bash
# Static geometry cache performance comparison
# Compares FPS between bgfx-solar2d baseline and feature/static-geometry-cache
set -e

CORONA_DIR="/Users/yee/data/dev/app/labo/corona"
RESULTS="/tmp/static_geo_bench_$(date +%Y%m%d_%H%M%S)"
SIMULATOR="./platform/mac/build/Release/Corona Simulator.app/Contents/MacOS/Corona Simulator"
WAIT_SECS=35

mkdir -p "$RESULTS"
cd "$CORONA_DIR"

CURRENT_BRANCH=$(git branch --show-current)

build_release() {
    echo "Building Release..."
    xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer -configuration Release build \
        CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO ARCHS=arm64 ONLY_ACTIVE_ARCH=NO 2>&1 | tail -3
}

run_bench() {
    local name="$1"
    local log="$RESULTS/${name}.log"

    echo "Running bench: $name ..."
    SOLAR2D_TEST=bench SOLAR2D_BACKEND=bgfx "$SIMULATOR" -no-console YES tests/bgfx-demo > "$log" 2>&1 &
    local pid=$!
    sleep $WAIT_SECS
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true

    grep -i "fps\|objects\|RESULT" "$log" | tail -20 > "$RESULTS/${name}_fps.txt"
    echo "  Results saved to $RESULTS/${name}_fps.txt"
}

run_static_geo_test() {
    local name="$1"
    local log="$RESULTS/${name}_static_geo.log"

    echo "Running static_geo test: $name ..."
    SOLAR2D_TEST=static_geo SOLAR2D_BACKEND=bgfx "$SIMULATOR" -no-console YES tests/bgfx-demo > "$log" 2>&1 &
    local pid=$!
    sleep $WAIT_SECS
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true

    grep -i "static_geo\|RESULT" "$log" | tail -20 > "$RESULTS/${name}_static_geo_fps.txt"
    echo "  Results saved to $RESULTS/${name}_static_geo_fps.txt"
}

# === Baseline (bgfx-solar2d) ===
echo ""
echo "=== Phase 1: Baseline (bgfx-solar2d) ==="
git stash -q 2>/dev/null || true
git checkout bgfx-solar2d -q 2>/dev/null
build_release
run_bench "baseline"
run_static_geo_test "baseline"

# === Optimized (feature/static-geometry-cache) ===
echo ""
echo "=== Phase 2: Optimized (static-geometry-cache) ==="
git checkout "$CURRENT_BRANCH" -q 2>/dev/null
git stash pop -q 2>/dev/null || true
build_release
run_bench "optimized"
run_static_geo_test "optimized"

# === Comparison ===
echo ""
echo "============================================"
echo "=== COMPARISON ==="
echo "============================================"
echo ""
echo "--- Bench (500-5000 objects) ---"
echo "Baseline:"
cat "$RESULTS/baseline_fps.txt" 2>/dev/null || echo "(no data)"
echo ""
echo "Optimized:"
cat "$RESULTS/optimized_fps.txt" 2>/dev/null || echo "(no data)"
echo ""
echo "--- Static Geometry Test (500 static + 500 dynamic) ---"
echo "Baseline:"
cat "$RESULTS/baseline_static_geo_fps.txt" 2>/dev/null || echo "(no data)"
echo ""
echo "Optimized:"
cat "$RESULTS/optimized_static_geo_fps.txt" 2>/dev/null || echo "(no data)"
echo ""
echo "Results directory: $RESULTS"
