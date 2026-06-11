#!/usr/bin/env bash
# Collect FTL bench v3 results from GCS logcat.
# Usage: bash tests/ftl_collect_bench_v3.sh <out_dir>

set -uo pipefail

OUT_DIR="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v3}"
[ -f "$OUT_DIR/dispatch.csv" ] || { echo "ERROR: $OUT_DIR/dispatch.csv not found"; exit 1; }

echo "==> Collecting FTL v3 bench results from $OUT_DIR"
echo ""

declare -A PERF_DATA

while IFS=',' read -r device variant matrix_id account; do
    [ "$device" = "device" ] && continue
    [ -z "$matrix_id" ] || [[ "$matrix_id" == DISPATCH_FAILED* ]] && {
        echo "  SKIP $device/$variant (dispatch failed)"
        continue
    }

    echo "  Fetching $device/$variant ($matrix_id) via $account..."
    DIR="$OUT_DIR/${variant}-${device}"
    [ "$variant" = "official" ] && DIR="$OUT_DIR/official-${device}"
    mkdir -p "$DIR"
    LOGCAT="$DIR/logcat.txt"

    # Get GCS bucket from dispatch log
    bucket=$(grep -oE 'gs://[^ ]+' "$DIR/dispatch.log" 2>/dev/null | head -1 || \
             grep -oE 'test-lab-[a-z0-9-]+' "$DIR/dispatch.log" 2>/dev/null | head -1 | sed 's/^/gs:\/\//')

    if [ -n "$bucket" ]; then
        gsutil -q -u "$(gcloud config get-value project 2>/dev/null)" \
            cp "gs://test-lab-$(echo $bucket | grep -oE 'test-lab-[^/]+' | sed 's/test-lab-//')/$matrix_id/**/logcat" \
            "$LOGCAT" 2>/dev/null || \
        gsutil --account="$account" -q cp "${bucket}/**/${matrix_id}/**/logcat" "$LOGCAT" 2>/dev/null || \
        gsutil --account="$account" -q cp "${bucket}/**/logcat" "$LOGCAT" 2>/dev/null || true
    fi

    if [ ! -s "$LOGCAT" ]; then
        # Try downloading via matrix describe
        bucket2=$(gcloud --account="$account" firebase test android runs describe "$matrix_id" \
            --format='value(resultsStorage.resultsUri)' 2>/dev/null || echo "")
        if [ -n "$bucket2" ]; then
            gsutil --account="$account" -q cp "${bucket2}/**/logcat" "$LOGCAT" 2>/dev/null || true
        fi
    fi

    if [ ! -s "$LOGCAT" ]; then
        echo "    WARNING: no logcat for $device/$variant"
        continue
    fi

    # Extract PERF RESULTS section
    perf_section=$(sed -n '/=== PERF RESULTS ===/,/=== END RESULTS ===/p' "$LOGCAT" | \
        grep -v "PERF RESULTS\|END RESULTS" || \
        grep -E "Scene [A-J].*fps|throughput|obj_per_sec" "$LOGCAT" | head -40)

    if [ -n "$perf_section" ]; then
        PERF_DATA["${device}_${variant}"]="$perf_section"
        echo "    ✅ Got PERF data"
    else
        echo "    ⚠️  No PERF RESULTS in logcat"
        grep -E "BENCH|bench|fps|FPS|scene" "$LOGCAT" | head -5 | sed 's/^/    /'
    fi
done < "$OUT_DIR/dispatch.csv"

echo ""
echo "=== PERF RESULTS SUMMARY ==="
echo ""

# Print per-device table
for device in redfin bluejay CPH2449; do
    case "$device" in
        redfin)  label="Pixel 5 / Adreno 620" ;;
        bluejay) label="Pixel 6a / Mali G57" ;;
        CPH2449) label="OnePlus 11 / Adreno 730" ;;
    esac
    echo "### $label"
    for variant in vk gl official; do
        key="${device}_${variant}"
        if [ -n "${PERF_DATA[$key]+x}" ]; then
            echo "  [$variant]"
            echo "${PERF_DATA[$key]}" | sed 's/^/    /'
        fi
    done
    echo ""
done

echo "Raw logcat files: $OUT_DIR/*/logcat.txt"
