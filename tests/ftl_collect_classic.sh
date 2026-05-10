#!/usr/bin/env bash
# Collect logcats from FTL classic matrix results and print a pass/fail summary.
# Usage: ftl_collect_classic.sh <dispatch_dir>
#
# Reads dispatch.csv from the dir, downloads logcat from GCS for each device,
# checks for crash / GameLoop auto-finish, prints a table.

set -uo pipefail

DISPATCH_DIR="${1:-}"
if [ -z "$DISPATCH_DIR" ] || [ ! -f "$DISPATCH_DIR/dispatch.csv" ]; then
    echo "Usage: $0 <dispatch_dir>" >&2
    exit 1
fi

echo "==> Collecting FTL classic matrix results from: $DISPATCH_DIR"
echo ""

PASS=0
FAIL=0
CRASH=0
SKIP=0

declare -A STATUS
declare -A DETAIL

while IFS=',' read -r codename api matrix_id console_url bucket account project label; do
    [ "$codename" = "codename" ] && continue

    if [[ "$matrix_id" == DISPATCH_FAILED* ]]; then
        echo "  SKIP $codename (dispatch failed: $matrix_id)"
        STATUS["$codename"]="SKIP"
        DETAIL["$codename"]="dispatch failed"
        ((SKIP++)) || true
        continue
    fi

    LOGCAT="$DISPATCH_DIR/$codename/logcat.txt"
    if [ ! -f "$LOGCAT" ] || [ ! -s "$LOGCAT" ]; then
        # Try to find the GCS path from dispatch.log
        DISPATCH_LOG="$DISPATCH_DIR/$codename/dispatch.log"
        GCS_BUCKET=$(grep -oE 'test-lab-[a-z0-9]+-[a-z0-9]+' "$DISPATCH_LOG" 2>/dev/null | head -1)
        GCS_PATH=$(grep "console.developers.google.com/storage/browser" "$DISPATCH_LOG" 2>/dev/null | \
            grep -oE 'test-lab-[^]]+' | head -1)

        if [ -n "$GCS_PATH" ]; then
            echo "  Downloading logcat for $codename ($label) via $account..."
            gcloud config set account "$account" 2>/dev/null
            mkdir -p "$DISPATCH_DIR/$codename"
            # Try common model-api-locale-orientation dir pattern
            MODEL_DIR=$(gsutil ls "gs://$GCS_PATH/" 2>/dev/null | grep -E "${codename}-${api}-" | head -1)
            if [ -n "$MODEL_DIR" ]; then
                gsutil -q cp "${MODEL_DIR}logcat" "$LOGCAT" 2>/dev/null || true
            fi
        fi
    fi

    if [ ! -s "$LOGCAT" ]; then
        echo "    WARNING: no logcat for $codename"
        STATUS["$codename"]="NO_DATA"
        DETAIL["$codename"]="logcat not found"
        continue
    fi

    # Analyze logcat
    if grep -q "Fatal signal\|crash_dump64\|beginning of crash" "$LOGCAT" 2>/dev/null; then
        crash_func=$(grep "F DEBUG.*#00" "$LOGCAT" | grep -oE '\([^()]+\)' | head -1)
        STATUS["$codename"]="CRASH"
        DETAIL["$codename"]="${crash_func:-unknown}"
        ((CRASH++)) || true
    elif grep -q "Auto-finishing TEST_LOOP\|GameLoop.*Auto-finish" "$LOGCAT" 2>/dev/null; then
        STATUS["$codename"]="PASS"
        DETAIL["$codename"]="auto-finished OK"
        ((PASS++)) || true
    elif grep -q "TEST_LOOP launched\|corona.*bgfx" "$LOGCAT" 2>/dev/null; then
        STATUS["$codename"]="TIMEOUT?"
        DETAIL["$codename"]="launched but no finish"
        ((FAIL++)) || true
    else
        STATUS["$codename"]="UNKNOWN"
        DETAIL["$codename"]="no test activity"
        ((FAIL++)) || true
    fi

done < "$DISPATCH_DIR/dispatch.csv"

gcloud config set account aicoding.yee@gmail.com 2>/dev/null

echo ""
echo "=== Classic Matrix Summary ==="
printf "%-20s %-25s %-10s %s\n" "Device" "Label" "Status" "Details"
printf "%.0s-" {1..75}; echo

while IFS=',' read -r codename api matrix_id console_url bucket account project label; do
    [ "$codename" = "codename" ] && continue
    st="${STATUS[$codename]:-N/A}"
    dt="${DETAIL[$codename]:-}"
    short_label=$(echo "$label" | cut -c1-25)
    printf "%-20s %-25s %-10s %s\n" "$codename" "$short_label" "$st" "$dt"
done < "$DISPATCH_DIR/dispatch.csv"

echo ""
echo "=== Totals: PASS=$PASS  CRASH=$CRASH  FAIL=$FAIL  SKIP=$SKIP ==="
