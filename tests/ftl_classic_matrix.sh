#!/usr/bin/env bash
# FTL classic device matrix dispatcher (parallel)
# Usage: ftl_classic_matrix.sh <APK_PATH> [OUT_DIR]
#
# Dispatches the 10-device classic GPU/SoC coverage matrix to Firebase Test Lab,
# rotating across multiple Google accounts (5 physical devices/day per account on Spark tier).
# Dispatches are run in parallel (one background process per device) so all uploads
# proceed concurrently rather than serially.
#
# Each row in MATRIX is: codename | api | account | project | label
#
# Output:
#   $OUT_DIR/dispatch.csv     — codename,matrix-id,console-url,bucket-prefix
#   $OUT_DIR/<codename>/dispatch.log — gcloud dispatch output

set -uo pipefail

APK="${1:-}"
OUT_DIR="${2:-/Users/yee/data/dev/app/labo/game_engine/tmp/coordinator/regression-android/ftl-matrix-$(date +%Y%m%d-%H%M%S)}"

if [ -z "$APK" ] || [ ! -f "$APK" ]; then
    echo "Usage: $0 <APK_PATH> [OUT_DIR]" >&2
    echo "  APK not found: ${APK:-(empty)}" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

# codename | api | account | project | label
MATRIX=$(cat <<'EOF'
shiba|34|aicoding.yee@gmail.com|ftl-aicoding-23858|Pixel 8 / Tensor G3 / Mali-G715
b0q|33|dmeapp.usa@gmail.com|ftl-dmeapp-23893|Galaxy S22 Ultra / SD 8 Gen 1 / Adreno 730
oriole|33|hohuukieule@gmail.com|ftl-hohuu-23505|Pixel 6 / Tensor G1 / Mali-G78
r8q|33|khanhkhanh77960@gmail.com|ftl-khanh-22226|Galaxy S20 FE 5G / SD 865 / Adreno 650
redfin|30|laboladoads@gmail.com|ftl-laboladoads-23655|Pixel 5 / SD 765G / Adreno 620
a54x|34|laboladopay@gmail.com|ftl-laboladopay-22264|Galaxy A54 5G / Exynos 1380 / Mali-G68
husky|35|no5@drsmarttrade.com|ftl-no5-23698|Pixel 8 Pro / Tensor G3 / Mali-G715
austin|33|pkysoft@gmail.com|ftl-pkysoft-23615|moto g 5G 2022 / SD 480 / Adreno 619
OP5552L1|34|countrymancostanza9@gmail.com|ftl-costanza-22188|OnePlus 10T 5G / SD 8+ Gen 1 / Adreno 730
EOF
)

dispatch_one() {
    local codename="$1" api="$2" account="$3" project="$4" label="$5"
    local DEVICE_DIR="$OUT_DIR/$codename"
    mkdir -p "$DEVICE_DIR"
    local LOG="$DEVICE_DIR/dispatch.log"

    gcloud --account="$account" --project="$project" \
        firebase test android run \
        --type=game-loop \
        --scenario-numbers=1 \
        --app="$APK" \
        --device "model=$codename,version=$api,locale=en,orientation=portrait" \
        --timeout 5m \
        --no-record-video \
        --no-performance-metrics \
        --async \
        > "$LOG" 2>&1
    echo "$?" > "$DEVICE_DIR/dispatch.rc"
}

echo "==> Parallel dispatch: APK=$APK"
echo "==> Output dir: $OUT_DIR"
echo ""

PIDS=()
while IFS='|' read -r codename api account project label; do
    [ -z "$codename" ] && continue
    echo "    launching $codename ($label) [account=$account]"
    dispatch_one "$codename" "$api" "$account" "$project" "$label" &
    PIDS+=($!)
done <<< "$MATRIX"

echo ""
echo "==> Waiting for ${#PIDS[@]} parallel dispatches..."
for pid in "${PIDS[@]}"; do wait "$pid"; done
echo "==> All dispatches complete."
echo ""

CSV="$OUT_DIR/dispatch.csv"
echo "codename,api,matrix_id,console_url,bucket_prefix,account,project,label" > "$CSV"

while IFS='|' read -r codename api account project label; do
    [ -z "$codename" ] && continue
    LOG="$OUT_DIR/$codename/dispatch.log"
    rc=$(cat "$OUT_DIR/$codename/dispatch.rc" 2>/dev/null || echo "?")
    matrix_id=$(grep -oE 'matrix-[a-z0-9]+' "$LOG" | head -1)
    bucket=$(grep -oE 'gs://[^ ]+/[0-9]{4}-[0-9]{2}-[0-9]{2}_[^/]+/' "$LOG" | head -1 | sed 's:/$::')
    console=$(grep -oE 'https://console\.firebase\.google\.com[^ ]+matrices/[0-9]+' "$LOG" | head -1)

    if [ "$rc" = "0" ] && [ -n "$matrix_id" ]; then
        echo "$codename,$api,$matrix_id,$console,$bucket,$account,$project,$label" >> "$CSV"
    else
        echo "$codename,$api,DISPATCH_FAILED_rc=$rc,,,$account,$project,$label" >> "$CSV"
    fi
done <<< "$MATRIX"

echo "==> Dispatch summary: $CSV"
column -ts, "$CSV" 2>/dev/null || cat "$CSV"
