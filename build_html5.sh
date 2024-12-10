#!/bin/bash
set -ex
cd "$(dirname "$0")"
current=$(pwd)

# Setup build environment variables
export YEAR=$(date +"%Y")
export BUILD_NUMBER="local_build"
export BUILD="$BUILD_NUMBER"
export MONTH=$(date +"%-m")
export DAY=$(date +"%-d")

# Update version information
if [ -n "$BUILD_NUMBER" ]; then
    sed -i .bak -E "s/define[[:space:]]*Rtt_BUILD_REVISION.*$/define Rtt_BUILD_REVISION $BUILD_NUMBER/" librtt/Core/Rtt_Version.h
    sed -i .bak -E "s/define[[:space:]]*Rtt_BUILD_YEAR[[:space:]]*[[:digit:]]*$/define Rtt_BUILD_YEAR $YEAR/" librtt/Core/Rtt_Version.h
    sed -i .bak -E "s/^#define[[:space:]]*Rtt_IS_LOCAL_BUILD[[:space:]]*[[:digit:]]*$/\/\/ #define Rtt_IS_LOCAL_BUILD/" librtt/Core/Rtt_Version.h
    rm -f librtt/Core/Rtt_Version.h.bak
fi

# Check if emsdk exists
EMSDK_DIR="$HOME/emsdk"
if [ ! -d "$EMSDK_DIR" ]; then
    echo "Installing emsdk..."
    wget -q --header='Accept:application/octet-stream' https://github.com/coronalabs/emsdk/releases/download/e2.0.34/emsdk.tar.xz -O emsdk.tar.xz
    tar -xjf emsdk.tar.xz -C ~/
    rm emsdk.tar.xz
    xattr -r -d com.apple.quarantine ~/emsdk || true
fi

# Activate emsdk
echo "Activating emscripten..."
source ~/emsdk/emsdk_env.sh

# Build webtemplate
echo "Building webtemplate..."
cd ${current}/platform/emscripten/gmake
./build_template.sh

# Copy result
echo "Copying build result..."
cd "$current"
mkdir -p output
cp -v platform/emscripten/webtemplate.zip output/

echo "Build completed! Output is in: $current/output/webtemplate.zip"
