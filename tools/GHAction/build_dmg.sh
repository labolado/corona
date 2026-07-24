#!/usr/bin/env bash
set -ex

WORKSPACE=$(cd "$(dirname "$0")/../.." && pwd)
export WORKSPACE
cd "${WORKSPACE}"

if [ -n "$CERT_PASSWORD" ]
then
    security delete-keychain build.keychain || true
    security create-keychain -p 'Password123' build.keychain
    security default-keychain -s build.keychain
    if security import "$WORKSPACE/tools/GHAction/Certificates.p12" -A -P "$CERT_PASSWORD"
    then
        security unlock-keychain -p 'Password123' build.keychain
        security set-keychain-settings build.keychain
        security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k 'Password123' build.keychain > /dev/null

        mkdir -p "$HOME/Library/MobileDevice/Provisioning Profiles"
        for PLATFORM_DIR in iphone tvos
        do
            cp "$WORKSPACE/platform/$PLATFORM_DIR"/*.mobileprovision "$HOME/Library/MobileDevice/Provisioning Profiles/"
        done
    else
        echo "WARNING: Certificate import failed. Building without code signing."
        security default-keychain -s login.keychain
        security delete-keychain build.keychain &> /dev/null || true
        CERT_PASSWORD=""
    fi
fi


BUILD_NUMBER=${BUILD_NUMBER:-3575}
YEAR=${YEAR:-2020}
# DMG filename suffix must match the release job's rename target,
# which uses the cosmetic ${BUILD} (full tag, e.g. "3729.bgfx.v2"),
# not the numeric ${BUILD_NUMBER} (used only for version macros).
BUILD=${BUILD:-$BUILD_NUMBER}

# Workaround: if BUILD was not propagated correctly from GITHUB_ENV (observed on
# macOS-15 runners where multi-segment tags like "3731.b3.v1" sometimes lose
# the suffix), re-read it directly from the env file.
if [ -n "${GITHUB_ENV:-}" ] && [ -f "$GITHUB_ENV" ]; then
    BUILD_FROM_FILE=$(grep '^BUILD=' "$GITHUB_ENV" 2>/dev/null | tail -1 | cut -d= -f2-)
    if [ -n "$BUILD_FROM_FILE" ] && [ "$BUILD_FROM_FILE" != "$BUILD_NUMBER" ]; then
        BUILD="$BUILD_FROM_FILE"
    fi
fi

echo "DMG_BUILD: YEAR=$YEAR BUILD=$BUILD BUILD_NUMBER=$BUILD_NUMBER" >&2

NATIVE_FLAG=""
if [ -f "${WORKSPACE}/Native/CoronaNative.tar.gz" ]; then
    NATIVE_FLAG="-e ${WORKSPACE}/Native/CoronaNative.tar.gz"
fi
if ! bin/mac/build_dmg.sh -d -b "$YEAR.$BUILD" $NATIVE_FLAG "${WORKSPACE}" "${WORKSPACE}/docs"
then
    BUILD_FAILED=YES
    echo "BUILD FAILED"
fi

if [ -n "$CERT_PASSWORD" ]
then
    security default-keychain -s login.keychain
    security delete-keychain build.keychain &> /dev/null || true
fi

if [ "$BUILD_FAILED" = "YES" ]
then
    exit 1
fi
echo $?
