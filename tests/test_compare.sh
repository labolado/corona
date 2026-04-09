#!/bin/bash
# GL vs bgfx 一键截图对比脚本
#
# 用法:
#   bash tests/test_compare.sh [step_name] [project_path]
#
# 参数:
#   step_name    — 截图文件名前缀（默认 "test"），截图保存为 /tmp/{step}_gl.png 和 /tmp/{step}_bgfx.png
#   project_path — Solar2D 项目路径（默认 /tmp/tank_test_copy）
#
# 流程:
#   1. 启动 GL 模式 → 检查日志 → 截图
#   2. 启动 bgfx 模式 → 检查日志 → 截图
#   3. 像素级对比（内置于 gemma4-ask，相同秒回 SAME，不同调模型分析）
#
# 示例:
#   bash tests/test_compare.sh step3b                           # 用 /tmp/tank_test_copy
#   bash tests/test_compare.sh mytest tests/bgfx-demo           # 用指定项目
#   IMG_DIFF_THRESHOLD=0.01 bash tests/test_compare.sh final    # 高精度对比
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

# GL
killall "Corona Simulator" 2>/dev/null; sleep 1
SOLAR2D_BACKEND=gl "$SIM" -no-console YES "$PROJECT" > /tmp/corona_gl.log 2>&1 &
sleep 8
check_fatal_errors /tmp/corona_gl.log "GL"
screenshot "$GL_IMG"
if [ ! -f "$GL_IMG" ]; then
    echo "❌ GL 截图失败（模拟器窗口未找到）"
    killall "Corona Simulator" 2>/dev/null
    exit 1
fi
killall "Corona Simulator" 2>/dev/null; sleep 1

# bgfx
SOLAR2D_BACKEND=bgfx "$SIM" -no-console YES "$PROJECT" > /tmp/corona_bgfx.log 2>&1 &
sleep 8
check_fatal_errors /tmp/corona_bgfx.log "bgfx"
screenshot "$BGFX_IMG"
if [ ! -f "$BGFX_IMG" ]; then
    echo "❌ bgfx 截图失败（模拟器窗口未找到）"
    killall "Corona Simulator" 2>/dev/null
    exit 1
fi
killall "Corona Simulator" 2>/dev/null; sleep 1

# 对比
echo "--- $STEP 对比结果 ---"
bash ~/.claude/skills/gemma4/scripts/gemma4-ask.sh -m 500 "GL（正确基准）vs bgfx 渲染对比。忽略对象位置偏移和动画帧差异（预期的，不是bug）。只关注渲染错误：颜色不对、纹理错乱、对象缺失/多余、亮度异常、透明度错误。按严重程度排序。" "$GL_IMG" "$BGFX_IMG"
