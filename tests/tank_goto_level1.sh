#!/bin/bash
# Navigate tank game to level 1 using adb taps
# Usage: bash tests/tank_goto_level1.sh [package_name]
PKG="${1:-com.labolado.tank}"

echo "=== Tank: Navigate to Level 1 ==="

# Force stop and restart
adb shell am force-stop "$PKG"
sleep 1
adb shell am start -n "$PKG/com.ansca.corona.CoronaActivity"
echo "Waiting for app to load..."
sleep 8

# Take screenshot to verify main menu
adb shell screencap -p /sdcard/_nav_step0.png
echo "Step 0: Main menu"

# Tap Play button (center of screen: 670, 400 on 1340x800)
adb shell input tap 670 400
echo "Step 1: Tapped Play"
sleep 3

# Take screenshot
adb shell screencap -p /sdcard/_nav_step1.png

# Tap first tank (top-left card area: ~300, 250)
adb shell input tap 300 250
echo "Step 2: Tapped first tank"
sleep 2

# Take screenshot
adb shell screencap -p /sdcard/_nav_step2.png

# Check if we're in the editor - if so, look for play/game button
# Tap the game mode icon on left sidebar (~35, 350)
adb shell input tap 35 350
echo "Step 3: Tapped game mode"
sleep 2

adb shell screencap -p /sdcard/_nav_step3.png

# Tap first level (~300, 300)
adb shell input tap 300 300
echo "Step 4: Tapped level 1"
sleep 3

# Final screenshot
adb shell screencap -p /sdcard/_nav_final.png
adb pull /sdcard/_nav_final.png /tmp/tank_level1.png 2>/dev/null

echo "=== Navigation complete ==="
echo "Screenshots: /sdcard/_nav_step{0,1,2,3}_final.png"
echo "Final: /tmp/tank_level1.png"
