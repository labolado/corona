#!/usr/bin/env bash
# FTL Bench v4 — pair baseline(before) vs optimized(after) from results-v4/, per
# device/backend/scene, compute Δ%.  Both variants live in the same OUT_DIR:
#   results-v4/<variant>-<backend>-<device>/logcat.txt
#
# Usage: bash tests/ftl_compare_v4.sh [RESULTS_DIR]
set -uo pipefail

RES="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v4}"

perf() { grep -E "Corona.*\[Perf\] Result" "$1" 2>/dev/null | sed 's/.*\[Perf\] Result //'; }
val() { # logcat key   (key "G" -> obj/sec ; else avg for "Scene count")
    local f="$1" key="$2"
    if [ "$key" = "G" ]; then
        perf "$f" | grep -E "^G throughput" | grep -oE '[0-9]+ obj/sec' | grep -oE '[0-9]+' | head -1
    else
        perf "$f" | grep -E "^${key}: " | grep -oE 'avg=[0-9.]+' | cut -d= -f2 | head -1
    fi
}
delta() { local b="$1" a="$2"; { [ -z "$b" ] || [ -z "$a" ]; } && { echo "—"; return; }
    awk "BEGIN{ if($b==0){print \"—\"} else {printf \"%+.1f%%\", ($a-$b)/$b*100} }"; }
label_for() { case "$1" in
    redfin)  echo "Pixel 5 / Adreno 620 (API30)";;
    bluejay) echo "Pixel 6a / Mali G57 (API32)";;
    CPH2449) echo "OnePlus 11 / Adreno 730 (API34)";; esac; }

KEYS=("A 1000" "A 2000" "A 5000" "B 500" "B 1000" "B 2000" "C 500" "C 1000" "C 2000" "G")

for backend in vk gl; do
    echo "## Backend: bgfx-$([ $backend = vk ] && echo vulkan || echo gles)"
    echo ""
    for device in redfin bluejay CPH2449; do
        bf="$RES/baseline-${backend}-${device}/logcat.txt"
        af="$RES/optimized-${backend}-${device}/logcat.txt"
        echo "### $(label_for "$device")"
        [ -s "$bf" ] || { echo "_baseline logcat missing_"; echo; continue; }
        [ -s "$af" ] || { echo "_optimized logcat missing_"; echo; continue; }
        echo "| Scene | before (baseline) | after (opt) | Δ% |"
        echo "|-------|--------|-------|-----|"
        for k in "${KEYS[@]}"; do
            b=$(val "$bf" "$k"); a=$(val "$af" "$k")
            { [ -z "$b" ] && [ -z "$a" ]; } && continue
            if [ "$k" = "G" ]; then disp="G throughput"; unit=" obj/s"; else disp="$k fps"; unit=""; fi
            printf "| %s | %s%s | %s%s | %s |\n" "$disp" "${b:-—}" "$unit" "${a:-—}" "$unit" "$(delta "$b" "$a")"
        done
        echo ""
    done
done
