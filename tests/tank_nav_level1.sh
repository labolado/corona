#!/bin/bash
# Auto-navigate tank to level 1 using recorded tap coordinates
# Usage: bash tests/tank_nav_level1.sh [package_name]
PKG="${1:-com.labolado.tank}"
echo "=== Tank: Auto-navigate to Level 1 ==="
adb shell am force-stop "$PKG" && sleep 1
adb shell am start -n "$PKG/com.ansca.corona.CoronaActivity"
echo "Waiting for app load..."
sleep 8

adb shell input tap 751 485  # Play button
sleep 2.0
adb shell input tap 689 357  # Select tank
sleep 1.5
adb shell input tap 725 283  # Confirm/texture
sleep 4.2
adb shell input tap 328 725  # Navigation
sleep 3.3
adb shell input tap 847 713  # Game mode
sleep 2.4
adb shell input tap 197 766  # Level select
sleep 1.5
adb shell input tap 150 716  # Level 1
sleep 1.5
adb shell input tap 223 726  # Confirm
sleep 1.6
adb shell input tap 41 715   # Start
sleep 1.8
adb shell input tap 51 715   # Start (retry)
sleep 2.8
adb shell input tap 1299 39  # Close dialog
sleep 2.0
adb shell input tap 1338 24  # Close
sleep 1.5
adb shell input tap 1301 30  # Close
sleep 1.5
adb shell input tap 1300 28  # Close
sleep 1.5
adb shell input tap 1298 33  # Close
sleep 1.5
adb shell input tap 1292 39  # Close
sleep 1.5
adb shell input tap 1285 42  # Close
sleep 1.5
adb shell input tap 879 263  # Final tap
sleep 3

# Screenshot
adb shell screencap -p /sdcard/_auto_level1.png
adb pull /sdcard/_auto_level1.png /tmp/auto_level1.png 2>/dev/null
echo "=== Done. Screenshot: /tmp/auto_level1.png ==="
