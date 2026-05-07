#!/usr/bin/env bash
set -e

: "${YEAR:=$(date +"%Y")}"
if [[ "$GITHUB_REF" == refs/tags/* ]]
then
    TAG_NAME="${GITHUB_REF#refs/tags/}"
    : "${BUILD:=$TAG_NAME}"
    # BUILD_NUMBER must be numeric only — it's substituted into Rtt_BUILD_REVISION
    # (integer macro), CFBundleVersion, and Windows file version. Tags like
    # "3729.bgfx.v2" must contribute only the leading "3729" to BUILD_NUMBER,
    # while BUILD keeps the full tag string for release names / artifact filenames.
    BUILD_NUMBER_FROM_TAG="$(printf '%s' "$TAG_NAME" | grep -oE '^[0-9]+' || true)"
    : "${BUILD_NUMBER:=${BUILD_NUMBER_FROM_TAG:-$GITHUB_RUN_NUMBER}}"
else
    : "${BUILD:=$GITHUB_RUN_NUMBER}"
    : "${BUILD_NUMBER:=$GITHUB_RUN_NUMBER}"
fi

{
    echo "BUILD_NUMBER=$BUILD_NUMBER"
    echo "BUILD=$BUILD"
    echo "YEAR=$YEAR"
    echo "MONTH=$(date +"%-m")"
    echo "DAY=$(date +"%-d")"
 } >> "$GITHUB_ENV"
