#!/bin/bash
# 全自动：扫描 USB Android 设备 → 切 tcpip 5555 → 检测 IP → wireless connect
# 用途：USB 接入新设备 / 设备重启后用于一键开启无线调试
# 副产物：tests/.adb_wireless_devices.txt 保存 IP 列表，供 adb_connect_all.sh 使用

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEVICE_LIST="$SCRIPT_DIR/.adb_wireless_devices.txt"

USB_DEVICES=$(adb devices -l 2>/dev/null | awk '/transport_id/ && !/emulator/ && !/:5555/ {print $1}')
if [ -z "$USB_DEVICES" ]; then
  echo "未检测到 USB Android 设备。先用 USB 接一台再跑此脚本。"
  exit 1
fi

> "$DEVICE_LIST"
echo "=== 扫描 USB 设备 ==="
for s in $USB_DEVICES; do
  IP=$(adb -s "$s" shell ip route 2>/dev/null | awk '/wlan/ {print $9}' | head -1)
  [ -z "$IP" ] && IP=$(adb -s "$s" shell ip addr show wlan0 2>/dev/null | awk '/inet / {print $2}' | cut -d'/' -f1 | head -1)
  MODEL=$(adb -s "$s" shell getprop ro.product.model 2>/dev/null | tr -d '\r')

  if [ -z "$IP" ]; then
    echo "  ✗ $s ($MODEL): 无 wlan IP（设备没连 WiFi？）"
    continue
  fi

  adb -s "$s" tcpip 5555 > /dev/null 2>&1
  sleep 1
  RESULT=$(adb connect "$IP:5555" 2>&1)
  case "$RESULT" in
    *"connected to"*|*"already connected"*)
      echo "  ✓ $MODEL @ $IP:5555"
      echo "$IP:5555|$MODEL" >> "$DEVICE_LIST"
      ;;
    *)
      echo "  ✗ $MODEL @ $IP:5555 — $RESULT"
      ;;
  esac
done

echo ""
echo "=== 已保存设备列表 → $DEVICE_LIST ==="
cat "$DEVICE_LIST" 2>/dev/null
echo ""
echo "现在可以拔掉 USB 线，wireless 持续可用（直到设备重启）。"
echo "重新连接：bash tests/adb_connect_all.sh"
