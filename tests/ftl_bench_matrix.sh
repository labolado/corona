#!/usr/bin/env bash
# FTL Benchmark matrix — dispatches bgfx-demo scenario 2 (bench) to multiple devices.
# Requires GameLoopActivity scenario 2 support (sets SOLAR2D_TEST=bench via Os.setenv).
#
# Usage: ftl_bench_matrix.sh <APK_PATH> [OUT_DIR]
#
# Bench output in logcat: "[Bench] NNNN objects: avg=XX.X min=XX.X max=XX.X FPS"
# Collect results with: ftl_collect_bench.sh <matrix_dir>

set -uo pipefail

APK="${1:-}"
OUT_DIR="${2:-/Users/yee/data/dev/app/labo/game_engine/tmp/coordinator/regression-android/ftl-bench-$(date +%Y%m%d-%H%M%S)}"

if [ -z "$APK" ] || [ ! -f "$APK" ]; then
    echo "Usage: $0 <APK_PATH> [OUT_DIR]" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

# Device matrix for benchmark — balanced across GPU families
# codename | api | account | project | label
MATRIX=$(cat <<'EOF'
oriole|31|aicoding.yee@gmail.com|ftl-aicoding-23858|Pixel 6 / Tensor G1 / Mali-G78
OP5552L1|31|countrymancostanza9@gmail.com|ebook-ocr-488201|OnePlus 11 / SD 8 Gen 2 / Adreno 740
b0q|33|dmeapp.usa@gmail.com|ftl-dmeapp-23893|Galaxy S22 Ultra / SD 8 Gen 1 / Adreno 730
husky|35|aicoding.yee@gmail.com|ftl-aicoding-23858|Pixel 8 Pro / Tensor G3 / Mali-G715
caiman|35|dmeapp.usa@gmail.com|ftl-dmeapp-23893|Pixel 9 Pro / Tensor G4 / Mali-G715
pa3q|36|laboladoads@gmail.com|ftl-laboladoads-23655|Galaxy S25 Ultra / SD 8 Elite / Adreno 830
EOF
)

dispatch_bench() {
    local codename="$1" api="$2" account="$3" project="$4" label="$5"
    local DEVICE_DIR="$OUT_DIR/$codename"
    mkdir -p "$DEVICE_DIR"
    local LOG="$DEVICE_DIR/dispatch.log"
    # Scenario 2 = bench mode; timeout 10m to cover all 5 stress levels
    gcloud --account="$account" --project="$project" \
        firebase test android run \
        --type=game-loop --scenario-numbers=2 --app="$APK" \
        --device "model=$codename,version=$api,locale=en,orientation=portrait" \
        --timeout 10m --no-record-video --no-performance-metrics --async \
        > "$LOG" 2>&1
    echo "$?" > "$DEVICE_DIR/dispatch.rc"
}

echo "==> FTL Bench dispatch: APK=$APK"
echo "==> Output: $OUT_DIR"
echo ""

PIDS=()
while IFS='|' read -r codename api account project label; do
    [ -z "$codename" ] && continue
    echo "    launching $codename ($label) [$account]"
    dispatch_bench "$codename" "$api" "$account" "$project" "$label" &
    PIDS+=($!)
done <<< "$MATRIX"

echo ""
echo "==> Waiting for ${#PIDS[@]} dispatches..."
for pid in "${PIDS[@]}"; do wait "$pid"; done
echo "==> All dispatches complete."
echo ""

CSV="$OUT_DIR/dispatch.csv"
echo "codename,api,matrix_id,console_url,account,project,label" > "$CSV"
while IFS='|' read -r codename api account project label; do
    [ -z "$codename" ] && continue
    LOG="$OUT_DIR/$codename/dispatch.log"
    rc=$(cat "$OUT_DIR/$codename/dispatch.rc" 2>/dev/null || echo "?")
    matrix_id=$(grep -oE 'matrix-[a-z0-9]+' "$LOG" | head -1)
    console=$(grep -oE 'https://console\.firebase\.google\.com[^ ]+matrices/[0-9]+' "$LOG" | head -1)
    if [ "$rc" = "0" ] && [ -n "$matrix_id" ]; then
        echo "$codename,$api,$matrix_id,$console,$account,$project,$label" >> "$CSV"
    else
        echo "$codename,$api,DISPATCH_FAILED_rc=$rc,,,$account,$project,$label" >> "$CSV"
    fi
done <<< "$MATRIX"

echo "==> Bench dispatch summary:"
column -ts, "$CSV" 2>/dev/null || cat "$CSV"
echo ""
echo "==> After tests complete (~15 min), collect results with:"
echo "    bash tests/ftl_collect_bench.sh $OUT_DIR"
