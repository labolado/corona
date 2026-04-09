#!/bin/bash
# crash_repro.sh — 一体化 crash 复现工具
#
# =============================================================================
# USAGE
# =============================================================================
#
#   bash tests/crash_repro.sh record [项目路径]
#       启动模拟器 + 后台录制鼠标点击，按 Enter 停止
#
#   bash tests/crash_repro.sh replay [重复次数] [项目路径]
#       回放录制的点击 N 次，统计崩溃次数
#
#   bash tests/crash_repro.sh asan [项目路径]
#       ASAN 构建 + 启动 + 回放 + 自动解析 ASAN 输出
#
#   bash tests/crash_repro.sh stress [项目路径]
#       压力测试：循环启动+回放+检测，直到崩溃
#
# =============================================================================
# WORKFLOW EXAMPLES
# =============================================================================
#
#   # 1. 录制你的操作流程（比如进入课程列表，点击卡片等）
#   bash tests/crash_repro.sh record /path/to/project
#   # ... 在模拟器上点击操作，完成后按 Enter ...
#
#   # 2. 回放 50 次，统计崩溃次数
#   bash tests/crash_repro.sh replay 50 /path/to/project
#   # 输出: "SUMMARY: 48/50 survived, 2 crashed"
#
#   # 3. ASAN 模式检测内存错误
#   bash tests/crash_repro.sh asan /path/to/project
#   # 自动检测: "ERROR: AddressSanitizer: heap-use-after-free"
#
#   # 4. 压力测试（直到崩溃）
#   bash tests/crash_repro.sh stress /path/to/project
#   # 循环执行，直到检测到崩溃或手动 Ctrl+C
#
# =============================================================================
# REQUIREMENTS
# =============================================================================
#
#   - macOS (使用 Cocoa/Quartz 框架)
#   - Python 3 + PyObjC (pip3 install pyobjc)
#   - Xcode (用于 ASAN 构建)
#   - 辅助功能权限 (System Preferences > Security > Accessibility)
#
# =============================================================================

set -e
cd "$(dirname "$0")/.."  # cd to corona root

MODE="${1:-help}"
CLICKS_FILE="/tmp/corona_test_clicks.json"
PIDFILE="/tmp/record_clicks.pid"
SIM="./platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
DEFAULT_PROJECT="/Users/yee/data/dev/app/labo/tank_test_copy"
LOG="/tmp/corona_crash_repro.log"
ASAN_LOG="/tmp/corona_asan.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

launch_sim() {
    local project="${1:-$DEFAULT_PROJECT}"
    local backend="${2:-bgfx}"
    local extra_env="${3:-}"

    pkill -9 "Corona Simulator" 2>/dev/null || true
    sleep 1

    echo "Launching: backend=$backend project=$project"
    eval $extra_env SOLAR2D_BACKEND=$backend \
        "$SIM" -no-console YES -project "$project/main.lua" \
        > "$LOG" 2>&1 &
    SIM_PID=$!
    echo "PID: $SIM_PID"

    sleep 8
    osascript -e 'tell application "Corona Simulator" to activate' 2>/dev/null
    echo "Simulator ready and focused."
}

get_simulator_pid() {
    pgrep -f "Corona Simulator.app" | head -1
}

check_alive() {
    local pid="${1:-$SIM_PID}"
    if ps -p "$pid" -o pid= 2>/dev/null > /dev/null; then
        return 0
    else
        return 1
    fi
}

print_banner() {
    echo ""
    echo "============================================"
    echo "  $1"
    echo "============================================"
    echo ""
}

cleanup_recorder() {
    if [ -f "$PIDFILE" ]; then
        local pid=$(cat "$PIDFILE" 2>/dev/null)
        if [ -n "$pid" ]; then
            kill "$pid" 2>/dev/null || true
        fi
        rm -f "$PIDFILE"
    fi
}

# =============================================================================
# MAIN COMMANDS
# =============================================================================

cmd_record() {
    local PROJECT="${2:-$DEFAULT_PROJECT}"
    launch_sim "$PROJECT"

    print_banner "RECORDING MODE"
    echo "Click inside the simulator to record your actions."
    echo "Recording runs in background. Press Enter when done."
    echo ""

    # Clean up any old recording
    rm -f "$CLICKS_FILE"
    
    # Start background recording (no timeout - wait for user to press Enter)
    python3 tests/record_clicks.py record "$CLICKS_FILE" --bg --pidfile "$PIDFILE"
    
    # Wait for user to press Enter
    echo "Recording... (Press Enter to stop)"
    read -r
    
    # Stop the recorder
    cleanup_recorder
    
    # Give it a moment to write the file
    sleep 0.5
    
    echo ""
    if [ -f "$CLICKS_FILE" ]; then
        local nclicks=$(python3 -c "import json; d=json.load(open('$CLICKS_FILE')); print(len(d.get('clicks', [])))" 2>/dev/null || echo "0")
        echo -e "${GREEN}Recorded $nclicks clicks to $CLICKS_FILE${NC}"
        echo "To replay: bash tests/crash_repro.sh replay [repeat_count]"
    else
        echo -e "${YELLOW}No recording file created${NC}"
    fi
    
    # Kill the simulator
    pkill -9 "Corona Simulator" 2>/dev/null || true
}

cmd_replay() {
    local REPEAT="${2:-5}"
    local PROJECT="${3:-$DEFAULT_PROJECT}"

    if [ ! -f "$CLICKS_FILE" ]; then
        echo -e "${RED}ERROR: No recording found. Run 'bash tests/crash_repro.sh record' first.${NC}"
        exit 1
    fi

    local nclicks=$(python3 -c "import json; d=json.load(open('$CLICKS_FILE')); print(len(d.get('clicks', [])))" 2>/dev/null || echo "0")
    echo "Replaying $nclicks clicks, $REPEAT times"

    local crashed=0
    local survived=0

    for i in $(seq 1 $REPEAT); do
        echo ""
        echo "=== Attempt $i/$REPEAT ==="
        launch_sim "$PROJECT"

        sleep 2
        
        # Get the simulator PID for checking
        local sim_pid=$(get_simulator_pid)
        echo "Simulator PID: $sim_pid"
        
        # Replay with crash detection
        if python3 tests/record_clicks.py replay "$CLICKS_FILE" --repeat 1 --check-alive "$sim_pid"; then
            echo -e "${GREEN}No crash on attempt $i${NC}"
            survived=$((survived + 1))
        else
            echo -e "${RED}CRASH DETECTED on attempt $i!${NC}"
            crashed=$((crashed + 1))
            
            # Show last log lines
            echo "--- Last 30 lines of log ---"
            tail -30 "$LOG" 2>/dev/null || true
            
            # Check for crash report
            if grep -q "CRASH" "$LOG" 2>/dev/null; then
                echo "Crash signature found in log"
            fi
        fi

        pkill -9 "Corona Simulator" 2>/dev/null || true
        sleep 1
    done

    echo ""
    print_banner "SUMMARY"
    echo -e "Total:  $REPEAT attempts"
    echo -e "Survived: ${GREEN}$survived${NC}"
    echo -e "Crashed:  ${RED}$crashed${NC}"
    
    if [ $crashed -eq 0 ]; then
        echo -e "${GREEN}=== No crash after $REPEAT attempts ===${NC}"
    else
        echo -e "${RED}=== CRASH REPRODUCED: $crashed/$REPEAT attempts ===${NC}"
        echo "Full log: $LOG"
    fi
}

cmd_asan() {
    local PROJECT="${2:-$DEFAULT_PROJECT}"

    print_banner "ASAN BUILD"
    echo "Building with Address Sanitizer..."
    
    # Build with ASAN
    if ! xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer \
        -configuration Debug build \
        CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
        ARCHS=arm64 ONLY_ACTIVE_ARCH=NO \
        ENABLE_ADDRESS_SANITIZER=YES > "$ASAN_LOG" 2>&1; then
        echo -e "${RED}ERROR: Build failed${NC}"
        tail -50 "$ASAN_LOG"
        exit 1
    fi
    
    echo -e "${GREEN}Build successful${NC}"
    tail -3 "$ASAN_LOG"

    echo ""
    print_banner "ASAN RUN"
    echo "Launching with ASAN..."
    
    # Clear old log
    > "$LOG"
    
    # Launch with ASAN options
    pkill -9 "Corona Simulator" 2>/dev/null || true
    sleep 1
    
    ASAN_OPTIONS='detect_leaks=0:halt_on_error=0:print_stats=1' \
        SOLAR2D_BACKEND=bgfx \
        "$SIM" -no-console YES -project "$PROJECT/main.lua" \
        > "$LOG" 2>&1 &
    SIM_PID=$!
    echo "PID: $SIM_PID"

    sleep 8
    osascript -e 'tell application "Corona Simulator" to activate' 2>/dev/null
    echo "Simulator ready."

    if [ -f "$CLICKS_FILE" ]; then
        local nclicks=$(python3 -c "import json; d=json.load(open('$CLICKS_FILE')); print(len(d.get('clicks', [])))" 2>/dev/null || echo "0")
        echo "Auto-replaying $nclicks recorded clicks..."
        sleep 2
        
        # Replay and check for crashes
        if ! python3 tests/record_clicks.py replay "$CLICKS_FILE" --repeat 1 --check-alive "$SIM_PID"; then
            echo -e "${YELLOW}Process terminated during replay${NC}"
        fi

        sleep 3
    else
        echo -e "${YELLOW}No recording found. Please operate manually.${NC}"
        echo "Press Enter when done to check ASAN output..."
        read -r
    fi

    # Check for ASAN output
    echo ""
    print_banner "ASAN RESULTS"
    
    local found_error=0
    
    if grep -q "ERROR: AddressSanitizer" "$LOG" 2>/dev/null; then
        found_error=1
        echo -e "${RED}!!! ASAN FOUND MEMORY ERROR !!!${NC}"
        echo ""
        echo "Summary:"
        grep "ERROR: AddressSanitizer" "$LOG" | head -3
        echo ""
        echo "Details:"
        grep -A 50 "ERROR: AddressSanitizer" "$LOG" | head -60
        echo ""
        echo "Full ASAN log: $LOG"
    elif grep -q "SUMMARY:.*AddressSanitizer" "$LOG" 2>/dev/null; then
        found_error=1
        echo -e "${RED}!!! ASAN SUMMARY FOUND !!!${NC}"
        grep -A 5 "SUMMARY:" "$LOG" | head -10
    else
        echo -e "${GREEN}No ASAN errors detected.${NC}"
    fi
    
    # Also check for regular crashes
    if ! check_alive; then
        echo ""
        echo -e "${YELLOW}Process crashed (non-ASAN crash)${NC}"
        echo "Last 20 lines of log:"
        tail -20 "$LOG"
    fi
    
    if [ $found_error -eq 0 ]; then
        echo ""
        echo "You can check the full log with: tail -f $LOG"
    fi
}

cmd_stress() {
    local PROJECT="${2:-$DEFAULT_PROJECT}"
    local attempt=0

    if [ ! -f "$CLICKS_FILE" ]; then
        echo -e "${RED}ERROR: No recording found. Run 'bash tests/crash_repro.sh record' first.${NC}"
        exit 1
    fi

    local nclicks=$(python3 -c "import json; d=json.load(open('$CLICKS_FILE')); print(len(d.get('clicks', [])))" 2>/dev/null || echo "0")
    
    print_banner "STRESS TEST MODE"
    echo "Replaying $nclicks clicks until crash detected..."
    echo "Press Ctrl+C to stop"
    echo ""

    while true; do
        attempt=$((attempt + 1))
        echo "=== Stress attempt $attempt ==="
        
        launch_sim "$PROJECT"
        sleep 2
        
        local sim_pid=$(get_simulator_pid)
        
        if ! python3 tests/record_clicks.py replay "$CLICKS_FILE" --repeat 1 --check-alive "$sim_pid"; then
            echo ""
            echo -e "${RED}!!! CRASH REPRODUCED on attempt $attempt !!!${NC}"
            echo ""
            echo "Last 50 lines of log:"
            tail -50 "$LOG"
            echo ""
            echo "Full log: $LOG"
            exit 0
        fi
        
        echo -e "${GREEN}Survived attempt $attempt${NC}"
        pkill -9 "Corona Simulator" 2>/dev/null || true
        sleep 1
    done
}

cmd_help() {
    cat << 'EOF'
crash_repro.sh — Crash reproduction tool for Corona Simulator

USAGE:
  bash tests/crash_repro.sh record [project_path]
      Launch sim + record clicks in background (press Enter to stop)

  bash tests/crash_repro.sh replay [N] [project_path]
      Replay N times, detect and count crashes

  bash tests/crash_repro.sh asan [project_path]
      Build+run with ASAN, auto-detect memory errors

  bash tests/crash_repro.sh stress [project_path]
      Loop until crash is reproduced

WORKFLOW:
  1. record — Record your interactions (navigate, click cards, etc.)
  2. replay — Replay automatically, count crashes
  3. asan   — Build with ASAN to catch memory errors
  4. stress — Keep replaying until crash occurs

EXAMPLES:
  # Record interactions
  bash tests/crash_repro.sh record /path/to/project

  # Replay 100 times and show statistics
  bash tests/crash_repro.sh replay 100

  # ASAN build and run
  bash tests/crash_repro.sh asan /path/to/project

  # Stress test until crash
  bash tests/crash_repro.sh stress
EOF
}

# =============================================================================
# MAIN
# =============================================================================

case "$MODE" in
    record)
        cmd_record "$@"
        ;;
    replay)
        cmd_replay "$@"
        ;;
    asan)
        cmd_asan "$@"
        ;;
    stress)
        cmd_stress "$@"
        ;;
    help|--help|-h|*)
        cmd_help
        ;;
esac
