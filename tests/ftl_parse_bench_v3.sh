#!/usr/bin/env bash
# Parse FTL bench v3 logcats and print comparison table.
# Usage: bash tests/ftl_parse_bench_v3.sh [results_dir]

set -uo pipefail

RESULTS="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v3}"

parse_logcat() {
    local f="$1"
    [ -s "$f" ] || { echo "NO_DATA"; return; }
    grep -E "Corona.*\[Perf\] Result" "$f" | sed 's/.*\[Perf\] Result //'
}

echo "=== FTL Bench v3 Results ==="
echo ""

for device in redfin bluejay CPH2449; do
    case "$device" in
        redfin)  label="Pixel 5 / Adreno 620" ;;
        bluejay) label="Pixel 6a / Mali G57" ;;
        CPH2449) label="OnePlus 11 / Adreno 730" ;;
    esac
    echo "### $label ($device)"

    for variant in vk gl official; do
        if [ "$variant" = "official" ]; then
            logcat="$RESULTS/official-${device}/logcat.txt"
        else
            logcat="$RESULTS/${variant}-${device}/logcat.txt"
        fi
        echo "  [$variant]"
        parse_logcat "$logcat" | grep -E "^[A-J] |^G throughput" | \
            awk '{printf "    %s\n", $0}' | head -30
    done
    echo ""
done

echo ""
echo "=== KEY METRICS COMPARISON ==="
echo ""

# Scene G throughput comparison
echo "Scene G: Create/Destroy Throughput (obj/sec)"
echo "Device        | bgfx-vk    | bgfx-gl    | official   | vk/off ratio"
echo "--------------|------------|------------|------------|-------------"
for device in redfin bluejay CPH2449; do
    case "$device" in
        redfin)  label="Pixel5/A620 " ;;
        bluejay) label="Pixel6a/G57 " ;;
        CPH2449) label="OnePlus11/A7" ;;
    esac
    vk=$(grep "G throughput" "$RESULTS/vk-${device}/logcat.txt" 2>/dev/null | \
         grep -oE '[0-9]+ obj/sec' | head -1 | grep -oE '[0-9]+' || echo "N/A")
    gl=$(grep "G throughput" "$RESULTS/gl-${device}/logcat.txt" 2>/dev/null | \
         grep -oE '[0-9]+ obj/sec' | head -1 | grep -oE '[0-9]+' || echo "N/A")
    off=$(grep "G throughput" "$RESULTS/official-${device}/logcat.txt" 2>/dev/null | \
          grep -oE '[0-9]+ obj/sec' | head -1 | grep -oE '[0-9]+' || echo "N/A")
    if [ "$vk" != "N/A" ] && [ "$off" != "N/A" ]; then
        ratio=$(awk "BEGIN{printf \"%.2f\", $vk/$off}")
    else
        ratio="N/A"
    fi
    printf "%-14s| %-10s | %-10s | %-10s | %s\n" "$label" "$vk" "$gl" "$off" "$ratio"
done

echo ""
echo "Scene A 5000: Sprite Rain FPS"
echo "Device        | bgfx-vk | bgfx-gl | official"
echo "--------------|---------|---------|----------"
for device in redfin bluejay CPH2449; do
    case "$device" in
        redfin)  label="Pixel5/A620 " ;;
        bluejay) label="Pixel6a/G57 " ;;
        CPH2449) label="OnePlus11/A7" ;;
    esac
    vk=$(grep "A 5000" "$RESULTS/vk-${device}/logcat.txt" 2>/dev/null | \
         grep -oE 'avg=[0-9.]+' | cut -d= -f2 | head -1 || echo "N/A")
    gl=$(grep "A 5000" "$RESULTS/gl-${device}/logcat.txt" 2>/dev/null | \
         grep -oE 'avg=[0-9.]+' | cut -d= -f2 | head -1 || echo "N/A")
    off=$(grep "A 5000" "$RESULTS/official-${device}/logcat.txt" 2>/dev/null | \
          grep -oE 'avg=[0-9.]+' | cut -d= -f2 | head -1 || echo "N/A")
    printf "%-14s| %-7s | %-7s | %s\n" "$label" "$vk" "$gl" "$off"
done

echo ""
echo "Scene C 1000: Effects FPS"
echo "Device        | bgfx-vk | bgfx-gl | official"
echo "--------------|---------|---------|----------"
for device in redfin CPH2449; do
    case "$device" in
        redfin)  label="Pixel5/A620 " ;;
        CPH2449) label="OnePlus11/A7" ;;
    esac
    vk=$(grep "\[Perf\] Result C 1000" "$RESULTS/vk-${device}/logcat.txt" 2>/dev/null | \
         grep -oE 'avg=[0-9.]+' | cut -d= -f2 | head -1 || echo "N/A")
    gl=$(grep "\[Perf\] Result C 1000" "$RESULTS/gl-${device}/logcat.txt" 2>/dev/null | \
         grep -oE 'avg=[0-9.]+' | cut -d= -f2 | head -1 || echo "N/A")
    off=$(grep "\[Perf\] Result C 1000" "$RESULTS/official-${device}/logcat.txt" 2>/dev/null | \
          grep -oE 'avg=[0-9.]+' | cut -d= -f2 | head -1 || echo "N/A")
    printf "%-14s| %-7s | %-7s | %s\n" "$label" "$vk" "$gl" "$off"
done
