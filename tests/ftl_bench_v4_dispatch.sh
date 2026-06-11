#!/usr/bin/env bash
# FTL Bench v4 — TRUE before/after. Dispatches 12 game-loop matrices:
#   variant ∈ {baseline(c0d30547), optimized(tip)} × backend ∈ {vk,gl} × device ∈ {redfin,bluejay,CPH2449}
# Devices use CORRECT apis: redfin/30, bluejay/32, CPH2449/34.
# Each (backend,device) account runs BOTH variants (2 tests/account).
#
# Usage:   bash tests/ftl_bench_v4_dispatch.sh [OUT_DIR]
# Collect: bash tests/ftl_collect_bench_v4.sh <OUT_DIR>
set -uo pipefail

BUILDS_DIR="/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds"
OUT_DIR="${1:-$BUILDS_DIR/results-v4}"
mkdir -p "$OUT_DIR"

apk_for() { # variant backend
    echo "$BUILDS_DIR/$1-$([ "$2" = vk ] && echo vulkan || echo gles).apk"
}
get_api() {
    case "$1" in redfin) echo 30;; bluejay) echo 32;; CPH2449) echo 34;; *) echo ""; return 1;; esac
}
# account|project keyed by backend-device (same account runs baseline+optimized)
get_slot() {
    case "$1" in
        vk-redfin)  echo "hohuukieule@gmail.com|ftl-hohuu-23505" ;;
        gl-redfin)  echo "aicoding.yee@gmail.com|ftl-aicoding-23858" ;;
        vk-bluejay) echo "laboladoads@gmail.com|ftl-laboladoads-23655" ;;
        gl-bluejay) echo "pkysoft@gmail.com|ftl-pkysoft-23615" ;;
        vk-CPH2449) echo "countrymancostanza9@gmail.com|ftl-costanza-22188" ;;
        gl-CPH2449) echo "khanhkhanh77960@gmail.com|ftl-khanh-22226" ;;
        *) echo ""; return 1 ;;
    esac
}

# preflight: all 4 APKs present
for v in baseline optimized; do for b in vk gl; do
    f=$(apk_for "$v" "$b"); [ -f "$f" ] || { echo "ERROR: missing $f"; exit 1; }
done; done

dispatch() { # variant backend device
    local variant="$1" backend="$2" device="$3"
    local apk; apk=$(apk_for "$variant" "$backend")
    local api; api=$(get_api "$device")
    local slot; slot=$(get_slot "${backend}-${device}")
    local acct="${slot%%|*}" proj="${slot##*|}"
    local dir="$OUT_DIR/${variant}-${backend}-${device}"; mkdir -p "$dir"
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
    echo "  [$variant/$backend/$device api=$api] ${mid:-DISPATCH_FAILED} ($acct)"
}

echo "=== FTL Bench v4 Dispatch — TRUE before/after (12 matrices) ==="
for v in baseline optimized; do for b in vk gl; do
    f=$(apk_for "$v" "$b"); echo "  $v-$b: $(du -sh "$f"|cut -f1)  $f"
done; done
echo "  output: $OUT_DIR"
echo ""

PIDS=()
for variant in baseline optimized; do
  for backend in vk gl; do
    for device in redfin bluejay CPH2449; do
      dispatch "$variant" "$backend" "$device" & PIDS+=($!)
    done
  done
done
echo ""
echo "Waiting for ${#PIDS[@]} dispatches..."
for pid in "${PIDS[@]}"; do wait "$pid" || true; done

echo ""
echo "=== Dispatch Summary ==="
CSV="$OUT_DIR/dispatch.csv"
echo "variant,backend,device,api,matrix_id,account,project" > "$CSV"
for variant in baseline optimized; do
  for backend in vk gl; do
    for device in redfin bluejay CPH2449; do
      api=$(get_api "$device"); dir="$OUT_DIR/${variant}-${backend}-${device}"
      mid=$(grep -oE 'matrix-[a-z0-9]+' "$dir/dispatch.log" 2>/dev/null | head -1 || echo "")
      slot=$(get_slot "${backend}-${device}"); acct="${slot%%|*}" proj="${slot##*|}"
      if [ -n "$mid" ]; then
        echo "$variant,$backend,$device,$api,$mid,$acct,$proj" >> "$CSV"
        echo "  ✅ $variant/$backend/$device (api=$api): $mid"
      else
        echo "$variant,$backend,$device,$api,DISPATCH_FAILED,$acct,$proj" >> "$CSV"
        echo "  ❌ $variant/$backend/$device (api=$api): rc=$(cat "$dir/dispatch.rc" 2>/dev/null)"
        tail -3 "$dir/dispatch.log" 2>/dev/null | sed 's/^/     /'
      fi
    done
  done
done
echo ""
echo "Tests running on FTL (~11-15 min). Collect:"
echo "  bash tests/ftl_collect_bench_v4.sh $OUT_DIR"
echo "  CSV: $CSV"
