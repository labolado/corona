#!/bin/bash
# 快速多平台测试脚本：编译 + 安装 + 运行 + 截图 + 日志检查
# 一条命令完成，不浪费 token
#
# 用法:
#   bash tests/quick_test.sh mac tank          # macOS 跑坦克（GL + bgfx 双后端）
#   bash tests/quick_test.sh mac mech          # macOS 跑机械工作室
#   bash tests/quick_test.sh mac demo          # macOS 跑 bgfx-demo
#   bash tests/quick_test.sh mac tank bgfx     # macOS 只跑 bgfx
#   bash tests/quick_test.sh mac tank gl       # macOS 只跑 GL
#   bash tests/quick_test.sh android tank      # Android 编译+安装+运行（GLES + Vulkan）
#   bash tests/quick_test.sh android demo gles # Android 只跑 GLES
#   bash tests/quick_test.sh android tank vulkan # Android 只跑 Vulkan
#   bash tests/quick_test.sh ios tank          # iOS 编译+安装+运行
#   bash tests/quick_test.sh build mac         # 只编译 macOS，不运行
#   bash tests/quick_test.sh build android     # 只编译 Android AAR，不打包
#
# 选项:
#   --no-build     跳过编译（用上次的构建产物）
#   --no-screenshot 不截图
#   --device SERIAL 指定 Android 设备
#   --wait N        启动后等 N 秒再截图（默认 8）
#   --test NAME     指定 SOLAR2D_TEST 入口（如 perf, bench, regression）

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORONA_DIR="${SCRIPT_DIR}/.."
cd "$CORONA_DIR"

# ============================================================
# 项目配置
# ============================================================
get_project_path() {
    case "$1" in
        tank) echo "/Users/yee/data/dev/app/labo/tank_test_copy" ;;
        mech) echo "/Users/yee/data/dev/app/labo/mech_test_copy" ;;
        demo) echo "${CORONA_DIR}/tests/bgfx-demo" ;;
        *)    echo "" ;;
    esac
}

get_android_package() {
    case "$1" in
        tank) echo "com.labolado.tank" ;;
        mech) echo "com.labolado.mech" ;;
        demo) echo "com.labolado.bgfxdemo" ;;
        *)    echo "" ;;
    esac
}

SIM="${CORONA_DIR}/platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator"
OUTPUT_DIR="/tmp/quick_test_$(date +%Y%m%d_%H%M%S)"

# ============================================================
# 参数解析
# ============================================================
PLATFORM="${1:?用法: bash tests/quick_test.sh <mac|android|ios|build> <tank|mech|demo> [backend] [options]}"
PROJECT="${2:-demo}"
BACKEND="both"
NO_BUILD=false
NO_SCREENSHOT=false
DEVICE_SERIAL=""
WAIT_SECS=8
TEST_ENTRY=""

shift 2 || true

while [[ $# -gt 0 ]]; do
    case "$1" in
        gl|bgfx|gles|vulkan|both) BACKEND="$1"; shift ;;
        --no-build) NO_BUILD=true; shift ;;
        --no-screenshot) NO_SCREENSHOT=true; shift ;;
        --device) DEVICE_SERIAL="$2"; shift 2 ;;
        --wait) WAIT_SECS="$2"; shift 2 ;;
        --test) TEST_ENTRY="$2"; shift 2 ;;
        *) shift ;;
    esac
done

if [ "$PLATFORM" != "build" ]; then
    PROJECT_PATH="$(get_project_path "$PROJECT")"
    [ -z "$PROJECT_PATH" ] && { echo "ERROR: 未知项目 '$PROJECT'，可选: tank, mech, demo"; exit 1; }
    [ ! -d "$PROJECT_PATH" ] && { echo "ERROR: 项目目录不存在: $PROJECT_PATH"; exit 1; }
else
    PROJECT_PATH=""
fi

mkdir -p "$OUTPUT_DIR"

log() { echo "[$(date '+%H:%M:%S')] $1"; }
fail() { echo "FAILED: $1"; exit 1; }

# ============================================================
# 编译函数
# ============================================================
build_mac() {
    log "编译 macOS Debug..."
    xcodebuild -project platform/mac/ratatouille.xcodeproj -target rttplayer -configuration Debug build \
        CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO ARCHS=arm64 ONLY_ACTIVE_ARCH=NO \
        2>&1 | tail -5
    [ ${PIPESTATUS[0]} -eq 0 ] && log "macOS 编译成功" || fail "macOS 编译失败"
}

build_android() {
    log "编译 Android Release..."
    cd platform/android
    ./gradlew :Corona:clean :Corona:assembleRelease --no-daemon 2>&1 | tail -10
    local result=${PIPESTATUS[0]}
    cd "$CORONA_DIR"
    [ $result -eq 0 ] && log "Android AAR 编译成功" || fail "Android AAR 编译失败"
}

build_ios() {
    log "编译 iOS..."
    bash tests/build_ios.sh 2>&1 | tail -10
    [ ${PIPESTATUS[0]} -eq 0 ] && log "iOS 编译成功" || fail "iOS 编译失败"
}

# ============================================================
# macOS 测试
# ============================================================
run_mac() {
    local backend="$1"  # gl or bgfx
    local env_backend
    [ "$backend" = "gl" ] && env_backend="gl" || env_backend="bgfx"

    log "macOS: 启动 $PROJECT ($env_backend)..."

    # 杀残留（bgfx 是单例，残留进程会导致 init 失败白屏）
    pkill -f 'Corona Simulator' 2>/dev/null || true
    pkill -f 'corona/platform.*lua' 2>/dev/null || true
    sleep 2

    local logfile="$OUTPUT_DIR/${PROJECT}_${backend}_mac.log"
    local screenshot="$OUTPUT_DIR/${PROJECT}_${backend}_mac.png"

    # 构建环境变量
    local env_vars="SOLAR2D_BACKEND=$env_backend"
    [ -n "$TEST_ENTRY" ] && env_vars="$env_vars SOLAR2D_TEST=$TEST_ENTRY"

    env $env_vars "$SIM" -no-console YES "$PROJECT_PATH" > "$logfile" 2>&1 &
    local pid=$!

    sleep "$WAIT_SECS"

    # 检查日志错误
    if grep -q 'stack traceback\|attempt to' "$logfile" 2>/dev/null; then
        log "WARNING: $backend 日志有错误:"
        grep 'attempt to\|stack traceback\|ERROR' "$logfile" | grep -v 'WARNING\|require.*path\|case-sensitive' | head -5
    else
        log "$backend 日志无致命错误"
    fi

    # 截图（找模拟器窗口 = 最小的 Corona 窗口，跳过欢迎窗口）
    if [ "$NO_SCREENSHOT" != "true" ]; then
        python3 << PYEOF
import Quartz
from Cocoa import NSBitmapImageRep
wl = Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID)
best = None
for w in wl:
    if 'Corona' in str(w.get('kCGWindowOwnerName', '')):
        bounds = w.get('kCGWindowBounds', {})
        ww = int(bounds.get('Width', 0))
        if ww > 50 and (best is None or ww < int(best.get('kCGWindowBounds', {}).get('Width', 9999))):
            best = w
if best:
    wid = best['kCGWindowNumber']
    img = Quartz.CGWindowListCreateImage(Quartz.CGRectNull, Quartz.kCGWindowListOptionIncludingWindow, wid, Quartz.kCGWindowImageDefault)
    if img:
        rep = NSBitmapImageRep.alloc().initWithCGImage_(img)
        data = rep.representationUsingType_properties_(4, None)
        data.writeToFile_atomically_('${screenshot}', True)
        print('截图: ${screenshot}')
PYEOF
    fi

    # 停止
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true
    log "macOS $backend 完成"
}

test_mac() {
    if [ "$NO_BUILD" != "true" ]; then
        build_mac
    fi

    case "$BACKEND" in
        gl)    run_mac gl ;;
        bgfx)  run_mac bgfx ;;
        both)  run_mac gl; run_mac bgfx ;;
        *)     run_mac gl; run_mac bgfx ;;
    esac
}

# ============================================================
# Android 测试
# ============================================================
adb_cmd() {
    if [ -n "$DEVICE_SERIAL" ]; then
        adb -s "$DEVICE_SERIAL" "$@"
    else
        adb "$@"
    fi
}

install_android() {
    local pkg="$(get_android_package "$PROJECT")"
    log "Android: 打包安装 $PROJECT (pkg=$pkg)..."

    if [ -n "$DEVICE_SERIAL" ]; then
        ANDROID_SERIAL="$DEVICE_SERIAL" bash tests/build_android.sh "$PROJECT_PATH" "$pkg" 2>&1 | tail -10
    else
        bash tests/build_android.sh "$PROJECT_PATH" "$pkg" 2>&1 | tail -10
    fi
    [ ${PIPESTATUS[0]} -eq 0 ] && log "Android 安装成功" || fail "Android 安装失败"
}

run_android() {
    local backend="$1"  # gles or vulkan
    local pkg="$(get_android_package "$PROJECT")"
    local logfile="$OUTPUT_DIR/${PROJECT}_${backend}_android.log"
    local screenshot="$OUTPUT_DIR/${PROJECT}_${backend}_android.png"

    log "Android: 启动 $PROJECT ($backend)..."

    adb_cmd logcat -c 2>/dev/null
    adb_cmd shell am force-stop "$pkg" 2>/dev/null

    # 设置 Vulkan 环境（通过 setprop 不可行，用文件标记）
    if [ "$backend" = "vulkan" ]; then
        # 创建标记文件让引擎选 Vulkan
        adb_cmd shell "run-as $pkg sh -c 'echo 1 > /data/data/$pkg/files/solar2d_vulkan'" 2>/dev/null || true
    else
        adb_cmd shell "run-as $pkg sh -c 'rm -f /data/data/$pkg/files/solar2d_vulkan'" 2>/dev/null || true
    fi

    adb_cmd shell am start -n "$pkg/com.ansca.corona.CoronaActivity"
    sleep "$WAIT_SECS"

    # 抓日志
    adb_cmd logcat -d -s Corona:V > "$logfile" 2>/dev/null

    # 检查 VulkanProbe 输出
    log "VulkanProbe 输出:"
    grep -iE 'VulkanProbe|BgfxRenderer.*auto-detect|BgfxRenderer.*SOLAR2D_VULKAN|BGFX_INIT' "$logfile" | head -10

    # 检查错误
    local errors
    errors=$(grep -iE 'error|crash|fatal|SIGSEGV|SIGABRT' "$logfile" | grep -v 'AudioTrack\|MediaCodec\|InputTransport\|eglCodecCommon' | head -5)
    if [ -n "$errors" ]; then
        log "WARNING: $backend 日志有错误:"
        echo "$errors"
    else
        log "$backend 日志无致命错误"
    fi

    # 截图
    if [ "$NO_SCREENSHOT" != "true" ]; then
        adb_cmd exec-out screencap -p > "$screenshot" 2>/dev/null
        [ -s "$screenshot" ] && log "截图: $screenshot" || log "截图失败"
    fi

    log "Android $backend 完成"
}

test_android() {
    if [ "$NO_BUILD" != "true" ]; then
        build_android
    fi

    # 列出连接的设备
    log "连接的 Android 设备:"
    adb devices | grep -v '^List' | grep device

    install_android

    case "$BACKEND" in
        gles)   run_android gles ;;
        vulkan) run_android vulkan ;;
        both)   run_android gles; run_android vulkan ;;
        *)      run_android gles; run_android vulkan ;;
    esac
}

# ============================================================
# Android 多设备测试
# ============================================================
test_android_all_devices() {
    if [ "$NO_BUILD" != "true" ]; then
        build_android
    fi

    local devices
    devices=$(adb devices | grep -v '^List' | grep 'device$' | awk '{print $1}')
    local count
    count=$(echo "$devices" | wc -l | tr -d ' ')
    log "发现 $count 台 Android 设备"

    for serial in $devices; do
        log "========== 设备: $serial =========="
        DEVICE_SERIAL="$serial"

        install_android

        case "$BACKEND" in
            gles)   run_android gles ;;
            vulkan) run_android vulkan ;;
            both)   run_android gles; run_android vulkan ;;
            *)      run_android gles; run_android vulkan ;;
        esac
    done
}

# ============================================================
# iOS 测试（基础版）
# ============================================================
test_ios() {
    if [ "$NO_BUILD" != "true" ]; then
        build_ios
    fi
    log "iOS: 请手动检查设备"
    # TODO: 集成 ios-deploy / devicectl
}

# ============================================================
# 只编译
# ============================================================
build_only() {
    local target="$PROJECT"  # 这里 PROJECT 实际上是平台名
    case "$target" in
        mac)     build_mac ;;
        android) build_android ;;
        ios)     build_ios ;;
        all)     build_mac; build_android ;;
        *)       fail "build 目标: mac, android, ios, all" ;;
    esac
}

# ============================================================
# 主入口
# ============================================================
log "平台=$PLATFORM 项目=$PROJECT 后端=$BACKEND 输出=$OUTPUT_DIR"

case "$PLATFORM" in
    mac)     test_mac ;;
    android) test_android_all_devices ;;
    ios)     test_ios ;;
    build)   build_only ;;
    *)       fail "平台: mac, android, ios, build" ;;
esac

# 汇总
log ""
log "============================================================"
log "测试完成！输出目录: $OUTPUT_DIR"
log "============================================================"
ls -la "$OUTPUT_DIR/" 2>/dev/null
