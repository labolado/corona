#!/bin/bash
# bench_all.sh - Comprehensive performance comparison
#
# Runs test_benchmark_all across multiple configurations and collects results.
#
# Usage: bash tests/bench_all.sh [project_path]
#
# Configurations tested:
#   1. GL Debug
#   2. GL Release
#   3. bgfx Debug (all optimizations ON)
#   4. bgfx Release (all optimizations ON)
#   5. bgfx Release (all optimizations OFF)

set -e

CORONA_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT="${1:-$CORONA_DIR/tests/bgfx-demo}"
LOG_DIR="/tmp/solar2d_bench_all_$(date +%Y%m%d_%H%M%S)"
TIMEOUT=300  # 5 minutes per config (4 scenes x ~10s each + overhead)

mkdir -p "$LOG_DIR"

echo "============================================"
echo "  Solar2D Comprehensive Performance Benchmark"
echo "============================================"
echo "Project: $PROJECT"
echo "Log dir: $LOG_DIR"
echo "Date: $(date)"
echo ""

# Check both Debug and Release simulators
SIM_DEBUG="$CORONA_DIR/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
SIM_RELEASE="$CORONA_DIR/platform/mac/build/Release/Corona Simulator.app/Contents/MacOS/Corona Simulator"

HAS_DEBUG=0
HAS_RELEASE=0

if [ -f "$SIM_DEBUG" ]; then
    HAS_DEBUG=1
    echo "[OK] Debug build found"
else
    echo "[SKIP] Debug build not found"
fi

if [ -f "$SIM_RELEASE" ]; then
    HAS_RELEASE=1
    echo "[OK] Release build found"
else
    echo "[SKIP] Release build not found"
fi

if [ $HAS_DEBUG -eq 0 ] && [ $HAS_RELEASE -eq 0 ]; then
    echo ""
    echo "ERROR: No simulator builds found. Build first:"
    echo "  Debug:   xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer -configuration Debug build CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO ARCHS=arm64 ONLY_ACTIVE_ARCH=NO"
    echo "  Release: xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer -configuration Release build CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO ARCHS=arm64 ONLY_ACTIVE_ARCH=NO"
    exit 1
fi

echo ""

# Configuration matrix: name, simulator, backend, batch, instance
CONFIGS=()

if [ $HAS_DEBUG -eq 1 ]; then
    CONFIGS+=("GL_Debug|$SIM_DEBUG|gl|1|1")
    CONFIGS+=("bgfx_Debug_ON|$SIM_DEBUG|bgfx|1|1")
fi

if [ $HAS_RELEASE -eq 1 ]; then
    CONFIGS+=("GL_Release|$SIM_RELEASE|gl|1|1")
    CONFIGS+=("bgfx_Release_ON|$SIM_RELEASE|bgfx|1|1")
    CONFIGS+=("bgfx_Release_OFF|$SIM_RELEASE|bgfx|0|0")
fi

TOTAL=${#CONFIGS[@]}
CURRENT=0
FAIL_LOG="$LOG_DIR/failures.log"
touch "$FAIL_LOG"

run_config() {
    local CONFIG_STR="$1"
    IFS='|' read -r NAME SIM BACKEND BATCH INSTANCE <<< "$CONFIG_STR"
    CURRENT=$((CURRENT + 1))

    echo "--- [$CURRENT/$TOTAL] $NAME ---"
    echo "  Backend=$BACKEND Batch=$BATCH Instance=$INSTANCE"

    local LOGFILE="$LOG_DIR/${NAME}.log"

    SOLAR2D_TEST=benchmark_all \
    SOLAR2D_BACKEND="$BACKEND" \
    SOLAR2D_BATCH="$BATCH" \
    SOLAR2D_INSTANCE="$INSTANCE" \
        "$SIM" -no-console YES "$PROJECT" > "$LOGFILE" 2>&1 &
    local PID=$!

    # Wait with timeout
    local ELAPSED=0
    while kill -0 $PID 2>/dev/null; do
        sleep 2
        ELAPSED=$((ELAPSED + 2))

        # Check if test completed (look for END BENCHMARK marker)
        if grep -q "END BENCHMARK" "$LOGFILE" 2>/dev/null; then
            sleep 2  # grace period for process to exit
            kill $PID 2>/dev/null || true
            break
        fi

        if [ $ELAPSED -ge $TIMEOUT ]; then
            echo "  TIMEOUT after ${TIMEOUT}s - killing"
            kill $PID 2>/dev/null || true
            echo "TIMEOUT: $NAME" >> "$FAIL_LOG"
            break
        fi
    done
    wait $PID 2>/dev/null || true

    # Check for errors
    if grep -q 'stack traceback\|attempt to\|ERROR' "$LOGFILE" 2>/dev/null; then
        echo "  WARNING: Errors detected in log"
        grep -m 3 'stack traceback\|attempt to\|ERROR' "$LOGFILE" | head -3
        echo "ERRORS: $NAME" >> "$FAIL_LOG"
    fi

    # Extract results
    if grep -q "END BENCHMARK" "$LOGFILE" 2>/dev/null; then
        echo "  OK - results collected"
    else
        echo "  WARNING: Benchmark may not have completed"
        echo "INCOMPLETE: $NAME" >> "$FAIL_LOG"
    fi
    echo ""
}

# Run all configurations sequentially
for cfg in "${CONFIGS[@]}"; do
    run_config "$cfg"
done

echo ""
echo "============================================"
echo "  RESULTS SUMMARY"
echo "============================================"
echo ""

# Extract and display results from each config
for cfg in "${CONFIGS[@]}"; do
    IFS='|' read -r NAME _ _ _ _ <<< "$cfg"
    LOGFILE="$LOG_DIR/${NAME}.log"

    echo "--- $NAME ---"
    if [ -f "$LOGFILE" ]; then
        # Print the results table
        sed -n '/=== BENCHMARK RESULTS/,/=== END BENCHMARK/p' "$LOGFILE" 2>/dev/null || echo "(no results)"
    else
        echo "(log not found)"
    fi
    echo ""
done

# Generate markdown report
REPORT="$LOG_DIR/BENCHMARK_REPORT.md"
cat > "$REPORT" << 'HEADER'
# Solar2D Comprehensive Benchmark Results

HEADER

echo "Date: $(date)" >> "$REPORT"
echo "Branch: $(cd "$CORONA_DIR" && git branch --show-current 2>/dev/null || echo 'unknown')" >> "$REPORT"
echo "Commit: $(cd "$CORONA_DIR" && git rev-parse --short HEAD 2>/dev/null || echo 'unknown')" >> "$REPORT"
echo "" >> "$REPORT"

# Parse FPS data from each config for each scenario
echo "## Per-Scenario Results" >> "$REPORT"
echo "" >> "$REPORT"

SCENARIO_NAMES=("Same-texture 2000" "Static UI 500+50" "Particles 3000" "Mixed 1000+500+500")

for scenario in "${SCENARIO_NAMES[@]}"; do
    echo "### $scenario" >> "$REPORT"
    echo "" >> "$REPORT"
    echo "| Config | Avg FPS | Min FPS | Max FPS | Draws | Submits |" >> "$REPORT"
    echo "|--------|---------|---------|---------|-------|---------|" >> "$REPORT"

    for cfg in "${CONFIGS[@]}"; do
        IFS='|' read -r NAME _ _ _ _ <<< "$cfg"
        LOGFILE="$LOG_DIR/${NAME}.log"

        if [ -f "$LOGFILE" ]; then
            # Extract the line matching this scenario from results table
            LINE=$(grep "$scenario" "$LOGFILE" 2>/dev/null | grep -E '[0-9]+\.[0-9]+' | tail -1)
            if [ -n "$LINE" ]; then
                # Parse: "Scenario           AvgFPS   MinFPS   MaxFPS   AvgDraws   AvgSubmits"
                AVG=$(echo "$LINE" | awk '{print $(NF-4)}')
                MIN=$(echo "$LINE" | awk '{print $(NF-3)}')
                MAX=$(echo "$LINE" | awk '{print $(NF-2)}')
                DRAWS=$(echo "$LINE" | awk '{print $(NF-1)}')
                SUBMITS=$(echo "$LINE" | awk '{print $NF}')
                echo "| $NAME | $AVG | $MIN | $MAX | $DRAWS | $SUBMITS |" >> "$REPORT"
            else
                echo "| $NAME | - | - | - | - | - |" >> "$REPORT"
            fi
        fi
    done
    echo "" >> "$REPORT"
done

echo "" >> "$REPORT"
echo "## Raw Logs" >> "$REPORT"
echo "" >> "$REPORT"
echo "Log directory: \`$LOG_DIR/\`" >> "$REPORT"

# Failures
FAIL_COUNT=$(wc -l < "$FAIL_LOG" 2>/dev/null | tr -d ' ')
if [ "$FAIL_COUNT" -gt 0 ]; then
    echo ""
    echo "WARNINGS ($FAIL_COUNT):"
    cat "$FAIL_LOG"
fi

echo ""
echo "Markdown report: $REPORT"
echo "Full logs: $LOG_DIR/"
echo ""
echo "============================================"
echo "  DONE"
echo "============================================"
