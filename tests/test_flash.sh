#!/bin/bash
# 彩闪自动验证脚本：录屏 + 回放场景跳转 + 逐帧分析
# 用法: bash tests/test_flash.sh [--no-gemma4] [--extra-wait N]
set -e

VIDEO_PATH="/sdcard/flash_test.mp4"
LOCAL_VIDEO="/tmp/flash_test.mp4"
FRAMES_DIR="/tmp/flash_frames"
PACKAGE="com.labolado.tank"
ACTIVITY="com.ansca.corona.CoronaActivity"
RECORDING_DIR="/sdcard/Android/data/$PACKAGE/files/recordings"
EXTRA_WAIT=${2:-8}
USE_GEMMA4=true

for arg in "$@"; do
    case $arg in
        --no-gemma4) USE_GEMMA4=false ;;
        --extra-wait) shift; EXTRA_WAIT=$1 ;;
    esac
done

echo "=== 彩闪自动验证 ==="

# 0. 确保 app 已安装且在前台
echo "[0] 启动应用..."
adb shell am force-stop "$PACKAGE" 2>/dev/null || true
sleep 1
adb shell am start -n "$PACKAGE/$ACTIVITY"
sleep 8  # 等 home 场景加载完成

# 1. 清理旧录像
adb shell rm -f "$VIDEO_PATH" 2>/dev/null || true

# 2. 开始屏幕录制（后台，最长60秒）
echo "[1] 开始屏幕录制..."
adb shell screenrecord --time-limit 60 "$VIDEO_PATH" &
RECORD_PID=$!
sleep 2  # 等 screenrecord 就绪

# 3. 从录制 JSON 回放场景跳转
RECORDING=$(adb shell ls -t "$RECORDING_DIR/" 2>/dev/null | grep '\.json$' | head -1 | tr -d '\r')
if [ -z "$RECORDING" ]; then
    echo "ERROR: 没有找到录制文件在 $RECORDING_DIR/"
    echo "请先在设备上录制操作序列（home→gallery→edit→design→game）"
    exit 1
fi
echo "[2] 使用录制: $RECORDING"

# 解析录制 JSON，按时间差回放 tap
adb shell cat "$RECORDING_DIR/$RECORDING" | python3 -c "
import json, sys, subprocess, time

data = json.load(sys.stdin)
events = data['events']

# 提取 tap 事件（只取 began 阶段的坐标）
taps = []
for e in events:
    if e['phase'] == 'began':
        taps.append((e['time'], int(e['x']), int(e['y'])))

print(f'  共 {len(taps)} 个 tap 事件')

prev_time = None
for i, (t, x, y) in enumerate(taps):
    if prev_time is not None:
        delay = (t - prev_time) / 1000.0
        print(f'  等待 {delay:.1f}s...')
        time.sleep(delay)
    print(f'  tap #{i+1}: ({x},{y}) @ {t:.0f}ms')
    subprocess.run(['adb', 'shell', 'input', 'tap', str(x), str(y)])
    prev_time = t

# 等游戏场景完全加载
import os
extra = int(os.environ.get('EXTRA_WAIT', 8))
print(f'  等待游戏场景加载 ({extra}s)...')
time.sleep(extra)
"

# 4. 停止录制
echo "[3] 停止屏幕录制..."
adb shell pkill -SIGINT screenrecord 2>/dev/null || true
wait $RECORD_PID 2>/dev/null || true
sleep 2  # 等文件写入完成

# 5. 拉取视频
echo "[4] 拉取视频..."
adb pull "$VIDEO_PATH" "$LOCAL_VIDEO"

if [ ! -f "$LOCAL_VIDEO" ]; then
    echo "ERROR: 视频拉取失败"
    exit 1
fi

FILE_SIZE=$(stat -f%z "$LOCAL_VIDEO" 2>/dev/null || stat -c%s "$LOCAL_VIDEO" 2>/dev/null)
echo "  视频大小: ${FILE_SIZE} bytes"

if [ "$FILE_SIZE" -lt 10000 ]; then
    echo "ERROR: 视频太小，可能录制失败"
    exit 1
fi

# 6. 逐帧分析：检测1帧闪烁
echo "[5] 逐帧分析闪烁..."
DURATION=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$LOCAL_VIDEO" | cut -d. -f1)
# 抽最后 10 秒的帧（场景切换发生在这段）
START=$((DURATION > 10 ? DURATION - 10 : 0))
rm -rf "$FRAMES_DIR" && mkdir -p "$FRAMES_DIR"
ffmpeg -y -ss "$START" -i "$LOCAL_VIDEO" -vf fps=30 "$FRAMES_DIR/frame_%04d.png" 2>/dev/null

FRAME_COUNT=$(ls "$FRAMES_DIR"/frame_*.png 2>/dev/null | wc -l | tr -d ' ')
echo "  抽取 $FRAME_COUNT 帧 (${START}s-${DURATION}s)"

# 逐帧像素差异分析 + 地平线检测
python3 << 'PYEOF'
import os, glob, sys
from PIL import Image
import numpy as np

frames_dir = "/tmp/flash_frames"
files = sorted(glob.glob(f"{frames_dir}/frame_*.png"))
if not files:
    print("  ERROR: 没有帧文件")
    sys.exit(1)

prev = None
flash_frames = []
for i, f in enumerate(files):
    img = np.array(Image.open(f).convert("RGB"))

    # 测量地平线位置（中间列）
    horizon = -1
    for y in range(img.shape[0]):
        r, g, b = img[y, img.shape[1]//2, :]
        if g < 180 and b < 200:  # 非天空
            horizon = y
            break

    if prev is not None:
        diff = np.mean(np.abs(img.astype(float) - prev.astype(float)))
        if diff > 30:
            fname = os.path.basename(f)
            t = float(os.environ.get('START', 0)) + i / 30.0
            flash_frames.append((i, fname, diff, horizon, t))
    prev = img

if flash_frames:
    print(f"\n  *** 检测到 {len(flash_frames)} 个突变帧:")
    for idx, fname, diff, hz, t in flash_frames:
        print(f"    帧 {idx} ({fname}): 差异={diff:.1f}, 地平线y={hz}, 时间={t:.2f}s")
    print("\n  结论: 彩闪仍然存在!")
    sys.exit(2)  # 返回非零表示有闪烁
else:
    print("\n  结论: 未检测到闪烁，修复成功!")
PYEOF
FLASH_RESULT=$?

# 7. 可选 gemma4 深度分析
if [ "$USE_GEMMA4" = true ] && [ $FLASH_RESULT -ne 0 ] && [ -d "$FRAMES_DIR" ]; then
    echo ""
    echo "[6] gemma4 分析闪烁帧..."
    # 找到突变帧前后帧
    FLASH_IDX=$(python3 -c "
import glob, os
from PIL import Image
import numpy as np
files = sorted(glob.glob('$FRAMES_DIR/frame_*.png'))
prev = None
for i, f in enumerate(files):
    img = np.array(Image.open(f).convert('RGB'))
    if prev is not None:
        diff = np.mean(np.abs(img.astype(float) - prev.astype(float)))
        if diff > 30:
            print(i)
            break
    prev = img
")
    if [ -n "$FLASH_IDX" ]; then
        BEFORE=$((FLASH_IDX - 1))
        AFTER=$((FLASH_IDX + 1))
        F_BEFORE=$(printf "frame_%04d.png" $BEFORE)
        F_FLASH=$(printf "frame_%04d.png" $FLASH_IDX)
        F_AFTER=$(printf "frame_%04d.png" $AFTER)
        bash ~/.claude/skills/gemma4/scripts/gemma4-ask.sh -m 500 \
            "3帧截图：正常帧→闪烁帧→恢复帧。描述闪烁帧异常：地平线位移、坦克位置/大小变化、颜色异常。" \
            "$FRAMES_DIR/$F_BEFORE" "$FRAMES_DIR/$F_FLASH" "$FRAMES_DIR/$F_AFTER"
    fi
fi

echo ""
echo "=== 验证完成 ==="
echo "视频: $LOCAL_VIDEO"
echo "帧目录: $FRAMES_DIR"
exit $FLASH_RESULT
