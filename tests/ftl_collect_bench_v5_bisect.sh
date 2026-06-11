#!/usr/bin/env bash
# Collect FTL bench v5 bisect logcats (4 matrices).
# Usage: bash tests/ftl_collect_bench_v5_bisect.sh [OUT_DIR]
set -uo pipefail

OUT_DIR="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v5-bisect}"
[ -d "$OUT_DIR" ] || { echo "ERROR: $OUT_DIR not found"; exit 1; }

slot() { # device -> "account|project"
    case "$1" in
        bluejay) echo "laboladoads@gmail.com|ftl-laboladoads-23655" ;;
        CPH2449) echo "countrymancostanza9@gmail.com|ftl-costanza-22188" ;;
    esac
}

ok=0; miss=0
for variant in opt-a opt-ab; do
  for device in bluejay CPH2449; do
    dir="$OUT_DIR/${variant}-${device}"
    [ -d "$dir" ] || { echo "  SKIP $variant/$device (no dir)"; continue; }
    logcat="$dir/logcat.txt"
    [ -s "$logcat" ] && { echo "  HAVE $variant/$device"; ok=$((ok+1)); continue; }

    s=$(slot "$device"); acct="${s%%|*}" proj="${s##*|}"

    # Find dispatch log with valid bucket URL
    # Priority: dispatch2.log > dispatch.log with matrix ID > retry log
    dlog=""
    for candidate in "$dir/dispatch2.log" "$dir/dispatch.log" "$OUT_DIR/opt-a-bluejay-retry/dispatch.log"; do
        [ -f "$candidate" ] || continue
        grep -q 'storage/browser/' "$candidate" 2>/dev/null && { dlog="$candidate"; break; }
    done

    gs=$(grep -oE 'storage/browser/[^ ]+' "$dlog" 2>/dev/null | head -1 \
         | sed -E 's|.*storage/browser/|gs://|; s|[]/]+$||')
    [ -n "$gs" ] || { echo "  ⚠️ $variant/$device: no bucket in $dlog"; miss=$((miss+1)); continue; }

    echo "  Fetching $variant/$device via $acct ..."
    src=$(gcloud storage ls --account="$acct" --project="$proj" "$gs/**" 2>/dev/null \
          | grep -E '/logcat$' | head -1)
    if [ -n "$src" ]; then
        gcloud storage cp --account="$acct" --project="$proj" "$src" "$logcat" >/dev/null 2>&1 || true
    fi
    if [ -s "$logcat" ]; then echo "    ✅ got logcat"; ok=$((ok+1)); else echo "    ⚠️ no logcat yet"; miss=$((miss+1)); fi
  done
done
echo ""
echo "collected=$ok missing=$miss  ($OUT_DIR)"
