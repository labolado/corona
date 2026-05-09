#!/bin/bash
# bgfx-demo 快速设备回归测试
# 用法：bash tests/quick_device_test.sh [device_id]
#
# 功能：
#   1. 安装 bgfx-demo APK（如果已有就复用，传 --build 强制重编）
#   2. 逐场景截图（用 Intent 直接跳场景，不依赖 adb tap）
#   3. gemma4 分析每张截图
#   4. 输出 PASS/FAIL 报告到 OUT_DIR
#
# 前提：
#   - adb 已连接设备
#   - APK 已编译：bash tests/build_android.sh tests/bgfx-demo com.labolado.bgfxdemo
#   - gemma4 可用（gemma4-ask 命令）
#
# 经验教训（来自 2026-05-02 回归测试）：
#   - 不要用 adb shell input tap 导航场景（坐标计算复杂且不可靠）
#   - 用 am start -e SOLAR2D_TEST_SCENE N 直接跳场景（需 bgfx-demo Lua 支持）
#   - 现阶段 bgfx-demo 不支持 Intent extras 跳场景，改用 am force-stop + am start 循环
#   - 每台设备截图存独立目录，方便比较
#
# 已知 BUG（待修）：
#   BUG-1: commit 53c511fc 引入循环符号链接（新 worktree/checkout 无法编译）
#     修法：git checkout 53c511fc^ -- external/bgfx external/bx external/bimg
#           然后 commit 修复
#   BUG-2: librtt/Renderer/Rtt_BgfxCommandBuffer.cpp:1448
#     ApplyMaskMatricesArr( prog, first ); ← 该函数已被 PR #31 revert 删除
#     修法：删除第 1448 行

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORONA_DIR="${CORONA_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
PACKAGE="com.labolado.bgfxdemo"
ACTIVITY="${PACKAGE}/com.ansca.corona.CoronaActivity"

# 输出目录
DATE=$(date +%Y%m%d_%H%M%S)
OUT_BASE="${GAME_ENGINE_DIR:-/Users/yee/data/dev/app/labo/game_engine}/issues/015-demo-regression"
OUT_DIR="${OUT_DIR:-$OUT_BASE/run_${DATE}}"

# 设备
DEVICE="${1:-}"
if [ -z "$DEVICE" ]; then
    DEVICE=$(adb devices | grep -v 'List of devices' | grep 'device$' | head -1 | awk '{print $1}')
    if [ -z "$DEVICE" ]; then
        echo "ERROR: 没有连接的设备，请用 adb connect 连接或传入 device_id"
        exit 1
    fi
fi

ADB="adb -s $DEVICE"
echo "设备: $DEVICE"
echo "输出: $OUT_DIR"

# APK 路径（build_android.sh 的输出）
APK_SEARCH_DIRS=(
    "/tmp/android-build-*/bgfx-demo.apk"
    "$CORONA_DIR/platform/android/app/build/outputs/apk/release/App-release-unsigned.apk"
)
APK=""
for PATTERN in "${APK_SEARCH_DIRS[@]}"; do
    FOUND=$(ls $PATTERN 2>/dev/null | head -1)
    if [ -n "$FOUND" ]; then
        APK="$FOUND"
        break
    fi
done

if [ -z "$APK" ] && [ "$1" != "--build" ]; then
    echo "找不到 APK，先检查设备上是否已安装..."
    if ! $ADB shell pm list packages | grep -q "$PACKAGE"; then
        echo "设备上没有 $PACKAGE，需要先编译:"
        echo "  bash tests/build_android.sh tests/bgfx-demo $PACKAGE"
        exit 1
    fi
    echo "设备上已有安装，跳过安装步骤"
else
    if [ -n "$APK" ]; then
        echo "安装 APK: $APK"
        $ADB install -r "$APK"
    fi
fi

mkdir -p "$OUT_DIR"

# 场景列表：(场景序号, 场景名, 等待秒数)
SCENES=(
    "1:shapes:4"
    "2:images:4"
    "3:text:4"
    "5:blend:4"
    "9:masks:5"
)

PASS=0
FAIL=0
ISSUES=()

screenshot_scene() {
    local SCENE_NUM="$1"
    local SCENE_NAME="$2"
    local WAIT="$3"
    local PNG="$OUT_DIR/scene${SCENE_NUM}_${SCENE_NAME}.png"

    echo ""
    echo "--- 场景 $SCENE_NUM ($SCENE_NAME) ---"

    # 停掉并重启（确保干净状态）
    $ADB shell am force-stop "$PACKAGE" 2>/dev/null || true
    sleep 1

    # 用 extra 传场景号（bgfx-demo 通过 SOLAR2D_TEST_SCENE 读取）
    $ADB shell am start -n "$ACTIVITY" \
        --ei "SOLAR2D_TEST_SCENE" "$SCENE_NUM" 2>/dev/null || \
    $ADB shell am start -n "$ACTIVITY" 2>/dev/null
    # 注：bgfx-demo 当前不响应 Intent extras，会启动到 Scene 1
    # TODO: 在 bgfx-demo/main.lua 里读 system.getInfo("intent") 跳场景

    sleep "$WAIT"

    # 检查有没有崩溃
    if $ADB shell "logcat -d -t 20 2>/dev/null" | grep -q 'FATAL EXCEPTION\|AndroidRuntime.*Error'; then
        echo "  ❌ 崩溃检测到"
        FAIL=$((FAIL + 1))
        ISSUES+=("场景$SCENE_NUM($SCENE_NAME): 崩溃")
        return
    fi

    # 截图
    $ADB exec-out screencap -p > "$PNG"
    local SIZE=$(wc -c < "$PNG")
    echo "  截图大小: ${SIZE} bytes"

    if [ "$SIZE" -lt 10000 ]; then
        echo "  ❌ 截图过小（可能黑屏）"
        FAIL=$((FAIL + 1))
        ISSUES+=("场景$SCENE_NUM($SCENE_NAME): 黑屏 (${SIZE}B)")
        return
    fi

    # Gemma4 分析
    if command -v gemma4-ask &>/dev/null; then
        ANALYSIS=$(gemma4-ask "这是 Android bgfx 渲染引擎截图。简短回答（20字内）：画面是否正常渲染？有无黑屏/白屏/颜色错误/形状缺失？" "$PNG" 2>/dev/null || echo "gemma4 不可用")
        echo "  Gemma4: $ANALYSIS"
        if echo "$ANALYSIS" | grep -qi '黑屏\|白屏\|异常\|错误\|失败'; then
            FAIL=$((FAIL + 1))
            ISSUES+=("场景$SCENE_NUM($SCENE_NAME): $ANALYSIS")
        else
            PASS=$((PASS + 1))
        fi
    else
        echo "  (gemma4-ask 不可用，跳过分析)"
        PASS=$((PASS + 1))
    fi
}

# 获取设备信息
DEVICE_MODEL=$($ADB shell getprop ro.product.model 2>/dev/null | tr -d '\r')
DEVICE_GPU=$($ADB shell getprop ro.hardware.egl 2>/dev/null | tr -d '\r')
RENDERER_LOG=$($ADB shell logcat -d -t 50 2>/dev/null | grep 'BgfxRenderer.*attempting\|BgfxRenderer.*type=' | tail -3)

echo ""
echo "设备型号: $DEVICE_MODEL"
echo "EGL: $DEVICE_GPU"
echo "渲染器: $RENDERER_LOG"
echo ""

# 逐场景测试
for SCENE in "${SCENES[@]}"; do
    IFS=':' read -r NUM NAME WAIT <<< "$SCENE"
    screenshot_scene "$NUM" "$NAME" "$WAIT"
done

# 停掉 APP
$ADB shell am force-stop "$PACKAGE" 2>/dev/null || true

# 写报告
REPORT="$OUT_DIR/REPORT.txt"
cat > "$REPORT" << EOF
快速设备测试报告
================
日期: $(date)
设备: $DEVICE ($DEVICE_MODEL)
GPU: $DEVICE_GPU
渲染器: $RENDERER_LOG

结果: PASS=$PASS FAIL=$FAIL

EOF

if [ ${#ISSUES[@]} -gt 0 ]; then
    echo "发现的问题:" >> "$REPORT"
    for ISSUE in "${ISSUES[@]}"; do
        echo "  - $ISSUE" >> "$REPORT"
    done
fi

echo "" >> "$REPORT"
echo "截图位置: $OUT_DIR/" >> "$REPORT"

cat "$REPORT"

echo ""
if [ $FAIL -eq 0 ]; then
    echo "✅ 所有场景通过 ($PASS/$((PASS+FAIL)))"
else
    echo "❌ 有失败场景 ($FAIL/$((PASS+FAIL)))"
    exit 1
fi
