#!/usr/bin/env bash
# FTL Phase 2 expanded matrix — 14 devices to find more bgfx init/runtime issues.
# Phase 1 (10 devices) already covered classic GPU families. Phase 2 expands:
#   - Tensor G3 family verification (husky) → confirm Pixel 8 crash pattern
#   - latest SoCs (Pixel 9/10, S24/S25, SD 8 Elite Adreno 830)
#   - foldables + tablets (surface lifecycle / large screen)
#   - older baselines (Galaxy S9 API 29, Pixel 6a)
#
# Usage: ftl_phase2_matrix.sh <APK_PATH> [OUT_DIR]

set -uo pipefail

APK="${1:-}"
OUT_DIR="${2:-/Users/yee/data/dev/app/labo/game_engine/tmp/coordinator/regression-android/ftl-phase2-$(date +%Y%m%d-%H%M%S)}"

if [ -z "$APK" ] || [ ! -f "$APK" ]; then
    echo "Usage: $0 <APK_PATH> [OUT_DIR]" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

# codename | api | account | project | label
MATRIX=$(cat <<'EOF'
husky|35|aicoding.yee@gmail.com|ftl-aicoding-23858|Pixel 8 Pro / Tensor G3 / Mali-G715 *** CRASH-VERIFY ***
caiman|35|dmeapp.usa@gmail.com|ftl-dmeapp-23893|Pixel 9 Pro / Tensor G4 / Mali-G715
tokay|36|hohuukieule@gmail.com|ftl-hohuu-23505|Pixel 9 / Tensor G4 / Mali-G715
frankel|36|khanhkhanh77960@gmail.com|ftl-khanh-22226|Pixel 10 / Tensor G5
pa3q|36|laboladoads@gmail.com|ftl-laboladoads-23655|Galaxy S25 Ultra / SD 8 Elite / Adreno 830
e3q|36|laboladopay@gmail.com|ftl-laboladopay-22264|Galaxy S24 Ultra / SD 8 Gen 3 / Adreno 750
dm3q|34|pkysoft@gmail.com|ftl-pkysoft-23615|Galaxy S23 Ultra / SD 8 Gen 2 / Adreno 740
q6q|34|countrymancostanza9@gmail.com|ftl-costanza-22188|Galaxy Z Fold6 / SD 8 Gen 3
b6q|34|countrymancostanza9@gmail.com|ftl-costanza-22050|Galaxy Z Flip6 / SD 8 Gen 3
gts9wifi|34|aicoding.yee@gmail.com|ftl-aicoding-23858|Galaxy Tab S9 / SD 8 Gen 2
starlte|29|dmeapp.usa@gmail.com|ftl-dmeapp-23893|Galaxy S9 / Exynos 9810 / Mali-G72 (legacy)
bluejay|32|hohuukieule@gmail.com|ftl-hohuu-23505|Pixel 6a / Tensor G1 / Mali-G78
r0q|34|khanhkhanh77960@gmail.com|ftl-khanh-22226|Galaxy S22 / SD 8 Gen 1 / Adreno 730 (vs b0q)
dubai|34|laboladoads@gmail.com|ftl-laboladoads-23655|motorola edge 30 / SD 778G+ / Adreno 642L
EOF
)

dispatch_one() {
    local codename="$1" api="$2" account="$3" project="$4" label="$5"
    local DEVICE_DIR="$OUT_DIR/$codename"
    mkdir -p "$DEVICE_DIR"
    local LOG="$DEVICE_DIR/dispatch.log"
    gcloud --account="$account" --project="$project" \
        firebase test android run \
        --type=game-loop --scenario-numbers=1 --app="$APK" \
        --device "model=$codename,version=$api,locale=en,orientation=portrait" \
        --timeout 5m --no-record-video --no-performance-metrics --async \
        > "$LOG" 2>&1
    echo "$?" > "$DEVICE_DIR/dispatch.rc"
}

echo "==> Phase 2 parallel dispatch: APK=$APK"
echo "==> Output: $OUT_DIR"
echo ""

PIDS=()
while IFS='|' read -r codename api account project label; do
    [ -z "$codename" ] && continue
    echo "    launching $codename ($label) [$account]"
    dispatch_one "$codename" "$api" "$account" "$project" "$label" &
    PIDS+=($!)
done <<< "$MATRIX"

echo ""
echo "==> Waiting for ${#PIDS[@]} dispatches..."
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
    bucket=$(grep -oE 'console\.developers\.google\.com/storage/browser/[^]]+' "$LOG" | head -1 | sed 's|console.developers.google.com/storage/browser/|gs://|; s|/$||')
    console=$(grep -oE 'https://console\.firebase\.google\.com[^ ]+matrices/[0-9]+' "$LOG" | head -1)
    if [ "$rc" = "0" ] && [ -n "$matrix_id" ]; then
        echo "$codename,$api,$matrix_id,$console,$bucket,$account,$project,$label" >> "$CSV"
    else
        echo "$codename,$api,DISPATCH_FAILED_rc=$rc,,,$account,$project,$label" >> "$CSV"
    fi
done <<< "$MATRIX"

echo "==> Dispatch summary:"
column -ts, "$CSV" 2>/dev/null || cat "$CSV"
