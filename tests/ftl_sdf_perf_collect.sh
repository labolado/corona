#!/usr/bin/env bash
# FTL SDF perf collect + parse
# Downloads logcats from GCS and extracts SDF_PERF markers.
#
# Usage: bash tests/ftl_sdf_perf_collect.sh [OUT_DIR]
set -uo pipefail

OUT_DIR="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/sdf-perf}"
[ -d "$OUT_DIR" ] || { echo "ERROR: $OUT_DIR not found"; exit 1; }

get_slot() {
    case "$1" in
        vk-redfin)  echo "ftl-sa@ftl-hohuu-23505.iam.gserviceaccount.com|ftl-hohuu-23505" ;;
        vk-bluejay) echo "hohuukieule@gmail.com|ftl-khanh-22226" ;;
        gl-b0q)     echo "hohuukieule@gmail.com|ftl-khanh-22226" ;;
    esac
}

SLOTS=("vk-redfin" "vk-bluejay" "gl-b0q")

# ── Step 1: Download logcats ───────────────────────────────────────────────
echo "=== Collecting logcats ==="
for key in "${SLOTS[@]}"; do
    b="${key%%-*}" d="${key##*-}"
    dir="$OUT_DIR/${b}-${d}"
    logcat="$dir/logcat.txt"

    [ -s "$logcat" ] && { echo "  HAVE ${b}/${d}"; continue; }
    [ -f "$dir/dispatch.log" ] || { echo "  SKIP ${b}/${d} (no dispatch.log)"; continue; }

    slot=$(get_slot "$key"); acct="${slot%%|*}" proj="${slot##*|}"
    gs=$(grep -oE 'storage/browser/[^ ]+' "$dir/dispatch.log" 2>/dev/null | head -1 \
         | sed -E 's|.*storage/browser/|gs://|; s|[]/]+$||')
    [ -n "$gs" ] || { echo "  ⚠️  ${b}/${d}: no GCS bucket in dispatch.log"; continue; }

    echo "  Fetching ${b}/${d} ..."
    src=$(gcloud storage ls --account="$acct" --project="$proj" "$gs/**" 2>/dev/null \
          | grep '/logcat$' | head -1)
    if [ -n "$src" ]; then
        gcloud storage cp --account="$acct" --project="$proj" "$src" "$logcat" >/dev/null 2>&1 || true
    fi
    [ -s "$logcat" ] && echo "    ✅ got logcat ($(wc -l < "$logcat") lines)" \
                     || echo "    ⚠️  logcat not available yet — retry later"
done

# ── Step 2: Parse SDF_PERF markers ────────────────────────────────────────
echo ""
echo "=== SDF Performance Results ==="
printf "%-20s %-6s %-8s %-8s %-8s %-8s %-8s\n" \
    "device/backend" "mode" "fps" "draws" "tris" "speedup" "tri_red"
echo "$(printf '%.0s-' {1..80})"

RESULT_FILE="$OUT_DIR/sdf_perf_results.csv"
echo "backend,device,mode,fps,draws,tris,speedup,tri_reduction" > "$RESULT_FILE"

for key in "${SLOTS[@]}"; do
    b="${key%%-*}" d="${key##*-}"
    logcat="$OUT_DIR/${b}-${d}/logcat.txt"
    [ -s "$logcat" ] || { printf "%-20s  (no logcat)\n" "${b}/${d}"; continue; }

    # Parse OFF and ON lines
    off_line=$(grep "SDF_PERF|OFF|" "$logcat" | tail -1)
    on_line=$(grep  "SDF_PERF|ON|"  "$logcat" | tail -1)
    su_line=$(grep  "SDF_PERF|SPEEDUP|" "$logcat" | tail -1)

    parse_field() { echo "$1" | grep -oE "$2=[^|]+" | cut -d= -f2; }

    off_fps=$(parse_field "$off_line" "fps"); off_fps=${off_fps:-"?"}
    off_dr=$(parse_field  "$off_line" "draws"); off_dr=${off_dr:-"?"}
    off_tri=$(parse_field "$off_line" "tris"); off_tri=${off_tri:-"?"}

    on_fps=$(parse_field  "$on_line" "fps"); on_fps=${on_fps:-"?"}
    on_dr=$(parse_field   "$on_line" "draws"); on_dr=${on_dr:-"?"}
    on_tri=$(parse_field  "$on_line" "tris"); on_tri=${on_tri:-"?"}

    speedup=$(echo "$su_line" | grep -oE 'SPEEDUP\|[0-9.]+' | cut -d'|' -f2 || echo "?")
    tri_red=$(echo "$su_line" | grep -oE 'tri_reduction=[0-9.]+' | cut -d= -f2 || echo "?")

    printf "%-20s %-6s %-8s %-8s %-8s\n" "${b}/${d}" "OFF" "$off_fps" "$off_dr" "$off_tri"
    printf "%-20s %-6s %-8s %-8s %-8s %-8s %-8s\n" "" "ON" "$on_fps" "$on_dr" "$on_tri" "${speedup}x" "${tri_red}x"
    echo ""

    echo "$b,$d,OFF,$off_fps,$off_dr,$off_tri,," >> "$RESULT_FILE"
    echo "$b,$d,ON,$on_fps,$on_dr,$on_tri,$speedup,$tri_red" >> "$RESULT_FILE"
done

echo "$(printf '%.0s-' {1..80})"
echo ""
echo "CSV: $RESULT_FILE"

# ── Step 3: Raw marker dump (for debugging) ───────────────────────────────
echo ""
echo "=== Raw SDF_PERF markers ==="
for key in "${SLOTS[@]}"; do
    b="${key%%-*}" d="${key##*-}"
    logcat="$OUT_DIR/${b}-${d}/logcat.txt"
    [ -s "$logcat" ] || continue
    echo "--- ${b}/${d} ---"
    grep "SDF_PERF|" "$logcat" | sed 's/^.*SDF_PERF/SDF_PERF/'
    echo ""
done
