#!/bin/bash
# Android bgfx 构建脚本：编译 Corona.aar → 安装到 Corona-b3 → CoronaBuilder 打包 → 安装到设备
# 用法: bash tests/build_android.sh <project_path> [package_name]
# 示例: bash tests/build_android.sh tests/bgfx-demo com.labolado.bgfxdemo
#       bash tests/build_android.sh /Users/yee/data/dev/app/labo/tank_test com.labolado.tank

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORONA_DIR="${CORONA_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
CORONA_B3="${CORONA_B3:-/Applications/Corona-b3}"
CORONABUILDER="$CORONA_B3/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder"
AAR_INSTALL_DIR="$CORONA_B3/Native/Corona/android/lib/gradle"

PROJECT_PATH="${1:?用法: bash tests/build_android.sh <project_path> [package_name]}"
PACKAGE_NAME="${2:-com.labolado.bgfxdemo}"
APP_NAME="$(basename "$PROJECT_PATH")"
DST="/tmp/android-build-$(date +%H%M%S)"

log() { echo "[$(date '+%H:%M:%S')] $1"; }
fail() { echo "FAILED: $1"; exit 1; }

# ============================================================
# Step 0: 验证环境
# ============================================================
log "Step 0: 验证环境"

cd "$CORONA_DIR" || fail "Cannot cd to $CORONA_DIR"
BRANCH=$(git branch --show-current)
log "  分支: $BRANCH"

[ -f "$PROJECT_PATH/main.lua" ] || fail "找不到 $PROJECT_PATH/main.lua"
[ -f "$CORONABUILDER" ] || fail "CoronaBuilder 不存在: $CORONABUILDER"

# 检查 Android SDK
ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
[ -d "$ANDROID_HOME" ] || fail "Android SDK 不存在: $ANDROID_HOME"

# 检查 adb
which adb > /dev/null 2>&1 || fail "adb 不在 PATH 中"

# 检查设备连接
ADB_DEVICE=$(adb devices 2>/dev/null | grep -v "List" | grep "device$" | head -1 | awk '{print $1}')
if [ -z "$ADB_DEVICE" ]; then
    log "  WARNING: 没有 Android 设备连接，跳过安装步骤"
fi

log "  项目: $PROJECT_PATH"
log "  包名: $PACKAGE_NAME"
log "  设备: ${ADB_DEVICE:-未连接}"

# ============================================================
# Step 1: 编译 Corona AAR (包含 bgfx)
# ============================================================
AAR_OUTPUT="$CORONA_DIR/platform/android/sdk/build/outputs/aar/Corona-release.aar"

# 检查是否需要编译
NEWEST_SRC=$(find librtt/ platform/android/ndk/ -name "*.cpp" -o -name "*.h" 2>/dev/null | xargs stat -f "%m" 2>/dev/null | sort -rn | head -1)
AAR_TIME=$(stat -f "%m" "$AAR_OUTPUT" 2>/dev/null || echo "0")

if [ -f "$AAR_OUTPUT" ] && [ "$AAR_TIME" -gt "$NEWEST_SRC" ] && [ "${FORCE_BUILD:-}" != "1" ]; then
    log "Step 1: Corona.aar 已是最新，跳过编译 (FORCE_BUILD=1 强制重编)"
else
    log "Step 1: 编译 Corona AAR (arm64-v8a, Release)"

    # 验证 bgfx 静态库存在
    BGFX_LIB="$CORONA_DIR/external/bgfx/.build/android-arm64/bin/libbgfxRelease.a"
    [ -f "$BGFX_LIB" ] || fail "bgfx 静态库不存在: $BGFX_LIB (需要先编译 bgfx for Android)"

    # 记录旧 BuildId（用于后续验证 C++ 是否真正重编）
    NDK_READELF_BIN="$ANDROID_HOME/ndk/27.0.12077973/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf"
    OLD_BUILD_ID=""
    if [ -f "$AAR_OUTPUT" ] && [ -f "$NDK_READELF_BIN" ]; then
        TMPSO="/tmp/old-libcorona-check-$$"
        mkdir -p "$TMPSO"
        unzip -q -o "$AAR_OUTPUT" "jni/arm64-v8a/libcorona.so" -d "$TMPSO" 2>/dev/null
        OLD_BUILD_ID=$("$NDK_READELF_BIN" -n "$TMPSO/jni/arm64-v8a/libcorona.so" 2>/dev/null | grep 'Build ID' | awk '{print $NF}')
        rm -rf "$TMPSO"
        [ -n "$OLD_BUILD_ID" ] && log "  旧 BuildId: $OLD_BUILD_ID"
    fi

    cd "$CORONA_DIR/platform/android"
    # 警告：bgfx 源码（external/bgfx/src/）在 Android 上是预编译静态库
    # 改了 renderer_vk.cpp 等必须先 make android-arm64-release，否则改动不会生效
    BGFX_A_TIME=$(stat -f "%m" "$BGFX_LIB" 2>/dev/null || echo "0")
    BGFX_VK_SRC="$CORONA_DIR/external/bgfx/src/renderer_vk.cpp"
    BGFX_VK_TIME=$(stat -f "%m" "$BGFX_VK_SRC" 2>/dev/null || echo "0")
    if [ "$BGFX_VK_TIME" -gt "$BGFX_A_TIME" ]; then
        log "  ⚠️  WARNING: renderer_vk.cpp 比 libbgfxRelease.a 新！"
        log "     改动未进入 .a，需先运行: cd external/bgfx && make android-arm64-release"
        [ "${FORCE_BUILD:-}" = "1" ] && fail "FORCE_BUILD=1 但 bgfx .a 未重编，请先 make android-arm64-release"
    fi

    # FORCE_BUILD=1 时必须 clean，否则 CMake 可能跳过 C++ 重编
    if [ "${FORCE_BUILD:-}" = "1" ]; then
        log "  FORCE_BUILD: 执行 clean + assembleRelease"
        ./gradlew :Corona:clean :Corona:assembleRelease --no-daemon 2>&1 | tail -5
    else
        ./gradlew :Corona:assembleRelease --no-daemon 2>&1 | tail -5
    fi

    cd "$CORONA_DIR"
    [ -f "$AAR_OUTPUT" ] || fail "AAR 编译失败"
fi

AAR_SIZE=$(ls -lh "$AAR_OUTPUT" | awk '{print $5}')
log "  AAR: $AAR_OUTPUT ($AAR_SIZE)"

# ============================================================
# Step 2: 验证编译产物包含 bgfx
# ============================================================
log "Step 2: 验证 bgfx 符号"

TEMP_CHECK="/tmp/aar-bgfx-verify-$$"
mkdir -p "$TEMP_CHECK"
cd "$TEMP_CHECK"
unzip -q -o "$AAR_OUTPUT" "jni/arm64-v8a/libcorona.so" 2>/dev/null

NDK_DIR="$ANDROID_HOME/ndk/27.0.12077973"
if [ -f "$NDK_DIR/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm" ]; then
    BGFX_COUNT=$("$NDK_DIR/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm" -D jni/arm64-v8a/libcorona.so 2>/dev/null | grep -c "bgfx" || true)
else
    BGFX_COUNT=$(nm -D jni/arm64-v8a/libcorona.so 2>/dev/null | grep -c "bgfx" || true)
fi

rm -rf "$TEMP_CHECK"
cd "$CORONA_DIR"

log "  bgfx 符号数: $BGFX_COUNT"
[ "$BGFX_COUNT" -gt 0 ] || fail "Corona.aar 中没有 bgfx 符号"

# BuildId 验证：确认 C++ 真正重编了
if [ -n "${OLD_BUILD_ID:-}" ] && [ -f "$NDK_READELF_BIN" ]; then
    NEW_BUILD_ID=$("$NDK_READELF_BIN" -n "$TEMP_CHECK/jni/arm64-v8a/libcorona.so" 2>/dev/null | grep 'Build ID' | awk '{print $NF}')
    log "  新 BuildId: ${NEW_BUILD_ID:-unknown}"
    if [ -n "$NEW_BUILD_ID" ] && [ "$NEW_BUILD_ID" = "$OLD_BUILD_ID" ]; then
        fail "C++ 未重编！BuildId 未变 ($OLD_BUILD_ID)。请检查 CMake 缓存或手动 clean。"
    fi
    [ -n "$NEW_BUILD_ID" ] && log "  ✅ BuildId 已变化，C++ 确认重编"
fi

# ============================================================
# Step 3: 安装 AAR + 修补模板到 Corona-b3
# ============================================================
log "Step 3: 安装 AAR 到 Corona-b3"

# 备份 AAR
ORIG_AAR="$AAR_INSTALL_DIR/Corona.aar"
if [ -f "$ORIG_AAR" ]; then
    BACKUP="$AAR_INSTALL_DIR/Corona.aar.bak-$(date +%Y%m%d-%H%M%S)"
    cp "$ORIG_AAR" "$BACKUP"
    log "  备份 AAR: $(basename "$BACKUP")"
fi
cp "$AAR_OUTPUT" "$ORIG_AAR"
log "  已安装 AAR"

# 修补 android-template.zip：minSdk 15→24（匹配 AAR 的 minSdk）
TMPL_ZIP="$CORONA_B3/Native/Corona/android/resource/android-template.zip"
if [ -f "$TMPL_ZIP" ]; then
    TMPL_NEEDS_PATCH=$(unzip -p "$TMPL_ZIP" "template/app/build.gradle.kts" 2>/dev/null | grep -c "?: 15" || true)
    if [ "$TMPL_NEEDS_PATCH" -gt 0 ]; then
        log "  修补模板 minSdk 15→24"
        PATCH_DIR="/tmp/tmpl-patch-$$"
        mkdir -p "$PATCH_DIR" && cd "$PATCH_DIR"
        unzip -q "$TMPL_ZIP"
        sed -i '' 's/?: 15/?: 24/' template/app/build.gradle.kts
        cp "$TMPL_ZIP" "${TMPL_ZIP}.bak-$(date +%Y%m%d-%H%M%S)"
        rm -f /tmp/android-template-patched.zip
        zip -r -q /tmp/android-template-patched.zip template/ sdk/
        cp /tmp/android-template-patched.zip "$TMPL_ZIP"
        rm -rf "$PATCH_DIR"
        cd "$CORONA_DIR"
        # 清除 Gradle 缓存以避免旧 manifest
        rm -rf ~/.gradle/caches/*/transforms/*/transformed/jetified-Corona 2>/dev/null
        log "  模板已修补"
    fi
fi

# ============================================================
# Step 3.5: _tags 目录转换 (Android aapt2 排除 _ 开头目录)
# ============================================================
TAGS_DIRS=$(find "$PROJECT_PATH" -type d -name "_tags" 2>/dev/null)
USE_TEMP_PROJECT=""
if [ -n "$TAGS_DIRS" ]; then
    log "Step 3.5: 转换 _tags 目录 (aapt2 兼容)"
    TEMP_PROJECT="/tmp/android-project-$$"
    rm -rf "$TEMP_PROJECT"
    cp -a "$PROJECT_PATH" "$TEMP_PROJECT"
    # 重命名 _tags → tags
    find "$TEMP_PROJECT" -type d -name "_tags" | while read -r d; do
        mv "$d" "$(dirname "$d")/tags"
        log "  重命名: ${d#$TEMP_PROJECT/} → tags"
    done
    # 替换所有文本文件中的路径引用 (_tags/ → tags/)，不影响变量名
    find "$TEMP_PROJECT" \( -name "*.lua" -o -name "*.json" -o -name "*.txt" -o -name "*.xml" -o -name "*.csv" \) -exec grep -l '_tags/' {} \; | while read -r f; do
        sed -i '' 's|_tags/|tags/|g' "$f"
    done
    UPDATED_COUNT=$(find "$TEMP_PROJECT" \( -name "*.lua" -o -name "*.json" \) -exec grep -l 'tags/' {} \; 2>/dev/null | wc -l | tr -d ' ')
    log "  已更新 $UPDATED_COUNT 个文件的 _tags/ → tags/ 路径"
    USE_TEMP_PROJECT="$TEMP_PROJECT"
fi

# ============================================================
# Step 4: CoronaBuilder 打包 APK
# ============================================================
log "Step 4: CoronaBuilder 打包"

mkdir -p "$DST"

# 使用绝对路径（如有临时副本则用临时副本）
BUILD_PROJECT="${USE_TEMP_PROJECT:-$PROJECT_PATH}"
ABS_PROJECT=$(cd "$BUILD_PROJECT" 2>/dev/null && pwd || echo "$BUILD_PROJECT")

cat > /tmp/build-android-bgfx.lua << LUAEOF
local params = {
    platform = 'android',
    appName = '$APP_NAME',
    appVersion = '1.0',
    dstPath = '$DST',
    projectPath = '$ABS_PROJECT',
    androidAppPackage = '$PACKAGE_NAME',
    keystorePath = '$HOME/.android/debug.keystore',
    keystorePassword = 'android',
    keystoreAlias = 'androiddebugkey',
    keystoreAliasPassword = 'android',
}
return params
LUAEOF

cd "$CORONA_DIR"
BUILD_OUTPUT=$("$CORONABUILDER" build --lua /tmp/build-android-bgfx.lua 2>&1)
echo "$BUILD_OUTPUT" | grep -E "succeeded|failed|error|BUILD" | head -5

APK_PATH="$DST/$APP_NAME.apk"
AAB_PATH="$DST/$APP_NAME.aab"

if [ -f "$APK_PATH" ]; then
    APK_SIZE=$(ls -lh "$APK_PATH" | awk '{print $5}')
    log "  APK: $APK_PATH ($APK_SIZE)"
elif [ -f "$AAB_PATH" ]; then
    AAB_SIZE=$(ls -lh "$AAB_PATH" | awk '{print $5}')
    log "  AAB: $AAB_PATH ($AAB_SIZE)"
else
    # 查找实际输出
    FOUND=$(find "$DST" -name "*.apk" -o -name "*.aab" 2>/dev/null | head -3)
    if [ -n "$FOUND" ]; then
        log "  输出: $FOUND"
        APK_PATH=$(echo "$FOUND" | head -1)
    else
        echo "$BUILD_OUTPUT" | tail -20
        fail "CoronaBuilder 打包失败，$DST 中没有 APK/AAB"
    fi
fi

# ============================================================
# Step 5: 安装到设备
# ============================================================
if [ -n "$ADB_DEVICE" ] && [ -f "$APK_PATH" ]; then
    log "Step 5: 安装到设备 $ADB_DEVICE"
    adb -s "$ADB_DEVICE" install -r "$APK_PATH" 2>&1 | grep -E "Success|Failure"
    log "  启动: adb shell am start -n $PACKAGE_NAME/com.ansca.corona.CoronaActivity"
elif [ -n "$ADB_DEVICE" ] && [ -f "$AAB_PATH" ]; then
    log "Step 5: AAB 需要通过 bundletool 安装（跳过直接安装）"
    log "  手动: bundletool build-apks --bundle=$AAB_PATH --output=/tmp/app.apks --local-testing"
else
    log "Step 5: 跳过安装（无设备或无 APK）"
fi

# ============================================================
# Step 6: 验证 logcat (如果设备已连接且已安装)
# ============================================================
if [ -n "$ADB_DEVICE" ] && [ -f "$APK_PATH" ]; then
    log "Step 6: 启动应用并检查 bgfx 日志"
    adb -s "$ADB_DEVICE" logcat -c 2>/dev/null
    adb -s "$ADB_DEVICE" shell am start -n "$PACKAGE_NAME/com.ansca.corona.CoronaActivity" 2>/dev/null
    sleep 3
    BGFX_LOG=$(adb -s "$ADB_DEVICE" logcat -d -s Corona:V 2>/dev/null | grep -i "bgfx\|BGFX" | head -5)
    if [ -n "$BGFX_LOG" ]; then
        log "  bgfx 日志:"
        echo "$BGFX_LOG" | while read -r line; do echo "    $line"; done
    else
        log "  WARNING: 未检测到 bgfx 日志（可能还在启动中）"
        log "  手动检查: adb logcat -s Corona:V | grep -i bgfx"
    fi
fi

# ============================================================
# 总结
# ============================================================
echo ""
echo "============================================"
echo "  Android bgfx 构建完成"
echo "  分支: $BRANCH"
echo "  包名: $PACKAGE_NAME"
echo "  AAR: $AAR_OUTPUT ($AAR_SIZE)"
if [ -f "$APK_PATH" ]; then
    echo "  APK: $APK_PATH"
fi
if [ -f "$AAB_PATH" ]; then
    echo "  AAB: $AAB_PATH"
fi
echo "  bgfx 符号: $BGFX_COUNT"
if [ -n "$ADB_DEVICE" ]; then
    echo "  设备: $ADB_DEVICE"
    echo "  启动: adb shell am start -n $PACKAGE_NAME/com.ansca.corona.CoronaActivity"
    echo "  日志: adb logcat -s Corona:V"
fi
if [ -n "$USE_TEMP_PROJECT" ]; then
    echo "  _tags 转换: 已应用"
fi
echo "============================================"

# 清理临时项目副本
if [ -n "$USE_TEMP_PROJECT" ]; then
    rm -rf "$USE_TEMP_PROJECT"
fi
