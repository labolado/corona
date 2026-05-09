#!/bin/bash
# 1 秒发现当前 WiFi 子网内所有 wireless ADB 设备
# 254 端口并行扫，1s 超时。发现后 adb connect + 查 model/serial。
#
# 用法：bash tests/adb_discover.sh
# 副产物：tests/.adb_known_devices.txt（serial 持久库）

set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
KNOWN_DB="$SCRIPT_DIR/.adb_known_devices.txt"
touch "$KNOWN_DB"

# 检测当前活跃接口（按默认路由）的子网前缀
IFACE=$(route -n get default 2>/dev/null | awk '/interface:/{print $2}')
[ -z "$IFACE" ] && IFACE=en0
SUBNET=$(ifconfig "$IFACE" 2>/dev/null | awk '/inet /{print $2}' | awk -F. '{print $1"."$2"."$3}')
[ -z "$SUBNET" ] && { echo "✗ 无法检测子网"; exit 1; }

echo "扫 $SUBNET.0/24:5555 (254 并行, 1s 超时)..."
T0=$(date +%s)
HITS=$(seq 1 254 | xargs -P 254 -I{} sh -c "nc -z -G 1 -w 1 $SUBNET.{} 5555 2>/dev/null && echo $SUBNET.{}:5555" | sort -u)
echo "$HITS" | grep -v '^$' > /tmp/adb_hits_$$.txt
N=$(wc -l < /tmp/adb_hits_$$.txt | tr -d ' ')
echo "扫描 $(($(date +%s) - T0))s, 找到 $N 个开放 5555:"

[ "$N" = "0" ] && { rm -f /tmp/adb_hits_$$.txt; echo "（无）— 设备需 USB 接一次跑 tests/adb_enable_wireless.sh 启用 tcpip 5555"; exit 0; }

echo ""
echo "=== adb connect + 识别 ==="
while IFS= read -r ep; do
  [ -z "$ep" ] && continue
  R=$(adb connect "$ep" 2>&1)
  if ! echo "$R" | grep -qE "connected to|already connected"; then
    echo "  ? $ep — $R"; continue
  fi
  # 重试拿 serial（adb connect 后 device 可能尚未 ready）
  for i in 1 2 3; do
    M=$(adb -s "$ep" shell getprop ro.product.model 2>/dev/null | tr -d '\r')
    S=$(adb -s "$ep" shell getprop ro.serialno 2>/dev/null | tr -d '\r')
    [ -n "$S" ] && break
    sleep 1
  done
  if [ -z "$S" ]; then
    echo "  ? $ep connected 但 shell 不响应（offline?）"
    adb disconnect "$ep" >/dev/null 2>&1
    continue
  fi
  TAG="新增"
  if grep -q "^$S|" "$KNOWN_DB" 2>/dev/null; then
    TAG="已知"
  else
    echo "$S|$M" >> "$KNOWN_DB"
  fi
  echo "  ✓ $ep → $M [$S] ($TAG)"
done < /tmp/adb_hits_$$.txt
rm -f /tmp/adb_hits_$$.txt

echo ""
echo "=== adb devices ==="
adb devices | grep -v "^List" | grep -v "^$"
echo ""
echo "总耗时: $(($(date +%s) - T0))s"
