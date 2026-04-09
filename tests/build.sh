#!/bin/bash
# Solar2D 一键编译脚本
# 用法:
#   bash tests/build.sh              # 编译 Solar2D (Debug)
#   bash tests/build.sh release      # 编译 Solar2D (Release)
#   bash tests/build.sh bgfx         # 重编 bgfx 库 + Solar2D (Debug)
#   bash tests/build.sh bgfx release # 重编 bgfx 库 + Solar2D (Release)
#   bash tests/build.sh all          # 重编 bgfx 库 + Solar2D Debug + Release

set -e

CORONA_DIR="/Users/yee/data/dev/app/labo/corona"
BGFX_DIR="$CORONA_DIR/external/bgfx"
PROJECT="$CORONA_DIR/platform/mac/ratatouille.xcodeproj"

BUILD_BGFX=false
CONFIGS=()

# 解析参数
for arg in "$@"; do
    case "$arg" in
        bgfx)    BUILD_BGFX=true ;;
        release) CONFIGS+=(Release) ;;
        debug)   CONFIGS+=(Debug) ;;
        all)     BUILD_BGFX=true; CONFIGS=(Debug Release) ;;
        *)       echo "Unknown arg: $arg"; exit 1 ;;
    esac
done

# 默认 Debug
if [ ${#CONFIGS[@]} -eq 0 ]; then
    CONFIGS=(Debug)
fi

FAIL=0

# Step 1: 重编 bgfx 库
if [ "$BUILD_BGFX" = true ]; then
    echo "=== 重编 bgfx 库 ==="
    cd "$BGFX_DIR"

    echo "--- bgfx Release ---"
    if make osx-arm64-release 2>&1 | tail -3; then
        cp .build/osx-arm64/bin/libbgfxRelease.a .build/projects/xcode15/libbgfxRelease.a
        echo "✓ bgfx Release: $(ls -lh .build/projects/xcode15/libbgfxRelease.a | awk '{print $5}')"
    else
        echo "✗ bgfx Release 编译失败"
        FAIL=1
    fi

    echo "--- bgfx Debug ---"
    if make osx-arm64-debug 2>&1 | tail -3; then
        cp .build/osx-arm64/bin/libbgfxDebug.a .build/projects/xcode15/libbgfxDebug.a 2>/dev/null || true
        echo "✓ bgfx Debug done"
    else
        echo "(bgfx Debug 编译失败，非致命)"
    fi
fi

# Step 2: 编译 Solar2D
cd "$CORONA_DIR"
for CONFIG in "${CONFIGS[@]}"; do
    echo "=== 编译 Solar2D ($CONFIG) ==="
    if xcodebuild -project "$PROJECT" -target rttplayer -configuration "$CONFIG" build \
        CODE_SIGN_IDENTITY='' CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
        ARCHS=arm64 ONLY_ACTIVE_ARCH=NO 2>&1 | tail -5; then
        echo "✓ Solar2D $CONFIG 编译成功"
    else
        echo "✗ Solar2D $CONFIG 编译失败"
        FAIL=1
    fi
done

# 结果
echo ""
echo "=== 编译结果 ==="
if [ $FAIL -eq 0 ]; then
    echo "✓ 全部成功"
else
    echo "✗ 有失败项，请检查上方输出"
    exit 1
fi
