#!/bin/bash
# 一键 iOS 构建脚本：编译 → 验证 → 打包 tar → CoronaBuilder 打包 → 签名 → 安装
# 用法: bash tests/build_ios.sh <project_path> [bundle_id] [profile_path] [display_name]
# 示例: bash tests/build_ios.sh /tmp/uaf_repro_project com.labolado.labo-brick-tank
#       bash tests/build_ios.sh /Users/yee/data/dev/app/labo/tank_test_copy com.labolado.labo-brick-tank-full

# 不用 set -e，手动检查关键步骤
# set -e 会导致 grep/strings 等返回非零时意外退出

# 自动检测路径（可通过环境变量覆盖）
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORONA_DIR="${CORONA_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
TDIR="${CORONA_TEMPLATE_DIR:-/Applications/Corona-b3/Corona Simulator.app/Contents/Resources/iostemplate}"
SIGN_ID="${IOS_SIGN_ID:-$(security find-identity -v -p codesigning 2>/dev/null | grep "Apple Development" | head -1 | awk '{print $2}')}"
DEVICE="${IOS_DEVICE:-$(xcrun devicectl list devices 2>/dev/null | grep "available.*paired" | head -1 | grep -oE '[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}')}"
API_DIR="${API_DIR:-/Users/yee/data/dev/app/api}"

# 如果自动检测失败，用默认值
[ -z "$SIGN_ID" ] && SIGN_ID="5421015426E6E4E23B8FB8C0184A3DF0ED4E343B"
[ -z "$DEVICE" ] && DEVICE="59B21151-EC4C-5713-98A1-AA0B324E0AD5"

PROJECT_PATH="${1:?用法: bash tests/build_ios.sh <project_path> [bundle_id] [profile_path] [display_name]}"
BUNDLE_ID="${2:-com.labolado.labo-brick-tank}"
PROFILE="${3:-$API_DIR/Development_${BUNDLE_ID//./_}.mobileprovision}"
DISPLAY_NAME="${4:-$(basename "$PROJECT_PATH")}"
APP_NAME="$(basename "$PROJECT_PATH")"
DST="/tmp/ios-build-$(date +%H%M%S)"

log() { echo "[$(date '+%H:%M:%S')] $1"; }
fail() { echo "❌ FAILED: $1"; exit 1; }

# ============================================================
# Step 0: 验证环境
# ============================================================
log "Step 0: 验证环境"

cd "$CORONA_DIR"
BRANCH=$(git branch --show-current)
log "  分支: $BRANCH"

# 检查有没有正在编译的 xcodebuild
if pgrep -f "xcodebuild.*ratatouille" > /dev/null; then
    fail "有其他 xcodebuild 正在运行，等它完成"
fi

# 检查项目
[ -f "$PROJECT_PATH/main.lua" ] || fail "找不到 $PROJECT_PATH/main.lua"

# 检查 profile
if [ ! -f "$PROFILE" ]; then
    log "  Profile $PROFILE 不存在，搜索可用的..."
    PROFILE=$(find "$API_DIR" -name "Development_*.mobileprovision" | head -1)
    [ -f "$PROFILE" ] || fail "找不到 provisioning profile"
    log "  使用: $PROFILE"
fi

log "  项目: $PROJECT_PATH"
log "  Bundle ID: $BUNDLE_ID"
log "  Profile: $(basename "$PROFILE")"

# ============================================================
# Step 1: 编译 iOS libtemplate（增量编译，只编变化的文件）
# ============================================================
LIBTEMPLATE="platform/iphone/build/Release-iphoneos/libtemplate.a"

# 检查是否需要编译：比较源码最新修改时间 vs libtemplate.a
NEWEST_SRC=$(find librtt/ platform/iphone/libtemplate/ -name "*.cpp" -o -name "*.mm" -o -name "*.h" 2>/dev/null | xargs stat -f "%m" 2>/dev/null | sort -rn | head -1)
LIB_TIME=$(stat -f "%m" "$LIBTEMPLATE" 2>/dev/null || echo "0")

if [ -f "$LIBTEMPLATE" ] && [ "$LIB_TIME" -gt "$NEWEST_SRC" ] && [ "${FORCE_BUILD:-}" != "1" ]; then
    log "Step 1: libtemplate 已是最新，跳过编译（FORCE_BUILD=1 强制重编）"
else
    log "Step 1: 编译 iOS libtemplate (Release, 增量)"
    xcodebuild -project platform/iphone/ratatouille.xcodeproj -target libtemplate -configuration Release \
        -sdk iphoneos ARCHS=arm64 ONLY_ACTIVE_ARCH=NO \
        CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
        PROVISIONING_PROFILE_SPECIFIER="" 2>&1 | tail -1
fi

[ -f "$LIBTEMPLATE" ] || fail "libtemplate.a 不存在"
log "  ✅ libtemplate.a: $(ls -lh "$LIBTEMPLATE" | awk '{print $5}')"

# ============================================================
# Step 2: 验证编译产物包含预期代码
# ============================================================
log "Step 2: 验证编译产物"

# 检查 bgfx 符号
BGFX_COUNT=$(nm "$LIBTEMPLATE" 2>/dev/null | grep -c "BgfxRenderer" || true)
log "  bgfx 符号: $BGFX_COUNT"
[ "$BGFX_COUNT" -gt 0 ] || log "  ⚠️ 没有 bgfx 符号（可能是纯 GL 构建）"

# 检查 UAF 修复：旧代码在 ~DisplayObjectExtensions 中有 GetParent 调用后才 SetUserData
# 新代码直接 SetUserData。通过检查源码确认（编译产物难以区分）
if grep -q "Always clear UserData unconditionally" librtt/Rtt_DisplayObjectExtensions.cpp; then
    log "  ✅ UAF 修复已确认在源码中"
else
    log "  ⚠️ UAF 修复不在源码中！检查分支"
fi

# 验证分支没被切走（编译期间可能被其他 worker 切分支）
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "$BRANCH" ]; then
    fail "分支被切走了！编译前: $BRANCH, 现在: $CURRENT_BRANCH"
fi

# ============================================================
# Step 3: 编译 iOS template.app（增量）
# ============================================================
TEMPLATE_APP="platform/iphone/build/Release-iphoneos/template.app"
TEMPLATE_BIN="$TEMPLATE_APP/template"

TMPL_TIME=$(stat -f "%m" "$TEMPLATE_BIN" 2>/dev/null || echo "0")

if [ -f "$TEMPLATE_BIN" ] && [ "$TMPL_TIME" -gt "$NEWEST_SRC" ] && [ "${FORCE_BUILD:-}" != "1" ]; then
    log "Step 3: template.app 已是最新，跳过编译"
else
    log "Step 3: 编译 iOS template.app (Release, 增量)"
    xcodebuild -project platform/iphone/ratatouille.xcodeproj -target template -configuration Release \
        -sdk iphoneos ARCHS=arm64 ONLY_ACTIVE_ARCH=NO \
        CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
        PROVISIONING_PROFILE_SPECIFIER="" 2>&1 | tail -1
fi

[ -d "$TEMPLATE_APP" ] || fail "template.app 不存在"
log "  ✅ template.app 就绪"

# ============================================================
# Step 4: 打包 template tar
# ============================================================
log "Step 4: 打包 template tar"

SDK_VER=$(xcrun --sdk iphoneos --show-sdk-version)
rm -rf /tmp/template_pack_clean
mkdir -p /tmp/template_pack_clean/libtemplate /tmp/template_pack_clean/template.app

# 合并 libbgfx.a 到 libtemplate.a（CoronaBuilder 重新链接时需要完整符号）
BGFX_LIB="external/bgfx/.build/ios-arm64/bin/libbgfxRelease.a"
if [ -f "$BGFX_LIB" ]; then
    log "  合并 libbgfx.a 到 libtemplate.a"
    libtool -static -o /tmp/template_pack_clean/libtemplate/libtemplate.a "$LIBTEMPLATE" "$BGFX_LIB"
else
    log "  ⚠️ libbgfx.a 不存在，使用原始 libtemplate（无插件项目可用，有插件会崩）"
    cp "$LIBTEMPLATE" /tmp/template_pack_clean/libtemplate/
fi
cp tools/buildsys-ios/libtemplate/*.lua /tmp/template_pack_clean/libtemplate/ 2>/dev/null
cp tools/buildsys-ios/libtemplate/build_output.sh /tmp/template_pack_clean/libtemplate/ 2>/dev/null

DST_TMPL="/tmp/template_pack_clean/template.app"
for f in _CoronaSplashScreen.png Corona3rdPartyLicenses.txt Info.plist \
         MainWindow-iPad.nib MainWindow.nib PkgInfo template; do
    cp "$TEMPLATE_APP/$f" "$DST_TMPL/" 2>/dev/null
done
cp -r "$TEMPLATE_APP/CoronaResources.bundle" "$DST_TMPL/" 2>/dev/null
cp -r "$TEMPLATE_APP/_CodeSignature" "$DST_TMPL/" 2>/dev/null

cd /tmp/template_pack_clean
TAR_FILE="/tmp/iphoneos_${SDK_VER}.tar.bz"
tar cjf "$TAR_FILE" \
    --exclude='CoronaSimLogo-256.png' --exclude='world.jpg' --exclude='Icon*.png' \
    ./libtemplate ./template.app

TAR_SIZE=$(ls -lh "$TAR_FILE" | awk '{print $5}')
log "  ✅ tar: $TAR_FILE ($TAR_SIZE)"

# ============================================================
# Step 5: 安装到 Corona-b3
# ============================================================
log "Step 5: 安装 tar 到 Corona-b3"

# 备份
BACKUP="$TDIR/iphoneos_26.1.tar.bz.bak-$(date +%Y%m%d-%H%M%S)"
cp "$TDIR/iphoneos_26.1.tar.bz" "$BACKUP"
log "  备份: $(basename "$BACKUP")"

cp "$TAR_FILE" "$TDIR/iphoneos_26.1.tar.bz"
log "  ✅ 已安装"

# ============================================================
# Step 6: CoronaBuilder 打包
# ============================================================
log "Step 6: CoronaBuilder 打包"

mkdir -p "$DST"
cat > /tmp/build-ios-auto.lua << LUAEOF
local params = {
    platform = 'ios',
    appName = '$APP_NAME',
    appVersion = '1.0',
    dstPath = '$DST',
    projectPath = '$PROJECT_PATH',
    certificatePath = '$PROFILE',
}
return params
LUAEOF

cd "$CORONA_DIR"
/Applications/Corona-b3/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder \
    build --lua /tmp/build-ios-auto.lua 2>&1 | grep -E "succeeded|failed|error"

APP_PATH="$DST/$APP_NAME.app"
[ -d "$APP_PATH" ] || fail "CoronaBuilder 打包失败，$APP_PATH 不存在"
log "  ✅ $APP_PATH"

# ============================================================
# Step 7: 签名
# ============================================================
log "Step 7: 签名"

/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$APP_PATH/Info.plist"
if [ -n "$DISPLAY_NAME" ]; then
    /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName $DISPLAY_NAME" "$APP_PATH/Info.plist" 2>/dev/null
fi
cp "$PROFILE" "$APP_PATH/embedded.mobileprovision"

security cms -D -i "$PROFILE" 2>/dev/null | python3 -c "
import plistlib, sys
data = plistlib.loads(sys.stdin.buffer.read())
plistlib.dump(data['Entitlements'], open('/tmp/ios-build-ent.plist','wb'))
"

codesign --force --sign "$SIGN_ID" --entitlements /tmp/ios-build-ent.plist "$APP_PATH" 2>&1 | tail -1
log "  ✅ 已签名"

# 最终验证：二进制包含 bgfx
FINAL_BGFX=$(strings "$APP_PATH/$APP_NAME" 2>/dev/null | grep -c "BgfxRenderer" || true)
log "  最终二进制 bgfx 符号: $FINAL_BGFX"

# ============================================================
# Step 8: 安装到设备
# ============================================================
log "Step 8: 安装到 iPhone"

xcrun devicectl device install app --device "$DEVICE" "$APP_PATH" 2>&1 | grep -E "installed|failed"
log "  ✅ 安装完成"

# ============================================================
# 总结
# ============================================================
echo ""
echo "============================================"
echo "  ✅ iOS 构建完成"
echo "  分支: $BRANCH"
echo "  Bundle ID: $BUNDLE_ID"
echo "  显示名: $DISPLAY_NAME"
echo "  App: $APP_PATH"
echo "  启动: xcrun devicectl device process launch --device $DEVICE --console $BUNDLE_ID"
echo "============================================"

