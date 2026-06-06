#!/bin/bash
#
# Directional-shadow regression.
#
# Renders SOLAR2D_TEST=shadowdir (an offset shadow filter applied to a
# display.newSnapshot, which exercises the bgfx Metal IsCanvasFlipY texelSize.y
# negation in Rtt_Renderer.cpp) on both GL and bgfx, then asserts the dark
# shadow band lands on the SAME vertical side in both backends. A wrong Metal
# flip would mirror the shadow on bgfx relative to GL and fail this test.
#
# Verdict is an external screenshot + center-column luminance scan. We do NOT use
# display.colorSample: its Display::Capture callback does not fire on bgfx when a
# snapshot with a fill.effect is on screen (see tasks/lessons.md). The window is
# raised before each capture because CGWindowListCreateImage returns blank for an
# occluded Metal window.
#
# Usage: bash tests/run_shadowdir_regression.sh [OUT_DIR]

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-/tmp/shadowdir-regression}"
mkdir -p "$OUT"
SIM="$ROOT/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
PROJ="$ROOT/tests/bgfx-demo"
[ -x "$SIM" ] || { echo "ERROR: simulator not found at $SIM"; exit 1; }

# Force the custom effect to recompile from current source.
rm -rf "$HOME/Library/Application Support/Corona Simulator/"bgfx-demo-*/Caches/bgfx_shaders 2>/dev/null

capture() {
    local be="$1" log="$OUT/$1.log" png="$OUT/$1.png" i
    pkill -f 'Corona Simulator' 2>/dev/null; pkill -f 'corona/platform.*lua' 2>/dev/null; sleep 1
    SOLAR2D_TEST=shadowdir SOLAR2D_BACKEND="$be" "$SIM" -no-console YES "$PROJ" >"$log" 2>&1 &
    for i in $(seq 1 40); do
        grep -q 'SHADOWDIR TEST READY\|Runtime error\|stack traceback' "$log" 2>/dev/null && break
        sleep 0.5
    done
    if grep -q 'Runtime error\|stack traceback' "$log" 2>/dev/null; then
        echo "ERROR: $be runtime error (see $log)"; pkill -f 'Corona Simulator' 2>/dev/null; exit 1
    fi
    sleep 1.5
    osascript -e 'tell application "System Events" to set frontmost of (first process whose name contains "Corona") to true' 2>/dev/null
    sleep 0.6
    python3 - "$png" <<'PY'
import Quartz, sys
from Cocoa import NSBitmapImageRep
for w in Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID):
    if 'Corona' in str(w.get('kCGWindowOwnerName', '')):
        img = Quartz.CGWindowListCreateImage(Quartz.CGRectNull, Quartz.kCGWindowListOptionIncludingWindow, w['kCGWindowNumber'], Quartz.kCGWindowImageBestResolution)
        if img:
            NSBitmapImageRep.alloc().initWithCGImage_(img).representationUsingType_properties_(4, None).writeToFile_atomically_(sys.argv[1], True)
        break
PY
    pkill -f 'Corona Simulator' 2>/dev/null
}

capture gl
capture bgfx

python3 - "$OUT/gl.png" "$OUT/bgfx.png" <<'PY'
import sys
from Cocoa import NSBitmapImageRep

def scan(path):
    rep = NSBitmapImageRep.imageRepWithContentsOfFile_(path)
    if rep is None:
        return None
    w = int(rep.pixelsWide()); h = int(rep.pixelsHigh()); cx = w // 2
    mn, mny, dark = 2.0, -1, 0
    for y in range(int(h * 0.30), int(h * 0.65)):
        c = rep.colorAtX_y_(cx, y)
        lum = 0.299 * c.redComponent() + 0.587 * c.greenComponent() + 0.114 * c.blueComponent()
        if lum < mn:
            mn, mny = lum, y
        if lum < 0.3:
            dark += 1
    return {"mn": mn, "yh": mny / h, "dark": dark}

g = scan(sys.argv[1]); b = scan(sys.argv[2])
if not g or not b:
    print("FAIL: missing capture (gl=%s bgfx=%s)" % (bool(g), bool(b))); sys.exit(1)
print("GL   darkest=%.3f y/h=%.3f darkpx=%d" % (g["mn"], g["yh"], g["dark"]))
print("BGFX darkest=%.3f y/h=%.3f darkpx=%d" % (b["mn"], b["yh"], b["dark"]))
ok = (g["mn"] < 0.3 and b["mn"] < 0.3 and g["dark"] >= 3 and b["dark"] >= 3
      and abs(g["yh"] - b["yh"]) < 0.04)
print("PASS: shadow on same side in GL and bgfx" if ok else
      "FAIL: shadow missing or on different side (Metal flip regression)")
sys.exit(0 if ok else 1)
PY
