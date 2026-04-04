#!/bin/bash
# 场景快速测试工具 — 自动启动模拟器、截图并本地分析
#
# 用法:
#   bash tools/scene_test.sh <scene_number> [screenshot_path]
#
# 示例:
#   bash tools/scene_test.sh 3           # 测试文字场景
#   bash tools/scene_test.sh 2 /tmp/s2.png  # 测试图片场景，指定截图路径
#
# 流程:
#   1. 复制 demo 到 /tmp/test-scene-N/
#   2. 修改入口文件切换到指定场景
#   3. 启动模拟器（-no-console YES）
#   4. 等待 4 秒截图
#   5. 用 screenshot_analyze.py 本地分析
#   6. 关闭模拟器

set -e

SCENE_NUM="$1"
SHOT_PATH="${2:-/tmp/test-scene-${SCENE_NUM}.png}"
DEMO_SRC="/Users/yee/data/dev/app/labo/corona/tests/bgfx-demo"
TEST_DIR="/tmp/test-scene-${SCENE_NUM}"
SIMULATOR="/Users/yee/data/dev/app/labo/corona/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"

if [ -z "$SCENE_NUM" ]; then
    echo "错误: 请提供场景编号"
    echo "用法: bash tools/scene_test.sh <scene_number> [screenshot_path]"
    exit 1
fi

# 场景名映射（与 bgfx-demo/main.lua 中的 scenes 数组对应）
case "$SCENE_NUM" in
    1) SCENE_NAME="shapes" ;;
    2) SCENE_NAME="images" ;;
    3) SCENE_NAME="text" ;;
    4) SCENE_NAME="transforms" ;;
    5) SCENE_NAME="blend" ;;
    6) SCENE_NAME="animation" ;;
    7) SCENE_NAME="groups" ;;
    8) SCENE_NAME="physics" ;;
    9) SCENE_NAME="masks" ;;
    10) SCENE_NAME="stress" ;;
    *) echo "错误: 无效场景编号 $SCENE_NUM（支持 1-10）"; exit 1 ;;
esac

echo "===== 场景测试: $SCENE_NUM ($SCENE_NAME) ====="

# 1. 复制 demo
rm -rf "$TEST_DIR"
cp -R "$DEMO_SRC" "$TEST_DIR"
echo "已复制 demo 到 $TEST_DIR"

# 2. 修改 main.lua 切换场景
MAIN_LUA="$TEST_DIR/main.lua"
sed -i '' "s/^\(_G.bgfxDemoCurrentScene = \)[0-9]*/\1${SCENE_NUM}/" "$MAIN_LUA"
sed -i '' "s/composer.gotoScene(\"scene_[a-z]*\")/composer.gotoScene(\"scene_${SCENE_NAME}\")/" "$MAIN_LUA"
echo "已修改 main.lua 切换到场景 $SCENE_NUM: $SCENE_NAME"

# 3. 关闭已运行的模拟器
pkill -f "Corona Simulator" 2>/dev/null || true
sleep 1

# 4. 启动模拟器
"$SIMULATOR" -no-console YES "$TEST_DIR" &
SIM_PID=$!
echo "模拟器启动中 (PID: $SIM_PID) ..."
sleep 4

# 5. 获取窗口 ID 并截图
WID=$(python3 -c "
from Quartz import CGWindowListCopyWindowInfo, kCGWindowListExcludeDesktopElements, kCGNullWindowID
windows = CGWindowListCopyWindowInfo(kCGWindowListExcludeDesktopElements, kCGNullWindowID)
for w in windows:
    owner = w.get('kCGWindowOwnerName', '')
    name = w.get('kCGWindowName', '')
    if owner == 'Corona Simulator' and 'iPhone' in str(name):
        print(w.get('kCGWindowNumber', ''))
        break
")

if [ -z "$WID" ]; then
    echo "错误: 未找到 Corona Simulator 窗口"
    kill $SIM_PID 2>/dev/null || true
    exit 1
fi

screencapture -l "$WID" "$SHOT_PATH"
echo "截图已保存: $SHOT_PATH"

# 6. 本地分析
echo ""
echo "===== 截图分析 ====="
python3 tools/screenshot_analyze.py "$SHOT_PATH"

# 7. 关闭模拟器
kill $SIM_PID 2>/dev/null || true
pkill -f "Corona Simulator" 2>/dev/null || true
sleep 1

echo ""
echo "===== 测试完成 ====="
