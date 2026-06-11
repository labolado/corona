#!/usr/bin/env bash
# FTL Bench v3 — Opt A+B+C validation
# Devices: redfin/30 + bluejay/33 + CPH2449/33
# Variants: bgfx-vulkan (game-loop/5) + bgfx-gles (game-loop/5) + official (robo)
#
# Usage: bash tests/ftl_bench_v3_dispatch.sh [OUT_DIR]
# Collect: bash tests/ftl_collect_bench_v3.sh <OUT_DIR>

set -uo pipefail

BUILDS_DIR="/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds"
BGFX_VK_APK="$BUILDS_DIR/bgfx-vulkan.apk"
BGFX_GL_APK="$BUILDS_DIR/bgfx-gles.apk"
OFFICIAL_APK="$BUILDS_DIR/official-android.apk"
OUT_DIR="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/bench-builds/results-v3}"

[ -f "$BGFX_VK_APK" ]  || { echo "ERROR: $BGFX_VK_APK not found"; exit 1; }
[ -f "$BGFX_GL_APK" ]  || { echo "ERROR: $BGFX_GL_APK not found"; exit 1; }
[ -f "$OFFICIAL_APK" ] || { echo "ERROR: $OFFICIAL_APK not found"; exit 1; }
mkdir -p "$OUT_DIR"

# Returns "account|project" for a given variant-device slot
get_slot() {
    case "$1" in
        vk-redfin)   echo "hohuukieule@gmail.com|ftl-hohuu-23505" ;;
        gl-redfin)   echo "aicoding.yee@gmail.com|ftl-aicoding-23858" ;;
        off-redfin)  echo "dmeapp.usa@gmail.com|ftl-dmeapp-23893" ;;
        vk-bluejay)  echo "laboladoads@gmail.com|ftl-laboladoads-23655" ;;
        gl-bluejay)  echo "pkysoft@gmail.com|ftl-pkysoft-23615" ;;
        off-bluejay) echo "laboladopay@gmail.com|ftl-laboladopay-22264" ;;
        vk-CPH2449)  echo "countrymancostanza9@gmail.com|ftl-costanza-22188" ;;
        gl-CPH2449)  echo "khanhkhanh77960@gmail.com|ftl-khanh-22226" ;;
        off-CPH2449) echo "no5@drsmarttrade.com|ftl-no5-23698" ;;
        *) echo ""; return 1 ;;
    esac
}

dispatch_gameloop() {
    local apk="$1" variant="$2" device="$3" api="$4"
    local slot; slot=$(get_slot "${variant}-${device}")
    local acct="${slot%%|*}" proj="${slot##*|}"
    local dir="$OUT_DIR/${variant}-${device}"; mkdir -p "$dir"
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
    echo "  [$variant/$device] ${mid:-DISPATCH_FAILED} ($acct)"
}

dispatch_robo() {
    local apk="$1" device="$2" api="$3"
    local slot; slot=$(get_slot "off-${device}")
    local acct="${slot%%|*}" proj="${slot##*|}"
    local dir="$OUT_DIR/official-${device}"; mkdir -p "$dir"
    gcloud --account="$acct" --project="$proj" \
        firebase test android run \
        --type=robo \
        --app="$apk" \
        --device="model=$device,version=$api,locale=en,orientation=portrait" \
        --timeout=660s \
        --no-record-video --no-performance-metrics --async \
        > "$dir/dispatch.log" 2>&1
    echo "$?" > "$dir/dispatch.rc"
    local mid; mid=$(grep -oE 'matrix-[a-z0-9]+' "$dir/dispatch.log" 2>/dev/null | head -1 || echo "")
    echo "  [official/$device] ${mid:-DISPATCH_FAILED} ($acct)"
}

echo "=== FTL Bench v3 Dispatch ==="
echo "  bgfx-vulkan: $(du -sh "$BGFX_VK_APK" | cut -f1)  $BGFX_VK_APK"
echo "  bgfx-gles:   $(du -sh "$BGFX_GL_APK" | cut -f1)  $BGFX_GL_APK"
echo "  official:    $(du -sh "$OFFICIAL_APK" | cut -f1)  $OFFICIAL_APK"
echo "  output:      $OUT_DIR"
echo ""

PIDS=()
# redfin/30 = Pixel 5
dispatch_gameloop "$BGFX_VK_APK" "vk" "redfin" "30" & PIDS+=($!)
dispatch_gameloop "$BGFX_GL_APK" "gl" "redfin" "30" & PIDS+=($!)
dispatch_robo     "$OFFICIAL_APK" "redfin" "30"   & PIDS+=($!)
# bluejay/33 = Pixel 6a
dispatch_gameloop "$BGFX_VK_APK" "vk" "bluejay" "33" & PIDS+=($!)
dispatch_gameloop "$BGFX_GL_APK" "gl" "bluejay" "33" & PIDS+=($!)
dispatch_robo     "$OFFICIAL_APK" "bluejay" "33"   & PIDS+=($!)
# CPH2449/33 = OnePlus 11
dispatch_gameloop "$BGFX_VK_APK" "vk" "CPH2449" "33" & PIDS+=($!)
dispatch_gameloop "$BGFX_GL_APK" "gl" "CPH2449" "33" & PIDS+=($!)
dispatch_robo     "$OFFICIAL_APK" "CPH2449" "33"   & PIDS+=($!)

echo ""
echo "Waiting for ${#PIDS[@]} dispatches..."
for pid in "${PIDS[@]}"; do wait "$pid" || true; done

echo ""
echo "=== Dispatch Summary ==="
CSV="$OUT_DIR/dispatch.csv"
echo "device,variant,matrix_id,account,project" > "$CSV"

for device in redfin bluejay CPH2449; do
    for variant in vk gl official; do
        if [ "$variant" = "official" ]; then
            dir="$OUT_DIR/official-${device}"
        else
            dir="$OUT_DIR/${variant}-${device}"
        fi
        rc=$(cat "$dir/dispatch.rc" 2>/dev/null || echo "?")
        mid=$(grep -oE 'matrix-[a-z0-9]+' "$dir/dispatch.log" 2>/dev/null | head -1 || echo "")
        slot=$(get_slot "${variant/official/off}-${device}" 2>/dev/null || echo "|")
        [ "$variant" = "official" ] && slot=$(get_slot "off-${device}")
        acct="${slot%%|*}" proj="${slot##*|}"
        if [ -n "$mid" ]; then
            echo "$device,$variant,$mid,$acct,$proj" >> "$CSV"
            echo "  ✅ $device/$variant: $mid"
        else
            echo "$device,$variant,DISPATCH_FAILED,$acct,$proj" >> "$CSV"
            echo "  ❌ $device/$variant: rc=$rc"
            cat "$dir/dispatch.log" 2>/dev/null | tail -3 | sed 's/^/     /'
        fi
    done
done

echo ""
echo "Tests running on FTL (~11 min). Collect:"
echo "  bash tests/ftl_collect_bench_v3.sh $OUT_DIR"
echo "  CSV: $CSV"
