#!/bin/bash
# Android 一键测试循环：编译 → 安装 → replay → 截图 → 日志
# 用法:
#   bash tests/android_test_cycle.sh [OPTIONS]
#
# 模式:
#   --build          编译 Corona.aar + CoronaBuilder 打包 + 安装（默认跳过，用设备上已有 APK）
#   --build-official 用官方 Corona SDK 打包安装（对比基准）
#   --replay         回放 InputRecorder 录制并截图（默认开启）
#   --no-replay      跳过回放
#   --screenshot     仅截图，不回放
#
# 示例:
#   bash tests/android_test_cycle.sh                    # 直接 replay 现有 APK
#   bash tests/android_test_cycle.sh --build            # 编译+安装+replay
#   bash tests/android_test_cycle.sh --build-official   # 用官方 SDK 打包对比
#   bash tests/android_test_cycle.sh --screenshot       # 仅截图
#
# 依赖: adb, ffmpeg, python3
# 参考: tests/INPUT_RECORDER.md, tests/android_replay.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORONA_DIR="${CORONA_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
PACKAGE="${PACKAGE:-com.labolado.tank}"
PROJECT="${PROJECT:-/Users/yee/data/dev/app/labo/tank_test_copy}"
OUTPUT_DIR="${OUTPUT_DIR:-/tmp/android_test_cycle}"
OFFICIAL_SDK="${OFFICIAL_SDK:-/Applications/Corona}"

DO_BUILD=false
DO_BUILD_OFFICIAL=false
DO_REPLAY=true
DO_SCREENSHOT_ONLY=false
EXTRA_WAIT=8

while [[ $# -gt 0 ]]; do
    case $1 in
        --build) DO_BUILD=true; shift ;;
        --build-official) DO_BUILD_OFFICIAL=true; shift ;;
        --replay) DO_REPLAY=true; shift ;;
        --no-replay) DO_REPLAY=false; shift ;;
        --screenshot) DO_SCREENSHOT_ONLY=true; DO_REPLAY=false; shift ;;
        --package) PACKAGE="$2"; shift 2 ;;
        --project) PROJECT="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --extra-wait) EXTRA_WAIT="$2"; shift 2 ;;
        *) echo "未知参数: $1"; exit 1 ;;
    esac
done

mkdir -p "$OUTPUT_DIR"
log() { echo "[$(date '+%H:%M:%S')] $1"; }

# === 检查设备 ===
ADB_DEVICE=$(adb devices 2>/dev/null | grep -v "List" | grep "device$" | head -1 | awk '{print $1}')
if [ -z "$ADB_DEVICE" ]; then
    echo "ERROR: 没有 Android 设备连接"
    exit 1
fi
log "设备: $ADB_DEVICE"

# === Step 1: 编译安装（可选） ===
if [ "$DO_BUILD" = true ]; then
    log "=== 编译 bgfx 版 APK ==="
    cd "$CORONA_DIR"
    FORCE_BUILD=1 bash tests/build_android.sh "$PROJECT" "$PACKAGE"
    log "编译安装完成"
fi

if [ "$DO_BUILD_OFFICIAL" = true ]; then
    log "=== 用官方 SDK 打包 ==="
    OFFICIAL_BUILDER="$OFFICIAL_SDK/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder"
    if [ ! -f "$OFFICIAL_BUILDER" ]; then
        # 尝试其他路径
        OFFICIAL_BUILDER="/Applications/Corona-b3/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder"
    fi
    if [ ! -f "$OFFICIAL_BUILDER" ]; then
        log "ERROR: 找不到官方 CoronaBuilder"
        log "  尝试路径: $OFFICIAL_SDK, /Applications/Corona-b3"
        exit 1
    fi

    ABS_PROJECT=$(cd "$PROJECT" && pwd)
    APP_NAME="$(basename "$PROJECT")"
    DST_OFFICIAL="/tmp/android-official-$(date +%H%M%S)"
    mkdir -p "$DST_OFFICIAL"

    cat > /tmp/build-official.lua << LUAEOF
local params = {
    platform = 'android',
    appName = '$APP_NAME',
    appVersion = '1.0',
    dstPath = '$DST_OFFICIAL',
    projectPath = '$ABS_PROJECT',
    androidAppPackage = '${PACKAGE}.official',
    keystorePath = '$HOME/.android/debug.keystore',
    keystorePassword = 'android',
    keystoreAlias = 'androiddebugkey',
    keystoreAliasPassword = 'android',
}
return params
LUAEOF

    "$OFFICIAL_BUILDER" build --lua /tmp/build-official.lua 2>&1 | grep -E "succeeded|failed|error" | head -5
    OFFICIAL_APK=$(find "$DST_OFFICIAL" -name "*.apk" 2>/dev/null | head -1)
    if [ -f "$OFFICIAL_APK" ]; then
        log "官方 APK: $OFFICIAL_APK"
        adb install -r "$OFFICIAL_APK" 2>&1 | grep -E "Success|Failure"
    else
        log "ERROR: 官方 SDK 打包失败"
    fi
fi

# === Step 2: 检查日志（启动验证） ===
log "=== 启动验证 ==="
adb shell am force-stop "$PACKAGE" 2>/dev/null || true
adb logcat -c 2>/dev/null
sleep 1
adb shell am start -n "$PACKAGE/com.ansca.corona.CoronaActivity"
sleep 8

# 检查启动日志
log "检查启动日志..."
ERRORS=$(adb logcat -d -s Corona:V 2>/dev/null | grep -E 'stack traceback|attempt to' | grep -v 'WARNING\|require.*path\|case-sensitive' | head -5)
if [ -n "$ERRORS" ]; then
    log "WARNING: 发现 Lua 错误:"
    echo "$ERRORS"
    echo "$ERRORS" > "$OUTPUT_DIR/startup_errors.txt"
fi

# === Step 3: Replay（可选） ===
if [ "$DO_REPLAY" = true ]; then
    log "=== 回放录制 ==="
    cd "$CORONA_DIR"
    bash tests/android_replay.sh \
        --package "$PACKAGE" \
        --no-restart \
        --no-frames \
        --output-dir "$OUTPUT_DIR" \
        --screenshot "$OUTPUT_DIR/after_replay.png" \
        --extra-wait "$EXTRA_WAIT" \
        --validate
    log "回放完成"
fi

# === Step 4: 截图 ===
if [ "$DO_SCREENSHOT_ONLY" = true ]; then
    log "=== 截图 ==="
    adb shell screencap -p /sdcard/cycle_screenshot.png
    adb pull /sdcard/cycle_screenshot.png "$OUTPUT_DIR/screenshot.png"
    log "截图: $OUTPUT_DIR/screenshot.png"
fi

# === Step 5: 抓日志 ===
log "=== 保存日志 ==="
adb logcat -d -s Corona:V > "$OUTPUT_DIR/logcat.txt" 2>/dev/null
LINES=$(wc -l < "$OUTPUT_DIR/logcat.txt" | tr -d ' ')
log "日志: $OUTPUT_DIR/logcat.txt ($LINES 行)"

# 提取关键信息
grep -E 'bgfx|BGFX|backend|Backend' "$OUTPUT_DIR/logcat.txt" | head -5 > "$OUTPUT_DIR/bgfx_info.txt" 2>/dev/null
grep -E 'ERROR|error|stack traceback' "$OUTPUT_DIR/logcat.txt" | grep -v 'AudioTrack\|MediaCodec' | head -10 > "$OUTPUT_DIR/errors.txt" 2>/dev/null

ERROR_COUNT=$(wc -l < "$OUTPUT_DIR/errors.txt" | tr -d ' ')
log "错误数: $ERROR_COUNT"

# === 汇总 ===
echo ""
log "=== 测试完成 ==="
echo "  输出目录: $OUTPUT_DIR"
[ -f "$OUTPUT_DIR/after_replay.png" ] && echo "  回放截图: $OUTPUT_DIR/after_replay.png"
[ -f "$OUTPUT_DIR/screenshot.png" ] && echo "  截图: $OUTPUT_DIR/screenshot.png"
echo "  日志: $OUTPUT_DIR/logcat.txt"
echo "  错误: $OUTPUT_DIR/errors.txt ($ERROR_COUNT 条)"
[ -f "$OUTPUT_DIR/startup_errors.txt" ] && echo "  启动错误: $OUTPUT_DIR/startup_errors.txt"
