#!/bin/bash
# Tank Level 1 自动测试脚本
# 用法:
#   bash tests/tank_test.sh                    # 导航到第一关并截图
#   bash tests/tank_test.sh --build            # 先编译再测试
#   bash tests/tank_test.sh --screenshot-only  # 只截图（假设已在第一关）
#
# 录制文件: tests/recordings/tank_to_level1.json
# 回放脚本: tests/replay_adb.sh

set -euo pipefail

CORONA_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RECORDING="$CORONA_DIR/tests/recordings/tank_to_level1.json"
REPLAY_SCRIPT="$CORONA_DIR/tests/replay_adb.sh"
PKG="com.labolado.tank"
TANK_PROJECT="/Users/yee/data/dev/app/labo/tank_test_copy"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
SCREENSHOT="/tmp/tank_level1_${TIMESTAMP}.png"

DO_BUILD=false
SCREENSHOT_ONLY=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build) DO_BUILD=true; shift ;;
        --screenshot-only) SCREENSHOT_ONLY=true; shift ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

echo "=== Tank Level 1 Test ==="

# Build if requested
if $DO_BUILD; then
    echo "[build] Compiling..."
    cd "$CORONA_DIR"
    FORCE_BUILD=1 bash tests/build_android.sh "$TANK_PROJECT" "$PKG" 2>&1 | tail -5
    echo "[build] Done"
fi

if ! $SCREENSHOT_ONLY; then
    # Navigate to level 1
    echo "[nav] Starting app..."
    adb shell am force-stop "$PKG" 2>/dev/null
    sleep 1
    adb shell am start -n "$PKG/com.ansca.corona.CoronaActivity" 2>/dev/null
    echo "[nav] Waiting 10s for app load..."
    sleep 10
    echo "[nav] Replaying..."
    bash "$REPLAY_SCRIPT" "$RECORDING" 2>&1 | tail -3
    echo "[nav] Waiting 3s for scene to settle..."
    sleep 3
fi

# Screenshot
echo "[screenshot] Capturing..."
adb shell screencap -p /sdcard/_tank_test.png
adb pull /sdcard/_tank_test.png "$SCREENSHOT" 2>/dev/null
echo "[screenshot] Saved: $SCREENSHOT"

echo "=== Done ==="
