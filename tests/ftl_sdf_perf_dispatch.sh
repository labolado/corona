#!/usr/bin/env bash
# FTL SDF perf dispatch — scenario 6 (sdf_perf)
# Builds fresh bgfx APKs (Vulkan + GLES) and dispatches to 3 devices.
# Each run: SDF OFF(10s) → SDF ON(10s) in sequence; logcat has SDF_PERF markers.
#
# Usage:  bash tests/ftl_sdf_perf_dispatch.sh [OUT_DIR]
# Collect: bash tests/ftl_sdf_perf_collect.sh [OUT_DIR]
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORONA_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

OUT_DIR="${1:-/Users/yee/data/dev/app/labo/game_engine/tmp/sdf-perf}"
BUILDS_DIR="$OUT_DIR/apks"
mkdir -p "$BUILDS_DIR"

log() { echo "[sdf-perf] $*"; }
fail() { echo "[FAIL] $*" >&2; exit 1; }

# ── Accounts (same as bench v4) ───────────────────────────────────────────
get_slot() {
    case "$1" in
        # ftl-hohuu-sa: SA for ftl-hohuu-23505 (OK)
        vk-redfin)  echo "ftl-sa@ftl-hohuu-23505.iam.gserviceaccount.com|ftl-hohuu-23505" ;;
        # khanh: hohuukieule@gmail.com / ftl-khanh-22226 (OK)
        vk-bluejay) echo "hohuukieule@gmail.com|ftl-khanh-22226" ;;
        gl-b0q)     echo "hohuukieule@gmail.com|ftl-khanh-22226" ;;
        *) fail "unknown slot: $1" ;;
    esac
}
get_api() { case "$1" in redfin) echo 30;; bluejay) echo 32;; b0q) echo 33;; esac }

BGFX_CB="/Applications/Corona-b3/Native/Corona/mac/bin/CoronaBuilder.app/Contents/MacOS/CoronaBuilder"
BGFX_DEMO="$CORONA_DIR/tests/bgfx-demo"

# ── Step 1: Build APKs ────────────────────────────────────────────────────
log "Building bgfx-vulkan.apk (build_android.sh) ..."
MARKER=$(mktemp)

bash "$SCRIPT_DIR/build_android.sh" "$BGFX_DEMO" com.labolado.bgfxdemo \
    >> "$OUT_DIR/build.log" 2>&1 || fail "build_android.sh vulkan failed"

APK_VK=$(find /tmp -name "bgfxdemo.apk" -newer "$MARKER" 2>/dev/null | head -1 \
         || find /tmp -name "*.apk" -newer "$MARKER" 2>/dev/null | head -1)
[ -n "$APK_VK" ] || fail "no vulkan APK produced"
cp "$APK_VK" "$BUILDS_DIR/bgfx-vulkan.apk"
log "  vulkan: $(du -sh "$BUILDS_DIR/bgfx-vulkan.apk" | cut -f1)"

log "Building bgfx-gles.apk (force_gles flag) ..."
GLES_TMP=$(mktemp -d "/tmp/bgfx-demo-gles-XXXX")
cp -r "$BGFX_DEMO/." "$GLES_TMP/"
touch "$GLES_TMP/solar2d_force_gles.txt"

cat > /tmp/sdf-perf-build-$$.lua << LUAEOF
local params = {
    platform = 'android',
    appName = 'bgfxdemo-gles',
    appVersion = '1.0',
    dstPath = '$(mktemp -d /tmp/sdf-perf-gles-XXXX)',
    projectPath = '$GLES_TMP',
    androidAppPackage = 'com.labolado.bgfxdemo.gles',
    keystorePath = '$HOME/.android/debug.keystore',
    keystorePassword = 'android',
    keystoreAlias = 'androiddebugkey',
    keystoreAliasPassword = 'android',
}
return params
LUAEOF

"$BGFX_CB" build --lua /tmp/sdf-perf-build-$$.lua \
    >> "$OUT_DIR/build.log" 2>&1 || fail "CoronaBuilder gles failed"
rm -f /tmp/sdf-perf-build-$$.lua
rm -rf "$GLES_TMP"

APK_GL=$(find /tmp -name "*.apk" -newer "$MARKER" 2>/dev/null \
         | grep -v bgfxdemo.apk | head -1)
[ -n "$APK_GL" ] || fail "no gles APK produced"
cp "$APK_GL" "$BUILDS_DIR/bgfx-gles.apk"
log "  gles: $(du -sh "$BUILDS_DIR/bgfx-gles.apk" | cut -f1)"

rm -f "$MARKER"

# ── Step 2: Dispatch ──────────────────────────────────────────────────────
# 3 slots: vk/redfin(api30, hohuu-sa), vk/bluejay(api32, khanh), gl/b0q(api33 Adreno730, khanh)
DISPATCHES=(
    "vk redfin $BUILDS_DIR/bgfx-vulkan.apk"
    "vk bluejay $BUILDS_DIR/bgfx-vulkan.apk"
    "gl b0q $BUILDS_DIR/bgfx-gles.apk"
)

log ""
log "=== Dispatching scenario 6 (sdf_perf) to 3 devices ==="

dispatch_one() {
    local backend="$1" device="$2" apk="$3"
    local api; api=$(get_api "$device")
    local slot; slot=$(get_slot "${backend}-${device}")
    local acct="${slot%%|*}" proj="${slot##*|}"
    local dir="$OUT_DIR/${backend}-${device}"; mkdir -p "$dir"

    gcloud --account="$acct" --project="$proj" \
        firebase test android run \
        --type=game-loop \
        --scenario-numbers=6 \
        --app="$apk" \
        --device="model=$device,version=$api,locale=en,orientation=portrait" \
        --timeout=120s \
        --no-record-video --no-performance-metrics --async \
        > "$dir/dispatch.log" 2>&1
    local rc=$?
    echo "$rc" > "$dir/dispatch.rc"
    local mid; mid=$(grep -oE 'matrix-[a-z0-9]+' "$dir/dispatch.log" 2>/dev/null | head -1 || echo "")
    if [ -n "$mid" ]; then
        log "  ✅ ${backend}/${device} api${api}: $mid  ($acct)"
    else
        log "  ❌ ${backend}/${device} FAILED (rc=$rc)"
        tail -3 "$dir/dispatch.log" | sed 's/^/     /'
    fi
}

PIDS=()
for entry in "${DISPATCHES[@]}"; do
    read -r b d apk <<< "$entry"
    dispatch_one "$b" "$d" "$apk" &
    PIDS+=($!)
done
for pid in "${PIDS[@]}"; do wait "$pid" || true; done

# ── Summary CSV ───────────────────────────────────────────────────────────
CSV="$OUT_DIR/dispatch.csv"
echo "backend,device,api,matrix_id,account,project" > "$CSV"
for entry in "${DISPATCHES[@]}"; do
    read -r b d _ <<< "$entry"
    api=$(get_api "$d")
    slot=$(get_slot "${b}-${d}"); acct="${slot%%|*}" proj="${slot##*|}"
    dir="$OUT_DIR/${b}-${d}"
    mid=$(grep -oE 'matrix-[a-z0-9]+' "$dir/dispatch.log" 2>/dev/null | head -1 || echo "")
    echo "$b,$d,$api,${mid:-FAILED},$acct,$proj" >> "$CSV"
done

log ""
log "Tests running (~2 min). Collect when done:"
log "  bash tests/ftl_sdf_perf_collect.sh $OUT_DIR"
log "  CSV: $CSV"
