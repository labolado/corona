#!/bin/bash
# bench_instancing.sh - Compare instancing ON vs OFF performance
# Usage: bash tests/bench_instancing.sh [project_path]
set -euo pipefail

CORONA_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SIM="$CORONA_DIR/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
PROJECT="${1:-$CORONA_DIR/tests/bgfx-demo}"
DURATION=30  # seconds per test

if [ ! -f "$SIM" ]; then
    echo "ERROR: Simulator not built. Run xcodebuild first."
    exit 1
fi

RESULTS_DIR="/tmp/instancing_bench_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

echo "=== Instancing Benchmark ==="
echo "Project: $PROJECT"
echo "Results: $RESULTS_DIR"
echo ""

run_test() {
    local label="$1"
    local instance_val="$2"
    local batch_val="$3"
    local logfile="$RESULTS_DIR/${label}.log"

    echo "--- Running: $label (SOLAR2D_INSTANCE=$instance_val SOLAR2D_BATCH=$batch_val) ---"

    SOLAR2D_BACKEND=bgfx \
    SOLAR2D_TEST=instancing \
    SOLAR2D_INSTANCE="$instance_val" \
    SOLAR2D_BATCH="$batch_val" \
    "$SIM" -no-console YES "$PROJECT" > "$logfile" 2>&1 &
    local PID=$!

    sleep "$DURATION"
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true

    # Extract results
    echo "  Results:"
    grep -E '(INSTANCING RESULTS|objects:|speedup:|PERF|FPS)' "$logfile" | head -20 || echo "  (no results found)"
    echo ""
}

# Test 1: Instancing ON, batching ON (default)
run_test "instancing_on" "1" "1"

# Test 2: Instancing OFF, batching ON (CPU merge only)
run_test "instancing_off" "0" "1"

# Test 3: Both OFF (no optimization)
run_test "no_optimization" "0" "0"

echo "=== Benchmark Complete ==="
echo "Logs: $RESULTS_DIR/"
echo ""

# Print comparison
echo "=== Comparison ==="
for f in "$RESULTS_DIR"/*.log; do
    label=$(basename "$f" .log)
    echo "--- $label ---"
    grep 'FPS' "$f" | tail -5 || echo "  (no FPS data)"
done
