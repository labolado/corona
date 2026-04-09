#!/bin/bash
# GL vs bgfx 渲染视觉对比脚本
# Usage: bash tests/compare_render.sh [project_path]
# 默认用 tests/bgfx-demo，可指定其他项目路径

set -e
cd "$(dirname "$0")/.."

PROJECT="${1:-tests/bgfx-demo}"
SIM="./platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
OUTDIR="/tmp/render_compare_$(date +%H%M%S)"
mkdir -p "$OUTDIR"

echo "=== GL vs bgfx Render Compare ==="
echo "Project: $PROJECT"
echo "Output: $OUTDIR"

# Screenshot function
take_screenshot() {
    local output="$1"
    python3 << PYEOF
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
            print("saved")
        break
PYEOF
}

# GL
echo "--- GL rendering ---"
killall "Corona Simulator" 2>/dev/null || true; sleep 1
SOLAR2D_BACKEND=gl "$SIM" -no-console YES "$PROJECT" > "$OUTDIR/gl.log" 2>&1 &
sleep 10
take_screenshot "$OUTDIR/gl.png"
killall "Corona Simulator" 2>/dev/null || true; sleep 1

# bgfx
echo "--- bgfx rendering ---"
SOLAR2D_BACKEND=bgfx "$SIM" -no-console YES "$PROJECT" > "$OUTDIR/bgfx.log" 2>&1 &
sleep 10
take_screenshot "$OUTDIR/bgfx.png"
killall "Corona Simulator" 2>/dev/null || true; sleep 1

# Compare with gemma4
echo "--- gemma4 visual comparison ---"
if [ -f "$OUTDIR/gl.png" ] && [ -f "$OUTDIR/bgfx.png" ]; then
    bash ~/.claude/skills/gemma4/scripts/gemma4-ask.sh -m 500 \
        "对比这两张游戏截图。第一张是正确的GL渲染，第二张是bgfx渲染。列出所有视觉差异，判断bgfx是否渲染正确。用中文。" \
        "$OUTDIR/gl.png" "$OUTDIR/bgfx.png" 2>&1 | tee "$OUTDIR/compare_report.txt"
else
    echo "ERROR: screenshots missing"
fi

echo ""
echo "=== Results in $OUTDIR ==="
ls -la "$OUTDIR/"
