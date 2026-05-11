#!/usr/bin/env bash
# FTL Performance Benchmark — bgfx fork vs 官方 SDK，5 设备并行 dispatch
# bench 由 solar2d_test.txt="perf" 触发（无需 GameLoop scenario 2）
#
# 用法: bash tests/ftl_bench_matrix.sh <bgfx_apk> <official_apk> [OUT_DIR]
# 收集: bash tests/ftl_collect_bench.sh <OUT_DIR>

set -uo pipefail

BGFX_APK="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/w-bench-build/bgfx-bench.apk}"
OFFICIAL_APK="${2:-/Users/yee/data/dev/app/labo/game_engine/tmp/w-bench-build/official-bench.apk}"
OUT_DIR="${3:-/Users/yee/data/dev/app/labo/game_engine/tmp/ftl-bench-$(date +%Y%m%d-%H%M)}"

[ -f "$BGFX_APK" ]    || { echo "ERROR: bgfx APK not found: $BGFX_APK"; exit 1; }
[ -f "$OFFICIAL_APK" ] || { echo "ERROR: official APK not found: $OFFICIAL_APK"; exit 1; }
mkdir -p "$OUT_DIR"

# device | api | bgfx_acct | bgfx_proj | official_acct | official_proj | label
MATRIX=$(cat <<'EOF'
oriole|33|hohuukieule@gmail.com|ftl-hohuu-23505|aicoding.yee@gmail.com|ftl-aicoding-23858|Pixel 6 / Tensor G1 / Mali-G78
b0q|33|dmeapp.usa@gmail.com|ftl-dmeapp-23893|khanhkhanh77960@gmail.com|ftl-khanh-22226|Galaxy S22 Ultra / SD 8 Gen 1 / Adreno 730
redfin|30|laboladoads@gmail.com|ftl-laboladoads-23655|pkysoft@gmail.com|ftl-pkysoft-23615|Pixel 5 / SD 765G / Adreno 620
a54x|34|laboladopay@gmail.com|ftl-laboladopay-22264|no5@drsmarttrade.com|ftl-no5-23698|Galaxy A54 5G / Exynos 1380 / Mali-G68
OP5552L1|34|countrymancostanza9@gmail.com|ftl-costanza-22188|yuyong@labolado.com|project-5dce9ec5-f92e-46af-ae9|OnePlus 10T / SD 8+ Gen 1 / Adreno 730
EOF
)

dispatch_bgfx() {
    local device="$1" api="$2" acct="$3" proj="$4" label="$7"
    local dir="$OUT_DIR/${device}-bgfx"; mkdir -p "$dir"
    gcloud --account="$acct" --project="$proj" \
        firebase test android run \
        --type=game-loop --scenario-numbers=1 \
        --app="$BGFX_APK" \
        --device="model=$device,version=$api,locale=en,orientation=portrait" \
        --timeout=960s \
        --no-record-video --no-performance-metrics --async \
        > "$dir/dispatch.log" 2>&1
    echo "$?" > "$dir/dispatch.rc"
}

dispatch_official() {
    local device="$1" api="$2" acct="$5" proj="$6" label="$7"
    local dir="$OUT_DIR/${device}-official"; mkdir -p "$dir"
    gcloud --account="$acct" --project="$proj" \
        firebase test android run \
        --type=robo \
        --app="$OFFICIAL_APK" \
        --device="model=$device,version=$api,locale=en,orientation=portrait" \
        --timeout=900s \
        --no-record-video --no-performance-metrics --async \
        > "$dir/dispatch.log" 2>&1
    echo "$?" > "$dir/dispatch.rc"
}

echo "=== FTL Bench Matrix ==="
echo "bgfx APK:     $(du -sh "$BGFX_APK" | cut -f1)  $BGFX_APK"
echo "official APK: $(du -sh "$OFFICIAL_APK" | cut -f1)  $OFFICIAL_APK"
echo "output:       $OUT_DIR"
echo ""

PIDS=()
while IFS='|' read -r device api b_acct b_proj o_acct o_proj label; do
    [ -z "$device" ] && continue
    echo "  dispatching $device: bgfx[$b_acct] + official[$o_acct]"
    dispatch_bgfx    "$device" "$api" "$b_acct" "$b_proj" "$o_acct" "$o_proj" "$label" &
    PIDS+=($!)
    dispatch_official "$device" "$api" "$b_acct" "$b_proj" "$o_acct" "$o_proj" "$label" &
    PIDS+=($!)
done <<< "$MATRIX"

echo ""
echo "Waiting for ${#PIDS[@]} dispatches..."
for pid in "${PIDS[@]}"; do wait "$pid" || true; done

echo ""
echo "=== Dispatch Summary ==="
CSV="$OUT_DIR/dispatch.csv"
echo "device,apk,matrix_id,console,account,project" > "$CSV"

while IFS='|' read -r device api b_acct b_proj o_acct o_proj label; do
    [ -z "$device" ] && continue
    for variant in bgfx official; do
        LOG="$OUT_DIR/${device}-${variant}/dispatch.log"
        rc=$(cat "$OUT_DIR/${device}-${variant}/dispatch.rc" 2>/dev/null || echo "?")
        matrix_id=$(grep -oE 'matrix-[a-z0-9]+' "$LOG" 2>/dev/null | head -1)
        console=$(grep -oE 'https://console\.firebase[^ ]+' "$LOG" 2>/dev/null | head -1)
        acct=$([[ "$variant" = "bgfx" ]] && echo "$b_acct" || echo "$o_acct")
        proj=$([[ "$variant" = "bgfx" ]] && echo "$b_proj" || echo "$o_proj")
        if [ "$rc" = "0" ] && [ -n "$matrix_id" ]; then
            echo "$device,$variant,$matrix_id,$console,$acct,$proj" >> "$CSV"
            echo "  ✅ $device-$variant: $matrix_id"
        else
            echo "$device,$variant,FAILED_rc=$rc,,$acct,$proj" >> "$CSV"
            echo "  ❌ $device-$variant: rc=$rc"
        fi
    done
done <<< "$MATRIX"

echo ""
echo "Tests running on FTL (~15 min). Collect results:"
echo "  bash tests/ftl_collect_bench.sh $OUT_DIR"
