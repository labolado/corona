#!/bin/bash
# bench_batching.sh - A/B comparison for draw call batching
#
# Usage: bash tests/bench_batching.sh [project_path]
#
# Compares bgfx with batching ON vs OFF using the test_batching test entry.

set -e

CORONA_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SIMULATOR="$CORONA_DIR/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
PROJECT="${1:-$CORONA_DIR/tests/bgfx-demo}"
LOG_DIR="/tmp/solar2d_batching_$(date +%Y%m%d_%H%M%S)"

mkdir -p "$LOG_DIR"

echo "=== Draw Call Batching Benchmark ==="
echo "Project: $PROJECT"
echo "Log dir: $LOG_DIR"
echo ""

# Check simulator exists
if [ ! -f "$SIMULATOR" ]; then
    echo "ERROR: Simulator not found at $SIMULATOR"
    echo "Build first: xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer -configuration Debug build ..."
    exit 1
fi

# Run with batching ON
echo "--- Test 1: Batching ENABLED ---"
SOLAR2D_TEST=batching SOLAR2D_BACKEND=bgfx SOLAR2D_BATCH=1 \
    "$SIMULATOR" -no-console YES "$PROJECT" > "$LOG_DIR/batch_on.log" 2>&1 &
PID_ON=$!
echo "PID: $PID_ON"

# Wait for test to complete (max 120 seconds)
TIMEOUT=120
ELAPSED=0
while kill -0 $PID_ON 2>/dev/null; do
    sleep 2
    ELAPSED=$((ELAPSED + 2))
    if [ $ELAPSED -ge $TIMEOUT ]; then
        echo "TIMEOUT: killing process"
        kill $PID_ON 2>/dev/null || true
        break
    fi
done
wait $PID_ON 2>/dev/null || true
echo "Batching ON test complete."
echo ""

# Run with batching OFF
echo "--- Test 2: Batching DISABLED ---"
SOLAR2D_TEST=batching SOLAR2D_BACKEND=bgfx SOLAR2D_BATCH=0 \
    "$SIMULATOR" -no-console YES "$PROJECT" > "$LOG_DIR/batch_off.log" 2>&1 &
PID_OFF=$!
echo "PID: $PID_OFF"

ELAPSED=0
while kill -0 $PID_OFF 2>/dev/null; do
    sleep 2
    ELAPSED=$((ELAPSED + 2))
    if [ $ELAPSED -ge $TIMEOUT ]; then
        echo "TIMEOUT: killing process"
        kill $PID_OFF 2>/dev/null || true
        break
    fi
done
wait $PID_OFF 2>/dev/null || true
echo "Batching OFF test complete."
echo ""

# Extract and compare results
echo "=== RESULTS ==="
echo ""
echo "--- Batching ENABLED ---"
grep -E '^\[|^=== BATCHING|Mode|same|mixed' "$LOG_DIR/batch_on.log" 2>/dev/null || echo "(no results found)"
echo ""
echo "--- Batching DISABLED ---"
grep -E '^\[|^=== BATCHING|Mode|same|mixed' "$LOG_DIR/batch_off.log" 2>/dev/null || echo "(no results found)"
echo ""

echo "Full logs: $LOG_DIR/"
echo "=== DONE ==="
