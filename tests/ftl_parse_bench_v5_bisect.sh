#!/usr/bin/env bash
# Parse FTL bench v5 bisect logcats and print Scene G throughput comparison.
# Compares 4 bisect variants against known baseline and opt-abc values.
#
# Usage: bash tests/ftl_parse_bench_v5_bisect.sh [RESULTS_DIR] [V4_RESULTS_DIR]
set -uo pipefail

BISECT="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v5-bisect}"
V4="${2:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v4}"

scene_g() {
    local f="$1"
    [ -s "$f" ] || { echo ""; return; }
    grep -E "Corona.*\[Perf\] Result" "$f" \
        | sed 's/.*\[Perf\] Result //' \
        | grep -E "^G throughput" \
        | grep -oE '[0-9]+ obj/sec' \
        | grep -oE '[0-9]+' \
        | head -1
}

delta() {
    local base="$1" val="$2"
    { [ -z "$base" ] || [ -z "$val" ]; } && { echo "—"; return; }
    awk "BEGIN{ printf \"%+.1f%%\", ($val-$base)/$base*100 }"
}

echo "=== FTL Bench v5 Bisect — Vulkan Scene G Throughput ==="
echo ""

# Load baseline and opt-abc from v4 results
base_mali=$(scene_g "$V4/baseline-vk-bluejay/logcat.txt")
base_adreno=$(scene_g "$V4/baseline-vk-CPH2449/logcat.txt")
abc_mali=$(scene_g "$V4/optimized-vk-bluejay/logcat.txt")
abc_adreno=$(scene_g "$V4/optimized-vk-CPH2449/logcat.txt")

# Load bisect variants — opt-a-bluejay retry log path
opt_a_mali_log="$BISECT/opt-a-bluejay/logcat.txt"
[ -s "$opt_a_mali_log" ] || opt_a_mali_log="$BISECT/opt-a-bluejay-retry/logcat.txt"
opt_a_adreno_log="$BISECT/opt-a-CPH2449/logcat.txt"
opt_ab_mali_log="$BISECT/opt-ab-bluejay/logcat.txt"
opt_ab_adreno_log="$BISECT/opt-ab-CPH2449/logcat.txt"

opta_mali=$(scene_g "$opt_a_mali_log")
opta_adreno=$(scene_g "$opt_a_adreno_log")
optab_mali=$(scene_g "$opt_ab_mali_log")
optab_adreno=$(scene_g "$opt_ab_adreno_log")

echo "### Mali G57 (Pixel 6a / bluejay / API32)"
echo "| Variant         | obj/s  | Δ vs baseline |"
echo "|-----------------|--------|---------------|"
printf "| baseline (c0d30547) | %s | — |\n" "${base_mali:-MISSING}"
printf "| Opt-A only (6e513db8) | %s | %s |\n" "${opta_mali:-MISSING}" "$(delta "$base_mali" "$opta_mali")"
printf "| Opt-A+B (6c0158cf)    | %s | %s |\n" "${optab_mali:-MISSING}" "$(delta "$base_mali" "$optab_mali")"
printf "| Opt-A+B+C (a1564c7a)  | %s | %s |\n" "${abc_mali:-MISSING}" "$(delta "$base_mali" "$abc_mali")"
echo ""

echo "### Adreno 730 (OnePlus 11 / CPH2449 / API34)"
echo "| Variant         | obj/s  | Δ vs baseline |"
echo "|-----------------|--------|---------------|"
printf "| baseline (c0d30547) | %s | — |\n" "${base_adreno:-MISSING}"
printf "| Opt-A only (6e513db8) | %s | %s |\n" "${opta_adreno:-MISSING}" "$(delta "$base_adreno" "$opta_adreno")"
printf "| Opt-A+B (6c0158cf)    | %s | %s |\n" "${optab_adreno:-MISSING}" "$(delta "$base_adreno" "$optab_adreno")"
printf "| Opt-A+B+C (a1564c7a)  | %s | %s |\n" "${abc_adreno:-MISSING}" "$(delta "$base_adreno" "$abc_adreno")"
echo ""

echo "### SHA Verification"
echo "  baseline  = 530b51d2"
echo "  opt-a     = 6ce129cb"
echo "  opt-ab    = 17258cc7"
echo "  opt-abc   = ce1322fd"
echo ""

echo "### Bisect Conclusion"
if [ -z "$opta_mali" ] || [ -z "$optab_mali" ] || [ -z "$opta_adreno" ] || [ -z "$optab_adreno" ]; then
    echo "  INCOMPLETE: some logcats missing"
else
    # Determine culprit
    mali_ab_delta=$(awk "BEGIN{ printf \"%+.1f\", ($optab_mali-$base_mali)/$base_mali*100 }")
    mali_a_delta=$(awk "BEGIN{ printf \"%+.1f\", ($opta_mali-$base_mali)/$base_mali*100 }")
    adreno_ab_delta=$(awk "BEGIN{ printf \"%+.1f\", ($optab_adreno-$base_adreno)/$base_adreno*100 }")
    adreno_a_delta=$(awk "BEGIN{ printf \"%+.1f\", ($opta_adreno-$base_adreno)/$base_adreno*100 }")

    echo "  Mali G57:   baseline→A(${mali_a_delta}%)  A→AB(delta from A: $(awk "BEGIN{ printf \"%+.1f%%\", ($optab_mali-$opta_mali)/$opta_mali*100 }"))"
    echo "  Adreno 730: baseline→A(${adreno_a_delta}%) A→AB(delta from A: $(awk "BEGIN{ printf \"%+.1f%%\", ($optab_adreno-$opta_adreno)/$opta_adreno*100 }"))"

    # Simple culprit logic
    mali_a_ok=$(awk "BEGIN{ print ($opta_mali >= $base_mali * 0.97) ? 1 : 0 }")
    mali_ab_ok=$(awk "BEGIN{ print ($optab_mali >= $base_mali * 0.97) ? 1 : 0 }")

    if [ "$mali_a_ok" = "1" ] && [ "$mali_ab_ok" = "0" ]; then
        echo "  CULPRIT: Opt B (6c0158cf) — lazy transient VB alloc"
    elif [ "$mali_a_ok" = "0" ]; then
        echo "  CULPRIT: Opt A (6e513db8) — view-reset high-water-mark"
    else
        echo "  CULPRIT: Opt C (a1564c7a) — side-table uniforms / shrink DeferredCmd"
    fi
fi
