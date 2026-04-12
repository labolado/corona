#!/bin/bash
# Run Solar2D Lua unit tests and collect results
# Usage: bash tests/run_unit_tests.sh [gl|bgfx|both]

set -euo pipefail

CORONA_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SIM="$CORONA_DIR/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
PROJECT="$CORONA_DIR/tests/bgfx-demo"
MODE="${1:-bgfx}"
TIMEOUT=30

run_tests() {
    local backend="$1"
    local log="/tmp/solar2d_unit_test_${backend}_$$.log"

    echo "=== Running unit tests with backend: $backend ==="

    # Start simulator
    SOLAR2D_TEST=unit SOLAR2D_BACKEND="$backend" "$SIM" -no-console YES "$PROJECT" > "$log" 2>&1 &
    local pid=$!

    # Wait for completion or timeout
    local elapsed=0
    while [ $elapsed -lt $TIMEOUT ]; do
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
        if grep -q '\[TEST_SUMMARY\]' "$log" 2>/dev/null; then
            # Give a moment for remaining output to flush
            sleep 1
            break
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true

    # Check for Lua errors
    local lua_errors
    lua_errors=$(grep -E 'stack traceback|attempt to' "$log" 2>/dev/null | wc -l | tr -d ' ')
    if [ "$lua_errors" -gt 0 ]; then
        echo "ERROR: Lua runtime errors detected!"
        grep -E 'stack traceback|attempt to|ERROR' "$log" | head -10
        echo ""
        echo "Full log: $log"
        return 1
    fi

    # Parse results
    local passed failed summary
    passed=$(grep -F '[PASS]' "$log" 2>/dev/null | wc -l | tr -d ' ')
    failed=$(grep -F '[FAIL]' "$log" 2>/dev/null | wc -l | tr -d ' ')
    summary=$(grep '\[TEST_SUMMARY\]' "$log" 2>/dev/null || echo "No summary found")

    echo "$summary"
    echo "Passed: $passed"
    echo "Failed: $failed"

    if [ "$failed" -gt 0 ]; then
        echo ""
        echo "--- Failures ---"
        grep -F '[FAIL]' "$log"
        echo ""
        echo "Full log: $log"
        echo "RESULT: FAILED"
        return 1
    fi

    if [ "$passed" -eq 0 ]; then
        echo "WARNING: No tests found in output. Check log: $log"
        echo "--- Log tail ---"
        tail -20 "$log"
        return 1
    fi

    echo "RESULT: ALL PASSED ($backend)"
    echo ""
    return 0
}

# Main
EXIT_CODE=0

case "$MODE" in
    gl)
        run_tests gl || EXIT_CODE=1
        ;;
    bgfx)
        run_tests bgfx || EXIT_CODE=1
        ;;
    both)
        echo "=========================================="
        run_tests gl || EXIT_CODE=1
        echo "=========================================="
        run_tests bgfx || EXIT_CODE=1
        echo "=========================================="
        if [ $EXIT_CODE -eq 0 ]; then
            echo "BOTH BACKENDS PASSED"
        else
            echo "SOME TESTS FAILED"
        fi
        ;;
    *)
        echo "Usage: $0 [gl|bgfx|both]"
        exit 1
        ;;
esac

exit $EXIT_CODE
