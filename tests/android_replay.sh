#!/bin/bash
# 通用 Android 录屏回放脚本
# 用法: bash tests/android_replay.sh [OPTIONS]
#
# 功能：启动 app → 回放 InputRecorder 录制 → 录屏 → 抽帧
# 输出：视频文件 + 帧目录，供后续分析脚本使用
#
# 选项:
#   --package PKG        包名（默认 com.labolado.tank）
#   --activity ACT       Activity（默认 com.ansca.corona.CoronaActivity）
#   --output-dir DIR     输出目录（默认 /tmp/android_replay）
#   --extra-wait N       回放后额外等待秒数（默认 8）
#   --fps N              抽帧帧率（默认 30）
#   --duration N         录屏最长秒数（默认 60）
#   --last-seconds N     分析最后 N 秒的帧（默认 10，0=全部）
#   --no-restart         不重启 app（用于多次回放场景）
#   --no-frames          不抽帧（只要视频）
#   --screenshot FILE    回放结束后截图保存到 FILE
#   --validate           回放前验证录制文件完整性
#
# 依赖: adb, ffmpeg, python3
# 参考: tests/INPUT_RECORDER.md（录制回放完整文档）

set -e

# === 默认参数 ===
PACKAGE="com.labolado.tank"
ACTIVITY="com.ansca.corona.CoronaActivity"
OUTPUT_DIR="/tmp/android_replay"
EXTRA_WAIT=8
FPS=30
DURATION=60
LAST_SECONDS=10
RESTART=true
EXTRACT_FRAMES=true
SCREENSHOT=""
VALIDATE=false

# === 解析参数 ===
while [[ $# -gt 0 ]]; do
    case $1 in
        --package) PACKAGE="$2"; shift 2 ;;
        --activity) ACTIVITY="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --extra-wait) EXTRA_WAIT="$2"; shift 2 ;;
        --fps) FPS="$2"; shift 2 ;;
        --duration) DURATION="$2"; shift 2 ;;
        --last-seconds) LAST_SECONDS="$2"; shift 2 ;;
        --no-restart) RESTART=false; shift ;;
        --no-frames) EXTRACT_FRAMES=false; shift ;;
        --screenshot) SCREENSHOT="$2"; shift 2 ;;
        --validate) VALIDATE=true; shift ;;
        *) echo "未知参数: $1"; exit 1 ;;
    esac
done

RECORDING_DIR="/sdcard/Android/data/$PACKAGE/files/recordings"
VIDEO_PATH="/sdcard/replay_video.mp4"
LOCAL_VIDEO="$OUTPUT_DIR/video.mp4"
FRAMES_DIR="$OUTPUT_DIR/frames"

mkdir -p "$OUTPUT_DIR"

# === 1. 查找录制文件 ===
RECORDING=$(adb shell ls -t "$RECORDING_DIR/" 2>/dev/null | grep '\.json$' | head -1 | tr -d '\r')
if [ -z "$RECORDING" ]; then
    # 尝试从本地备份恢复
    LOCAL_BACKUP_DIR="${CORONA_DIR:-$(cd "$(dirname "$0")/.." && pwd)}/tests/recordings/$PACKAGE"
    LOCAL_REC=$(ls -t "$LOCAL_BACKUP_DIR/"*.json 2>/dev/null | head -1)
    if [ -n "$LOCAL_REC" ]; then
        RECORDING=$(basename "$LOCAL_REC")
        echo "[恢复] 设备无录制，从本地备份恢复: $RECORDING"
        adb shell mkdir -p "$RECORDING_DIR"
        adb push "$LOCAL_REC" "$RECORDING_DIR/$RECORDING" > /dev/null 2>&1
    else
        echo "ERROR: 没有找到录制文件在 $RECORDING_DIR/ 或本地备份"
        echo ""
        echo "录制方法："
        echo "  1. adb shell mkdir -p $RECORDING_DIR"
        echo "  2. adb shell touch $RECORDING_DIR/RECORD"
        echo "  3. 启动 app，在设备上操作完整流程"
        echo "  4. InputRecorder 自动录制，触发文件会被自动删除"
        echo "  5. 再次运行本脚本"
        echo ""
        echo "详细文档: tests/INPUT_RECORDER.md"
        exit 1
    fi
fi
echo "[录制] 使用: $RECORDING"

# === 1b. 本地备份录制文件（防止 APK 重装丢失） ===
BACKUP_DIR="${CORONA_DIR:-$(cd "$(dirname "$0")/.." && pwd)}/tests/recordings/$PACKAGE"
mkdir -p "$BACKUP_DIR"
if [ ! -f "$BACKUP_DIR/$RECORDING" ]; then
    adb shell cat "$RECORDING_DIR/$RECORDING" > "$BACKUP_DIR/$RECORDING" 2>/dev/null
    echo "[备份] $BACKUP_DIR/$RECORDING"
else
    echo "[备份] 已存在: $RECORDING"
fi

# === 2. 验证录制文件（可选） ===
if [ "$VALIDATE" = true ]; then
    echo "[验证] 检查录制文件完整性..."
    adb shell cat "$RECORDING_DIR/$RECORDING" | python3 -c "
import json, sys
data = json.load(sys.stdin)
events = data['events']
taps = [(e['time'], int(e['x']), int(e['y'])) for e in events if e['phase'] == 'began']
print(f'  tap 数量: {len(taps)}')
for i, (t, x, y) in enumerate(taps):
    print(f'    #{i+1}: ({x},{y}) @ {t:.0f}ms')
if len(taps) < 3:
    print('  WARNING: tap 数量过少，录制可能不完整!')
    sys.exit(1)
meta = data.get('meta', {})
if meta:
    print(f'  平台: {meta.get(\"platform\", \"?\")}')
    print(f'  屏幕: {meta.get(\"screenWidth\", \"?\")}x{meta.get(\"screenHeight\", \"?\")}')
print('  验证通过')
"
    VALIDATE_RESULT=$?
    if [ $VALIDATE_RESULT -ne 0 ]; then
        echo "ERROR: 录制文件验证失败"
        exit 1
    fi
fi

# === 3. 重启 app（可选） ===
if [ "$RESTART" = true ]; then
    echo "[启动] 重启 $PACKAGE..."
    adb shell am force-stop "$PACKAGE" 2>/dev/null || true
    sleep 1
    adb shell am start -n "$PACKAGE/$ACTIVITY"
    sleep 8  # 等 home 场景加载完成
fi

# === 4. 开始录屏 ===
adb shell rm -f "$VIDEO_PATH" 2>/dev/null || true
echo "[录屏] 开始（最长 ${DURATION}s）..."
adb shell screenrecord --time-limit "$DURATION" "$VIDEO_PATH" &
RECORD_PID=$!
sleep 2  # 等 screenrecord 就绪

# === 5. 回放录制 ===
echo "[回放] 回放触控事件..."
adb shell cat "$RECORDING_DIR/$RECORDING" | EXTRA_WAIT=$EXTRA_WAIT python3 -c "
import json, sys, subprocess, time, os

data = json.load(sys.stdin)
events = data['events']

# 提取 tap 事件（只取 began 阶段）
# 坐标转换：InputRecorder 录制的是 content 坐标（如 2732x2048），
# adb input tap 需要物理屏幕坐标（如 1340x800）
# 转换公式：physicalX = contentX * (physicalWidth / contentWidth)

# 获取物理屏幕尺寸（landscape）
result = subprocess.run(['adb', 'shell', 'wm', 'size'], capture_output=True, text=True)
phys_match = __import__('re').search(r'(\d+)x(\d+)', result.stdout)
if phys_match:
    pw, ph = int(phys_match.group(1)), int(phys_match.group(2))
    # 确保是 landscape（宽 > 高）
    phys_w, phys_h = max(pw, ph), min(pw, ph)
else:
    phys_w, phys_h = 1340, 800  # fallback

# 获取 content 尺寸（从录制 meta）
meta = data.get('meta', {})
content_w = meta.get('screenWidth', 0)
content_h = meta.get('screenHeight', 0)

# 计算缩放比
if content_w > 0 and content_h > 0:
    scale_x = phys_w / content_w
    scale_y = phys_h / content_h
    print(f'  坐标转换: content({content_w}x{content_h}) -> physical({phys_w}x{phys_h}), scale=({scale_x:.4f}, {scale_y:.4f})')
else:
    scale_x, scale_y = 1.0, 1.0
    print(f'  WARNING: 录制文件无 meta 信息，跳过坐标转换 (physical={phys_w}x{phys_h})')

taps = []
for e in events:
    if e['phase'] == 'began':
        taps.append((e['time'], int(e['x']), int(e['y'])))

print(f'  共 {len(taps)} 个 tap 事件')

prev_time = None
for i, (t, x, y) in enumerate(taps):
    if prev_time is not None:
        delay = (t - prev_time) / 1000.0
        time.sleep(delay)
    # 转换 content 坐标到物理屏幕坐标
    px, py = int(x * scale_x), int(y * scale_y)
    print(f'  tap #{i+1}: content({x},{y}) -> physical({px},{py}) @ {t:.0f}ms')
    subprocess.run(['adb', 'shell', 'input', 'tap', str(px), str(py)])
    prev_time = t

extra = int(os.environ.get('EXTRA_WAIT', 8))
print(f'  等待场景加载 ({extra}s)...')
time.sleep(extra)
"

# === 6. 截图（可选） ===
if [ -n "$SCREENSHOT" ]; then
    echo "[截图] 保存到 $SCREENSHOT"
    adb shell screencap -p /sdcard/replay_screenshot.png
    adb pull /sdcard/replay_screenshot.png "$SCREENSHOT"
fi

# === 7. 停止录屏 ===
echo "[录屏] 停止..."
adb shell pkill -SIGINT screenrecord 2>/dev/null || true
wait $RECORD_PID 2>/dev/null || true
sleep 2

# === 8. 拉取视频 ===
echo "[拉取] 视频..."
adb pull "$VIDEO_PATH" "$LOCAL_VIDEO"

if [ ! -f "$LOCAL_VIDEO" ]; then
    echo "ERROR: 视频拉取失败"
    exit 1
fi

FILE_SIZE=$(stat -f%z "$LOCAL_VIDEO" 2>/dev/null || stat -c%s "$LOCAL_VIDEO" 2>/dev/null)
echo "  视频: $LOCAL_VIDEO (${FILE_SIZE} bytes)"

# === 9. 抽帧（可选） ===
if [ "$EXTRACT_FRAMES" = true ]; then
    TOTAL_DURATION=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$LOCAL_VIDEO" | cut -d. -f1)
    if [ "$LAST_SECONDS" -gt 0 ] && [ "$TOTAL_DURATION" -gt "$LAST_SECONDS" ]; then
        START=$((TOTAL_DURATION - LAST_SECONDS))
    else
        START=0
    fi

    rm -rf "$FRAMES_DIR" && mkdir -p "$FRAMES_DIR"
    ffmpeg -y -ss "$START" -i "$LOCAL_VIDEO" -vf fps=$FPS "$FRAMES_DIR/frame_%04d.png" 2>/dev/null

    FRAME_COUNT=$(ls "$FRAMES_DIR"/frame_*.png 2>/dev/null | wc -l | tr -d ' ')
    echo "  帧: $FRAMES_DIR ($FRAME_COUNT 帧, ${START}s-${TOTAL_DURATION}s, ${FPS}fps)"
fi

echo ""
echo "=== 回放完成 ==="
echo "视频: $LOCAL_VIDEO"
[ "$EXTRACT_FRAMES" = true ] && echo "帧目录: $FRAMES_DIR"
[ -n "$SCREENSHOT" ] && echo "截图: $SCREENSHOT"
