#!/usr/bin/env bash
# FTL Bench v5 Bisect — Vulkan only, bisect variants
# Dispatches 4 matrices: variant ∈ {opt-a, opt-ab} × device ∈ {bluejay, CPH2449}
# Accounts reused from v4 (vk-bluejay and vk-CPH2449 slots)
#
# Usage: bash tests/ftl_bench_v5_bisect_dispatch.sh [OUT_DIR]
set -uo pipefail

BUILDS_DIR="/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds"
OUT_DIR="${1:-$BUILDS_DIR/results-v5-bisect}"
mkdir -p "$OUT_DIR"

apk_for() { # variant
    echo "$BUILDS_DIR/bisect-$1-vulkan.apk"
}
get_api() {
    case "$1" in bluejay) echo 32;; CPH2449) echo 34;; *) echo ""; return 1;; esac
}
# Same vk accounts as v4
get_slot() {
    case "$1" in
        bluejay) echo "laboladoads@gmail.com|ftl-laboladoads-23655" ;;
        CPH2449) echo "countrymancostanza9@gmail.com|ftl-costanza-22188" ;;
        *) echo ""; return 1 ;;
    esac
}

# preflight
for v in opt-a opt-ab; do
    f=$(apk_for "$v"); [ -f "$f" ] || { echo "ERROR: missing $f"; exit 1; }
    sz=$(du -sh "$f" | cut -f1); echo "  $v: $sz  $f"
done
echo "  output: $OUT_DIR"
echo ""

dispatch() { # variant device
    local variant="$1" device="$2"
    local apk; apk=$(apk_for "$variant")
    local api; api=$(get_api "$device")
    local slot; slot=$(get_slot "$device")
    local acct="${slot%%|*}" proj="${slot##*|}"
    local dir="$OUT_DIR/${variant}-${device}"; mkdir -p "$dir"
    gcloud --account="$acct" --project="$proj" \
        firebase test android run \
        --type=game-loop --scenario-numbers=5 \
        --app="$apk" \
        --device="model=$device,version=$api,locale=en,orientation=portrait" \
        --timeout=660s \
        --no-record-video --no-performance-metrics --async \
        > "$dir/dispatch.log" 2>&1
    echo "$?" > "$dir/dispatch.rc"
    local mid; mid=$(grep -oE 'matrix-[a-z0-9]+' "$dir/dispatch.log" 2>/dev/null | head -1 || echo "")
    echo "  [$variant/$device api=$api] ${mid:-DISPATCH_FAILED} ($acct)"
}

echo "=== FTL Bench v5 Bisect Dispatch — Vulkan only, 4 matrices ==="
PIDS=()
for variant in opt-a opt-ab; do
  for device in bluejay CPH2449; do
    dispatch "$variant" "$device" & PIDS+=($!)
  done
done
echo "Waiting for ${#PIDS[@]} dispatches..."
for pid in "${PIDS[@]}"; do wait "$pid" || true; done

echo ""
echo "=== Dispatch Summary ==="
CSV="$OUT_DIR/dispatch.csv"
echo "variant,device,matrix_id,rc" > "$CSV"
for variant in opt-a opt-ab; do
  for device in bluejay CPH2449; do
    dir="$OUT_DIR/${variant}-${device}"
    rc=$(cat "$dir/dispatch.rc" 2>/dev/null || echo "?")
    mid=$(grep -oE 'matrix-[a-z0-9]+' "$dir/dispatch.log" 2>/dev/null | head -1 || echo "")
    echo "  $variant/$device: mid=${mid:-NONE} rc=$rc"
    echo "$variant,$device,$mid,$rc" >> "$CSV"
  done
done
echo ""
echo "CSV: $CSV"
echo "Collect: bash tests/ftl_collect_bench_v5_bisect.sh $OUT_DIR"
