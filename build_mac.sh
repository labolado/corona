#!/usr/bin/env bash
set -ex
cd "$(dirname "$0")"
current=$(pwd)

. ./replace_code_sign.sh

cd ${current}/platform/mac
xcodebuild -project ./ratatouille.xcodeproj -target rttplayer -configuration Release
