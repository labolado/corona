#!/bin/bash
#
# Scene-switch regression for a2d1ada1's reset pre-scan (bgfx::reset before FBO
# draws on a viewport change). Cycles composer through FBO/effect-heavy scenes
# with slide transitions on both GL and bgfx and asserts: every transition
# completes, no crash/Lua error, and the resulting frame is not black/frozen.
#
# Covered: scene-switch (the main viewport/FBO-transition exposure of the
# pre-scan). NOT covered here: window resize and app suspend/resume, which are
# not cleanly scriptable on the macOS simulator (verify those manually).
#
# Usage: bash tests/run_sceneswitch_regression.sh [OUT_DIR]

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-/tmp/sceneswitch-regression}"
mkdir -p "$OUT"
SIM="$ROOT/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
PROJ="$ROOT/tests/bgfx-demo"
[ -x "$SIM" ] || { echo "ERROR: simulator not found at $SIM"; exit 1; }

run() {
    local be="$1" log="$OUT/$1.log" png="$OUT/$1.png" i
    pkill -f 'Corona Simulator' 2>/dev/null; pkill -f 'corona/platform.*lua' 2>/dev/null; sleep 1
    SOLAR2D_TEST=sceneswitch SOLAR2D_BACKEND="$be" "$SIM" -no-console YES "$PROJ" >"$log" 2>&1 &
    for i in $(seq 1 60); do grep -aq 'loaded=images round=1\|Runtime error' "$log" 2>/dev/null && break; sleep 0.2; done
    osascript -e 'tell application "System Events" to set frontmost of (first process whose name contains "Corona") to true' 2>/dev/null
    sleep 0.5
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
    for i in $(seq 1 40); do grep -aq 'SCENESWITCH DONE\|Runtime error' "$log" 2>/dev/null && break; sleep 0.5; done
    pkill -f 'Corona Simulator' 2>/dev/null
}

run gl
run bgfx

# All assertions done in python (reads files directly; immune to shell/rtk noise).
python3 - "$OUT" <<'PY'
import sys, re
from Cocoa import NSBitmapImageRep
OUT = sys.argv[1]
EXPECTED = 14  # 7 scenes x 2 rounds (test_sceneswitch.lua)

def bright_frac(path):
    rep = NSBitmapImageRep.imageRepWithContentsOfFile_(path)
    if rep is None:
        return -1.0
    w = int(rep.pixelsWide()); h = int(rep.pixelsHigh()); n = b = 0
    for y in range(int(h*0.15), int(h*0.85), 6):
        for x in range(int(w*0.1), int(w*0.9), 6):
            c = rep.colorAtX_y_(x, y)
            l = 0.299*c.redComponent()+0.587*c.greenComponent()+0.114*c.blueComponent()
            n += 1
            if l > 0.4:
                b += 1
    return b / n if n else 0.0

def check(be):
    data = open("%s/%s.log" % (OUT, be), "rb").read().decode("utf-8", "replace")
    loaded = len(re.findall(r"SCENESWITCH loaded=", data))
    done = "SCENESWITCH DONE" in data
    errors = len(re.findall(r"Runtime error|stack traceback|attempt to|SCENESWITCH ERROR", data))
    bf = bright_frac("%s/%s.png" % (OUT, be))
    ok = (loaded == EXPECTED and done and errors == 0 and bf > 0.01)
    print("%-4s loaded=%d/%d done=%s errors=%d bright_frac=%.3f -> %s"
          % (be, loaded, EXPECTED, done, errors, bf, "OK" if ok else "FAIL"))
    return ok

ok = check("gl") & check("bgfx")
print("PASS: scene-switch completes with no crash/black on GL and bgfx" if ok
      else "FAIL: scene-switch regression")
sys.exit(0 if ok else 1)
PY
