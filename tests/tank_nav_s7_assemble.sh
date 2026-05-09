#!/usr/bin/env bash
# Navigate S7 Edge (SM-G9350, serial=22f7a3d0) to tank_test_copy 拼装界面 + 截图。
# Usage: bash tank_nav_s7_assemble.sh [output_png_path]
# Default output: /tmp/s7_assemble.png

set -euo pipefail

SERIAL="${SERIAL:-22f7a3d0}"
OUT="${1:-/tmp/s7_assemble.png}"

# Verify connected
if ! adb devices | grep -q "^${SERIAL}[[:space:]]*device"; then
    echo "ERROR: S7 (${SERIAL}) not connected. adb devices output:"
    adb devices
    exit 1
fi

# Screen wakeful (S7 Edge doze 在 30s 让 logcat 失真)
adb -s "$SERIAL" shell input keyevent KEYCODE_WAKEUP
adb -s "$SERIAL" shell settings put system screen_off_timeout 1800000 >/dev/null
adb -s "$SERIAL" shell svc power stayon true

# Cold-restart tank
adb -s "$SERIAL" shell am force-stop com.labolado.labo_brick_tank
adb -s "$SERIAL" logcat -c
adb -s "$SERIAL" shell am start -n com.labolado.labo_brick_tank/com.ansca.corona.CoronaActivity >/dev/null
sleep 12

# 2-step navigation to baseline bug repro (saved-tank assemble screen):
#   1. Main menu → Play (center)
#   2. Tank list → 2nd card (top-middle, saved tank) → assemble with first-component-grey bug
# Verified 2026-04-28: this matches s7_02_courses.png baseline (1st component grey)
adb -s "$SERIAL" shell input tap 960 540   # Play
sleep 4
adb -s "$SERIAL" shell input tap 960 250   # 2nd card (saved tank, top-middle)
sleep 4

# Capture
adb -s "$SERIAL" shell screencap -p /sdcard/test.png
adb -s "$SERIAL" pull /sdcard/test.png "$OUT" >/dev/null
adb -s "$SERIAL" shell rm /sdcard/test.png

echo "S7 assemble screen captured: $OUT"
