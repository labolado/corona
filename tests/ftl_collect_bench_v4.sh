#!/usr/bin/env bash
# Collect FTL bench v4 logcats (12 matrices) from GCS.
# Bucket comes from each dispatch.log line:
#   Raw results will be stored ... storage/browser/<bucket>/<path>/
# Logcat lives at gs://<bucket>/<path>/<device>-<api>-<locale>-<orient>/logcat
# Usage: bash tests/ftl_collect_bench_v4.sh [OUT_DIR]
set -uo pipefail

OUT_DIR="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v4}"
[ -d "$OUT_DIR" ] || { echo "ERROR: $OUT_DIR not found"; exit 1; }

slot() { # backend-device -> "account|project"
    case "$1" in
        vk-redfin)  echo "hohuukieule@gmail.com|ftl-hohuu-23505" ;;
        gl-redfin)  echo "aicoding.yee@gmail.com|ftl-aicoding-23858" ;;
        vk-bluejay) echo "laboladoads@gmail.com|ftl-laboladoads-23655" ;;
        gl-bluejay) echo "pkysoft@gmail.com|ftl-pkysoft-23615" ;;
        vk-CPH2449) echo "countrymancostanza9@gmail.com|ftl-costanza-22188" ;;
        gl-CPH2449) echo "khanhkhanh77960@gmail.com|ftl-khanh-22226" ;;
    esac
}

ok=0; miss=0
for variant in baseline optimized; do
  for backend in vk gl; do
    for device in redfin bluejay CPH2449; do
      dir="$OUT_DIR/${variant}-${backend}-${device}"
      [ -d "$dir" ] || { echo "  SKIP $variant/$backend/$device (no dir)"; continue; }
      logcat="$dir/logcat.txt"
      [ -s "$logcat" ] && { echo "  HAVE $variant/$backend/$device"; ok=$((ok+1)); continue; }

      s=$(slot "${backend}-${device}"); acct="${s%%|*}" proj="${s##*|}"
      # bucket from dispatch.log (storage/browser/<bucket>/<path>/)
      gs=$(grep -oE 'storage/browser/[^ ]+' "$dir/dispatch.log" 2>/dev/null | head -1 \
           | sed -E 's|.*storage/browser/|gs://|; s|[]/]+$||')
      [ -n "$gs" ] || { echo "  ⚠️ $variant/$backend/$device: no bucket in dispatch.log"; miss=$((miss+1)); continue; }

      echo "  Fetching $variant/$backend/$device via $acct ..."
      src=$(gcloud storage ls --account="$acct" --project="$proj" "$gs/**" 2>/dev/null \
            | grep -E '/logcat$' | head -1)
      if [ -n "$src" ]; then
        gcloud storage cp --account="$acct" --project="$proj" "$src" "$logcat" >/dev/null 2>&1 || true
      fi
      if [ -s "$logcat" ]; then echo "    ✅ got logcat"; ok=$((ok+1)); else echo "    ⚠️ no logcat yet"; miss=$((miss+1)); fi
    done
  done
done
echo ""
echo "collected=$ok missing=$miss  ($OUT_DIR)"
