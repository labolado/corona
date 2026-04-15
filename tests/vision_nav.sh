#!/usr/bin/env bash
# vision_nav.sh — Gemma4 视觉驱动的 Android UI 自动导航
#
# 用法:
#   bash tests/vision_nav.sh --target "game level 1" --package com.labolado.tank
#   bash tests/vision_nav.sh --target "tank selection screen" --max-steps 15
#   bash tests/vision_nav.sh --target "main menu" --dry-run
#
# 每一步: adb 截图 → Gemma4 识别界面+决策 → adb tap/swipe → 循环
# 支持: tap（点击）、swipe_left/right/up/down（方向滑动）

set -euo pipefail

GEMMA4_ASK="$HOME/.claude/skills/gemma4/scripts/gemma4-ask.sh"
SCREENSHOT_DIR="/tmp/vision_nav"
TARGET=""
PACKAGE=""
MAX_STEPS=10
WAIT_SEC=3
DRY_RUN=false
VERBOSE=false
STUCK_THRESHOLD=5

# --- Parse args ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --target)   TARGET="$2"; shift 2 ;;
        --package)  PACKAGE="$2"; shift 2 ;;
        --max-steps) MAX_STEPS="$2"; shift 2 ;;
        --wait)     WAIT_SEC="$2"; shift 2 ;;
        --dry-run)  DRY_RUN=true; shift ;;
        --verbose)  VERBOSE=true; shift ;;
        --help|-h)
            cat << 'USAGE'
Usage: bash tests/vision_nav.sh --target <目标界面> [OPTIONS]

Options:
  --target     目标界面描述 (必填)
  --package    Android 包名，用于启动 app
  --max-steps  最大步数 (默认 10)
  --wait       每步等待秒数 (默认 3)
  --dry-run    只截图分析不执行操作
  --verbose    显示 Gemma4 完整输出
USAGE
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

[[ -z "$TARGET" ]] && { echo "ERROR: --target is required"; exit 1; }
command -v adb &>/dev/null || { echo "ERROR: adb not found"; exit 1; }
adb devices | grep -q 'device$' || { echo "ERROR: No Android device connected"; exit 1; }
[[ -x "$GEMMA4_ASK" ]] || { echo "ERROR: gemma4-ask.sh not found at $GEMMA4_ASK"; exit 1; }

# --- Setup ---
rm -rf "$SCREENSHOT_DIR"
mkdir -p "$SCREENSHOT_DIR"

SCREEN_W=0
SCREEN_H=0
echo "Target: $TARGET"
echo "Max steps: $MAX_STEPS"
echo ""

# Launch app if package specified
if [[ -n "$PACKAGE" ]]; then
    echo "Launching $PACKAGE..."
    adb shell am start -n "${PACKAGE}/com.ansca.corona.CoronaActivity" 2>/dev/null || \
    adb shell monkey -p "$PACKAGE" -c android.intent.category.LAUNCHER 1 2>/dev/null
    sleep 3
fi

# --- Swipe presets (adjusted per screen size later) ---
do_swipe() {
    local dir="$1"
    local cx=$((SCREEN_W / 2))
    local cy=$((SCREEN_H / 2))
    case "$dir" in
        swipe_left)  adb shell input swipe $((cx + cx/2)) $cy $((cx/2)) $cy 500 ;;
        swipe_right) adb shell input swipe $((cx/2)) $cy $((cx + cx/2)) $cy 500 ;;
        swipe_up)    adb shell input swipe $cx $((cy + cy/2)) $cx $((cy/2)) 500 ;;
        swipe_down)  adb shell input swipe $cx $((cy/2)) $cx $((cy + cy/2)) 500 ;;
        *) echo "  WARNING: Unknown swipe direction: $dir"; return 1 ;;
    esac
}

# --- Navigation loop ---
stuck_count=0
LAST_ACTION_DESC=""

for step in $(seq 1 "$MAX_STEPS"); do
    echo "=== Step $step/$MAX_STEPS ==="

    # 1. Screenshot
    SHOT="/tmp/vision_nav_step_${step}.png"
    adb shell screencap -p /sdcard/vision_nav_tmp.png
    adb pull /sdcard/vision_nav_tmp.png "$SHOT" 2>/dev/null
    adb shell rm /sdcard/vision_nav_tmp.png 2>/dev/null
    cp "$SHOT" "$SCREENSHOT_DIR/step_${step}.png"

    [[ -f "$SHOT" ]] || { echo "ERROR: Failed to capture screenshot"; exit 1; }

    # Detect actual image dimensions
    IMG_DIMS=$(python3 -c "from PIL import Image; im=Image.open('$SHOT'); print(f'{im.width}x{im.height}')")
    SCREEN_W=$(echo "$IMG_DIMS" | cut -dx -f1)
    SCREEN_H=$(echo "$IMG_DIMS" | cut -dx -f2)
    echo "  Screenshot: $SHOT (${SCREEN_W}x${SCREEN_H})"

    # 2. Create annotated screenshot with grid
    ANNOTATED="/tmp/vision_nav_annotated_${step}.png"
    python3 << PYEOF
from PIL import Image, ImageDraw
im = Image.open("$SHOT")
draw = ImageDraw.Draw(im)
w, h = im.size
for i in range(1, 4):
    x, y = w * i // 4, h * i // 4
    draw.line([(x, 0), (x, h)], fill=(255, 0, 0, 80), width=1)
    draw.line([(0, y), (w, y)], fill=(255, 0, 0, 80), width=1)
    draw.text((x + 2, 2), f"x={x}", fill=(255, 0, 0))
    draw.text((2, y + 2), f"y={y}", fill=(255, 0, 0))
draw.text((2, 2), "0,0", fill=(255, 0, 0))
draw.text((w - 80, h - 16), f"{w},{h}", fill=(255, 0, 0))
im.save("$ANNOTATED")
PYEOF

    # 3. Build prompt
    STUCK_HINT=""
    if [[ $stuck_count -gt 0 && -n "$LAST_ACTION_DESC" ]]; then
        STUCK_HINT="重要：上一步 ${LAST_ACTION_DESC} 没有效果！请换一个完全不同的操作。如果 tap 没用，试试 swipe 滑动翻页；如果需要选择不同的选项，点击其他位置。"
    fi

    CX=$((SCREEN_W / 2))
    CY=$((SCREEN_H / 2))
    PROMPT="Android 游戏截图（${SCREEN_W}x${SCREEN_H}像素），红色网格标注坐标。原点(0,0)在左上角，屏幕中心≈(${CX},${CY})。
目标：导航到 ${TARGET}。
${STUCK_HINT}
分析界面，决定下一步：
- tap：点击按钮/选项，给出中心像素坐标。参考网格线定位。
- swipe_left / swipe_right：水平翻页（如选择不同坦克、切换页面）
- swipe_up / swipe_down：垂直滚动列表

输出一行 JSON（选一种）：
点击: {\"screen\":\"界面名\",\"reached\":false,\"action\":\"tap\",\"tap\":[x,y],\"reason\":\"原因\"}
滑动: {\"screen\":\"界面名\",\"reached\":false,\"action\":\"swipe_left\",\"reason\":\"原因\"}
已到达: {\"screen\":\"界面名\",\"reached\":true,\"action\":\"none\",\"reason\":\"原因\"}"

    echo "  Asking Gemma4..."
    RESPONSE=$(bash "$GEMMA4_ASK" -m 200 "$PROMPT" "$ANNOTATED" 2>/dev/null) || {
        echo "  WARNING: Gemma4 call failed, retrying..."
        sleep 2
        RESPONSE=$(bash "$GEMMA4_ASK" -m 200 "$PROMPT" "$ANNOTATED" 2>/dev/null) || {
            echo "  ERROR: Gemma4 failed twice at step $step"; exit 1
        }
    }

    $VERBOSE && echo "  Raw response: $RESPONSE"

    # 4. Parse JSON — write response to temp file to avoid shell escaping issues
    echo "$RESPONSE" > /tmp/vision_nav_response.txt
    JSON=$(python3 << 'PARSE_PY'
import json, re, sys
with open('/tmp/vision_nav_response.txt') as f:
    text = f.read()
# Try JSONDecoder for proper nested JSON parsing
start = text.find('{')
while start >= 0:
    try:
        obj, end = json.JSONDecoder().raw_decode(text, start)
        if isinstance(obj, dict) and 'screen' in obj:
            print(json.dumps(obj))
            sys.exit(0)
    except json.JSONDecodeError:
        pass
    start = text.find('{', start + 1)
print('PARSE_ERROR')
PARSE_PY
) || JSON="PARSE_ERROR"

    if [[ "$JSON" == "PARSE_ERROR" ]]; then
        echo "  WARNING: Parse failed, trying fallback prompt..."
        RESPONSE=$(bash "$GEMMA4_ASK" -m 150 "截图${SCREEN_W}x${SCREEN_H}。目标：${TARGET}。输出JSON:{\"screen\":\"名\",\"reached\":false,\"action\":\"tap\",\"tap\":[x,y],\"reason\":\"why\"}" "$ANNOTATED" 2>/dev/null) || true
        echo "$RESPONSE" > /tmp/vision_nav_response.txt
        JSON=$(python3 << 'PARSE_PY2'
import json, sys
with open('/tmp/vision_nav_response.txt') as f:
    text = f.read()
start = text.find('{')
while start >= 0:
    try:
        obj, end = json.JSONDecoder().raw_decode(text, start)
        if isinstance(obj, dict) and 'screen' in obj:
            print(json.dumps(obj)); sys.exit(0)
    except json.JSONDecodeError: pass
    start = text.find('{', start + 1)
print('PARSE_ERROR')
PARSE_PY2
) || JSON="PARSE_ERROR"
        if [[ "$JSON" == "PARSE_ERROR" ]]; then
            echo "  ERROR: Cannot parse. Raw: $RESPONSE"; sleep "$WAIT_SEC"; continue
        fi
    fi

    # Extract fields (use defaults to avoid unbound variable errors with set -u)
    SCREEN=$(echo "$JSON" | python3 -c "import sys,json; print(json.load(sys.stdin).get('screen','unknown'))" 2>/dev/null) || true
    SCREEN="${SCREEN:-unknown}"
    REACHED=$(echo "$JSON" | python3 -c "import sys,json; print('true' if json.load(sys.stdin).get('reached') else 'false')" 2>/dev/null) || true
    REACHED="${REACHED:-false}"
    ACTION=$(echo "$JSON" | python3 -c "import sys,json; print(json.load(sys.stdin).get('action','tap'))" 2>/dev/null) || true
    ACTION="${ACTION:-tap}"
    REASON=$(echo "$JSON" | python3 -c "import sys,json; print(json.load(sys.stdin).get('reason',''))" 2>/dev/null) || true
    REASON="${REASON:-}"
    TAP_X=$(echo "$JSON" | python3 -c "import sys,json; t=json.load(sys.stdin).get('tap',[]); print(int(t[0]) if len(t)>=2 else '')" 2>/dev/null) || true
    TAP_X="${TAP_X:-}"
    TAP_Y=$(echo "$JSON" | python3 -c "import sys,json; t=json.load(sys.stdin).get('tap',[]); print(int(t[1]) if len(t)>=2 else '')" 2>/dev/null) || true
    TAP_Y="${TAP_Y:-}"

    echo "  Screen: $SCREEN | Action: $ACTION | Reached: $REACHED"
    echo "  Reason: $REASON"

    # 5. Check if reached
    if [[ "$REACHED" == "true" ]]; then
        echo ""
        echo "=== SUCCESS: Reached '$TARGET' at step $step ==="
        echo "  Final screenshot: $SHOT"
        echo "  All screenshots in: $SCREENSHOT_DIR/"
        exit 0
    fi

    # 6. Stuck detection (pixel-based)
    if [[ $step -gt 1 ]]; then
        PREV_SHOT="/tmp/vision_nav_step_$((step - 1)).png"
        PIXEL_DIFF=$(python3 -c "
from PIL import Image; import numpy as np
a,b = np.array(Image.open('$PREV_SHOT').convert('RGB')), np.array(Image.open('$SHOT').convert('RGB'))
print(100.0 if a.shape!=b.shape else f'{np.abs(a.astype(float)-b.astype(float)).mean():.2f}')
" 2>/dev/null || echo "999")
        echo "  Pixel diff: $PIXEL_DIFF"
        if python3 -c "exit(0 if float('$PIXEL_DIFF') < 8.0 else 1)" 2>/dev/null; then
            stuck_count=$((stuck_count + 1))
            if [[ $stuck_count -ge $STUCK_THRESHOLD ]]; then
                echo ""; echo "=== STUCK: $stuck_count steps unchanged (diff=$PIXEL_DIFF) ==="
                echo "  Screenshots in: $SCREENSHOT_DIR/"; exit 2
            fi
            echo "  WARNING: Stuck ($stuck_count/$STUCK_THRESHOLD)"
        else
            stuck_count=0
        fi
    fi

    # 7. Override action when stuck (cycle through different strategies)
    if [[ $stuck_count -gt 0 ]]; then
        SCAN_IDX=$(( (stuck_count - 1) % 7 ))
        case $SCAN_IDX in
            0) ACTION="tap"; TAP_X=$CX; TAP_Y=$CY;       echo "  Override → tap center ($CX,$CY)" ;;
            1) ACTION="tap"; TAP_X=$CX; TAP_Y=$((CY*2/3)); echo "  Override → tap upper ($CX,$((CY*2/3)))" ;;
            2) ACTION="swipe_left";  echo "  Override → swipe_left" ;;
            3) ACTION="tap"; TAP_X=$((SCREEN_W*4/5)); TAP_Y=$CY; echo "  Override → tap right ($TAP_X,$CY)" ;;
            4) ACTION="swipe_up";    echo "  Override → swipe_up" ;;
            5) ACTION="tap"; TAP_X=$((SCREEN_W/5)); TAP_Y=$CY; echo "  Override → tap left ($TAP_X,$CY)" ;;
            6) ACTION="swipe_right"; echo "  Override → swipe_right" ;;
        esac
    fi

    # 8. Execute action
    case "$ACTION" in
        tap)
            if [[ -z "$TAP_X" || -z "$TAP_Y" ]]; then
                echo "  WARNING: No tap coordinates"; LAST_ACTION_DESC="(skipped)"; sleep "$WAIT_SEC"; continue
            fi
            # Clamp to bounds
            TAP_X=$(python3 -c "print(max(0, min($SCREEN_W, int($TAP_X))))")
            TAP_Y=$(python3 -c "print(max(0, min($SCREEN_H, int($TAP_Y))))")
            echo "  → Tap ($TAP_X, $TAP_Y)"
            LAST_ACTION_DESC="tap ($TAP_X,$TAP_Y)"
            $DRY_RUN || adb shell input tap "$TAP_X" "$TAP_Y"
            ;;
        swipe_left|swipe_right|swipe_up|swipe_down)
            echo "  → ${ACTION}"
            LAST_ACTION_DESC="$ACTION"
            $DRY_RUN || do_swipe "$ACTION"
            ;;
        none)
            LAST_ACTION_DESC="none"
            ;;
        *)
            echo "  WARNING: Unknown action '$ACTION', treating as tap"
            if [[ -n "$TAP_X" && -n "$TAP_Y" ]]; then
                TAP_X=$(python3 -c "print(max(0, min($SCREEN_W, int($TAP_X))))")
                TAP_Y=$(python3 -c "print(max(0, min($SCREEN_H, int($TAP_Y))))")
                LAST_ACTION_DESC="tap ($TAP_X,$TAP_Y)"
                $DRY_RUN || adb shell input tap "$TAP_X" "$TAP_Y"
            fi
            ;;
    esac

    echo "  Waiting ${WAIT_SEC}s..."
    sleep "$WAIT_SEC"
done

echo ""
echo "=== FAILED: Did not reach '$TARGET' in $MAX_STEPS steps ==="
echo "  Screenshots in: $SCREENSHOT_DIR/"
exit 1
