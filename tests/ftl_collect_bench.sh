#!/usr/bin/env bash
# Collect FTL bench results from logcat after test completion.
# Usage: ftl_collect_bench.sh <dispatch_dir> [account]
#
# Downloads logcat from each matrix result bucket, extracts [Bench] lines,
# and prints a comparison table.

set -uo pipefail

DISPATCH_DIR="${1:-}"
DEFAULT_ACCOUNT="${2:-aicoding.yee@gmail.com}"

if [ -z "$DISPATCH_DIR" ] || [ ! -f "$DISPATCH_DIR/dispatch.csv" ]; then
    echo "Usage: $0 <dispatch_dir>" >&2
    exit 1
fi

echo "==> Collecting FTL bench results from: $DISPATCH_DIR"
echo ""

declare -A RESULTS

while IFS=',' read -r codename api matrix_id console_url account project label; do
    [ "$codename" = "codename" ] && continue
    [ -z "$matrix_id" ] || [[ "$matrix_id" == DISPATCH_FAILED* ]] && {
        echo "  SKIP $codename (dispatch failed)"
        continue
    }

    use_account="${account:-$DEFAULT_ACCOUNT}"
    echo "  Fetching $codename ($matrix_id) via $use_account..."

    # Get test result bucket from matrix
    RESULT_DIR="$DISPATCH_DIR/$codename"
    mkdir -p "$RESULT_DIR"
    LOGCAT="$RESULT_DIR/logcat.txt"

    # Find GCS bucket from dispatch log
    bucket=$(grep -oE 'gs://[^/]+' "$RESULT_DIR/dispatch.log" | head -1)
    if [ -z "$bucket" ]; then
        # Fallback: extract from console URL pattern
        bucket=$(grep -oE 'storage/browser/([^/\]]+)' "$RESULT_DIR/dispatch.log" | head -1 | sed 's|storage/browser/|gs://|')
    fi

    if [ -n "$bucket" ]; then
        # Download logcat from GCS result bucket
        gsutil -q cp "${bucket}/${matrix_id}/**/**/logcat" "$LOGCAT" 2>/dev/null || \
        gsutil -q cp "${bucket}/**/logcat" "$LOGCAT" 2>/dev/null || true
    fi

    if [ ! -s "$LOGCAT" ]; then
        echo "    WARNING: no logcat for $codename"
        RESULTS["$codename"]="NO_DATA"
        continue
    fi

    # Parse [Bench] lines
    bench_data=$(grep '\[Bench\].*objects:' "$LOGCAT" | \
        sed 's/.*\[Bench\] //' | \
        awk '{
            split($0, a, " ")
            count = a[1]
            for(i=1; i<=NF; i++) {
                if (a[i] ~ /avg=/) {
                    split(a[i], b, "=")
                    printf "%s:%s ", count, b[2]
                }
            }
        }')

    if [ -z "$bench_data" ]; then
        echo "    WARNING: no bench data in logcat for $codename"
        RESULTS["$codename"]="NO_BENCH"
    else
        RESULTS["$codename"]="$bench_data"
        echo "    $codename: $bench_data"
    fi

done < "$DISPATCH_DIR/dispatch.csv"

echo ""
echo "=== Bench Summary ==="
printf "%-20s %8s %8s %8s %8s %8s\n" "Device" "500obj" "1000obj" "2000obj" "3000obj" "5000obj"
printf "%-20s %8s %8s %8s %8s %8s\n" "$(printf '%.0s-' {1..20})" "--------" "--------" "--------" "--------" "--------"

while IFS=',' read -r codename api matrix_id console_url account project label; do
    [ "$codename" = "codename" ] && continue
    data="${RESULTS[$codename]:-N/A}"

    fps500=$(echo "$data" | grep -oE '500:[0-9.]+' | cut -d: -f2 || echo "-")
    fps1000=$(echo "$data" | grep -oE '1000:[0-9.]+' | cut -d: -f2 || echo "-")
    fps2000=$(echo "$data" | grep -oE '2000:[0-9.]+' | cut -d: -f2 || echo "-")
    fps3000=$(echo "$data" | grep -oE '3000:[0-9.]+' | cut -d: -f2 || echo "-")
    fps5000=$(echo "$data" | grep -oE '5000:[0-9.]+' | cut -d: -f2 || echo "-")

    short_name=$(echo "$label" | cut -d'/' -f1 | xargs)
    printf "%-20s %8s %8s %8s %8s %8s\n" \
        "${short_name:0:20}" \
        "${fps500:--}" "${fps1000:--}" "${fps2000:--}" "${fps3000:--}" "${fps5000:--}"
done < "$DISPATCH_DIR/dispatch.csv"
