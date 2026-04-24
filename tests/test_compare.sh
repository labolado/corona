#!/bin/bash
# GL vs bgfx 一键截图对比脚本
#
# 用法:
#   bash tests/test_compare.sh [step_name] [project_path]
#
# 参数:
#   step_name    — 截图文件名前缀，同时用于查找断言文件 <project>/assertions/<step>.txt
#   project_path — Solar2D 项目路径（默认 /tmp/tank_test_copy）
#
# 流程:
#   1. 启动 GL 模式 → 检查日志 → 截图
#   2. 启动 bgfx 模式 → 检查日志 → 截图
#   3. 若存在 assertions/<step>.txt → gemma4 逐条断言判断（输出 PASS:/FAIL:，有 FAIL 则 exit 1）
#      否则 → 通用 diff 描述
#
# 示例:
#   bash tests/test_compare.sh step3b                                         # 用 /tmp/tank_test_copy，通用对比
#   SOLAR2D_TEST=particles bash tests/test_compare.sh particles tests/bgfx-demo  # 断言模式
#   IMG_DIFF_THRESHOLD=0.01 bash tests/test_compare.sh final                  # 高精度对比
#
# 依赖: gemma4-ask.sh（含像素快速判断 + 模型分析）
set +e  # killall 等命令可能返回非零，不要退出

STEP="${1:-test}"
cd "$(dirname "$0")/.." 2>/dev/null  # cd 到 corona 根目录（如果从 tests/ 下运行）
SIM="./platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
if [ ! -f "$SIM" ]; then
    SIM="/Users/yee/data/dev/app/labo/corona/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
fi
PROJECT="${2:-/tmp/tank_test_copy}"
GL_IMG="/tmp/${STEP}_gl.png"
BGFX_IMG="/tmp/${STEP}_bgfx.png"

screenshot() {
    local output="$1"
    python3 -c "
import Quartz
from Cocoa import NSBitmapImageRep
wl = Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID)
for w in wl:
    if 'Corona' in str(w.get('kCGWindowOwnerName', '')):
        wid = w['kCGWindowNumber']
        img = Quartz.CGWindowListCreateImage(Quartz.CGRectNull, Quartz.kCGWindowListOptionIncludingWindow, wid, Quartz.kCGWindowImageDefault)
        if img and Quartz.CGImageGetWidth(img) > 100:
            rep = NSBitmapImageRep.alloc().initWithCGImage_(img)
            data = rep.representationUsingType_properties_(4, None)
            data.writeToFile_atomically_('${output}', True)
            print('saved')
        break
"
}

# 错误检查函数：有致命错误就停止，不继续无意义的截图
check_fatal_errors() {
    local logfile="$1"
    local backend="$2"
    # 只匹配真正的 Lua 运行时错误，排除 require/path 相关的 traceback（那只是警告）
    if grep -q 'attempt to call\|attempt to index\|attempt to perform\|attempt to compare\|module .* not found' "$logfile" 2>/dev/null; then
        echo "❌ $backend Lua 致命错误，停止测试："
        grep 'attempt to\|stack traceback\|ERROR' "$logfile" | grep -v 'WARNING\|require.*path\|case-sensitive' | head -5
        echo ""
        echo "完整日志: $logfile"
        killall "Corona Simulator" 2>/dev/null
        exit 1
    fi
}

# 等待截图信号：Lua 打印 SCREENSHOT_READY 时立即截图，否则等满 SCREENSHOT_WAIT 秒
# 兼容旧测试（无信号则等满超时）
SCREENSHOT_WAIT="${SCREENSHOT_WAIT:-8}"

wait_and_screenshot() {
    local logfile="$1"
    local output="$2"
    local backend="$3"
    local deadline=$(( $(date +%s) + SCREENSHOT_WAIT ))

    # 等 SCREENSHOT_READY 信号，每 0.3s 检查一次
    while [ $(date +%s) -lt $deadline ]; do
        if grep -q 'SCREENSHOT_READY' "$logfile" 2>/dev/null; then
            sleep 0.2  # 等一帧确保画面稳定
            break
        fi
        sleep 0.3
    done

    check_fatal_errors "$logfile" "$backend"
    screenshot "$output"
    if [ ! -f "$output" ]; then
        echo "❌ $backend 截图失败（模拟器窗口未找到）"
        killall "Corona Simulator" 2>/dev/null
        exit 1
    fi
}

# GL
killall "Corona Simulator" 2>/dev/null; sleep 1
SOLAR2D_BACKEND=gl "$SIM" -no-console YES "$PROJECT" > /tmp/corona_gl.log 2>&1 &
wait_and_screenshot /tmp/corona_gl.log "$GL_IMG" "GL"
killall "Corona Simulator" 2>/dev/null; sleep 1

# bgfx
SOLAR2D_BACKEND=bgfx "$SIM" -no-console YES "$PROJECT" > /tmp/corona_bgfx.log 2>&1 &
wait_and_screenshot /tmp/corona_bgfx.log "$BGFX_IMG" "bgfx"
killall "Corona Simulator" 2>/dev/null; sleep 1

# 对比
echo "--- $STEP 对比结果 ---"

# 查找断言文件：优先 <project>/assertions/<step>.txt，其次 tests/bgfx-demo/assertions/<step>.txt
ASSERTIONS_FILE=""
if [ -f "${PROJECT}/assertions/${STEP}.txt" ]; then
    ASSERTIONS_FILE="${PROJECT}/assertions/${STEP}.txt"
elif [ -f "tests/bgfx-demo/assertions/${STEP}.txt" ]; then
    ASSERTIONS_FILE="tests/bgfx-demo/assertions/${STEP}.txt"
elif [ -f "${PROJECT}/assertions/_default.txt" ]; then
    ASSERTIONS_FILE="${PROJECT}/assertions/_default.txt"
elif [ -f "tests/bgfx-demo/assertions/_default.txt" ]; then
    ASSERTIONS_FILE="tests/bgfx-demo/assertions/_default.txt"
fi

if [ -n "$ASSERTIONS_FILE" ]; then
    ASSERTIONS=$(cat "$ASSERTIONS_FILE")
    RESULT=$(bash ~/.claude/skills/gemma4/scripts/gemma4-ask.sh -m 600 \
"GL（左图）是正确基准，bgfx（右图）是被测目标。两者是不同渲染后端实现。
判断标准：
- 允许：轻微抗锯齿差异、亚像素级精度偏差、细微亮度差（<5%）
- 不允许（直接FAIL）：几何形状类型不同（矩形变圆形）、颜色色相明显偏移（深红变粉红/橙色）、亮度/饱和度差异>15%、图形缺失或多余
请逐条判断以下断言是否成立，判断要严格，有明确视觉差异就FAIL。
输出格式严格每行：PASS: <断言> 或 FAIL: <断言> — <具体差异描述>

断言列表：
${ASSERTIONS}" "$GL_IMG" "$BGFX_IMG")
    echo "$RESULT"
    echo ""
    if echo "$RESULT" | grep -q '^FAIL:'; then
        FAIL_COUNT=$(echo "$RESULT" | grep -c '^FAIL:')
        echo "❌ ${FAIL_COUNT} 条断言失败 ($ASSERTIONS_FILE)"
        exit 1
    else
        echo "✅ 所有断言通过"
    fi
else
    bash ~/.claude/skills/gemma4/scripts/gemma4-ask.sh -m 500 \
        "GL（正确基准）vs bgfx 渲染对比。忽略对象位置偏移和动画帧差异（预期的，不是bug）。只关注渲染错误：颜色不对、纹理错乱、对象缺失/多余、亮度异常、透明度错误。按严重程度排序。" \
        "$GL_IMG" "$BGFX_IMG"
fi
