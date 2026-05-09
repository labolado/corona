#!/usr/bin/env bash
# Usage: bash tests/bisect_one.sh <commit_hash> [tag]
# 切到指定 commit + 编 tank APK + 装 P40 + 启动
set -u
COMMIT="${1:?usage: bisect_one.sh <commit> [tag]}"
TAG="${2:-$COMMIT}"
WORKTREE=/tmp/corona-p40-fix
TANK=/Users/yee/data/dev/app/labo/tank_test_copy
PKG=com.labolado.labo-brick-tank
P40=192.168.1.13:5555
OUT=/tmp/p40-bisect-builds
mkdir -p "$OUT"

cd "$WORKTREE" || exit 1
echo "===== checkout $COMMIT ====="
git checkout -f "$COMMIT" 2>&1 | tail -3 || exit 1

echo "===== FORCE_BUILD ====="
FORCE_BUILD=1 bash tests/build_android.sh "$TANK" "$PKG" 2>&1 | tail -5 || exit 1

APK=$(ls -t /tmp/android-build-*/tank_test_copy.apk 2>/dev/null | head -1)
[ -z "$APK" ] && { echo "BUILD FAILED, no APK"; exit 1; }
DEST="$OUT/${TAG}.apk"
cp "$APK" "$DEST"
HASH=$(unzip -p "$DEST" lib/arm64-v8a/libcorona.so 2>/dev/null | shasum | cut -c1-12)
echo "[$TAG] APK=$DEST hash=$HASH"

echo "===== install + launch P40 ====="
adb -s "$P40" install -r "$DEST" 2>&1 | tail -2
adb -s "$P40" shell am force-stop "$PKG"
adb -s "$P40" shell am start -n "$PKG/com.ansca.corona.CoronaActivity"
echo "DONE: $TAG hash=$HASH path=$DEST"
