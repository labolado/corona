#!/bin/bash
# 一键无线连接所有 Android 真机
# 重启过的设备需要先 USB 接一次跑 enable_wireless（tcpip 模式重启失效）

DEVICES=(
  "192.168.1.13:5555|ANA-AN00 (P40)"
  "192.168.1.15:5555|SM-T220 (Tab A7 Lite)"
  "192.168.1.20:5555|SM-G9350 (S7 Edge)"
  "192.168.1.26:5555|CPH2477 (OnePlus)"
)

echo "=== Wireless ADB Connect ==="
for entry in "${DEVICES[@]}"; do
  IP="${entry%%|*}"
  NAME="${entry##*|}"
  RESULT=$(adb connect "$IP" 2>&1)
  case "$RESULT" in
    *"already connected"*|*"connected to"*)
      echo "  ✓ $NAME @ $IP"
      ;;
    *"Connection refused"*|*"failed"*|*"timed out"*)
      echo "  ✗ $NAME @ $IP — $RESULT"
      echo "    （需要 USB 接一次跑 tests/adb_enable_wireless.sh）"
      ;;
    *)
      echo "  ? $NAME @ $IP — $RESULT"
      ;;
  esac
done

echo ""
echo "=== 当前 adb devices ==="
adb devices | grep -v "^List of"
