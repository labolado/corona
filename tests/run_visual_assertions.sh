#!/bin/bash
# 全量视觉断言测试：对所有视觉测试跑 GL vs bgfx 对比
# 用法: bash tests/run_visual_assertions.sh [output_dir]
#
# 每个测试：SOLAR2D_TEST=<name> bash tests/test_compare.sh <name> tests/bgfx-demo
# 有专属 assertions/<name>.txt 的用专属断言，否则用 _default.txt（最小断言兜底）
# 结果汇总到 <output_dir>/visual_report.md

set +e

cd "$(dirname "$0")/.."
OUT_DIR="${1:-/tmp/visual_assertions_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "$OUT_DIR"
REPORT="$OUT_DIR/visual_report.md"

# 视觉测试列表（排除 perf/stress/crash/minimal 类）
VISUAL_TESTS=(
  25d atlas batch batching byteorder
  capture capture_color capture_flip capture_flash
  circle_vulkan color color_capture combo_fbo composite_tiling
  container culling custom_shader custom_vs customfont
  dirty display_save drawcall
  effect_mask effect_resume
  fallback filltex font
  generator_effect
  images_real instancing
  line_quality
  mesh_dynamic
  negative_uv
  outline outline2 outline3 outline4 outline_flip
  paint_fill particles
  realworld resume_effect road road_minimal
  sdf shader_compat skybox skybox_repro sky_shader
  sprite static_geo
  texcomp tiling_verify
  unit
)

echo "# Visual Assertion Report" > "$REPORT"
echo "Generated: $(date)" >> "$REPORT"
echo "Total: ${#VISUAL_TESTS[@]} tests" >> "$REPORT"
echo "" >> "$REPORT"
echo "| Test | Result | Note |" >> "$REPORT"
echo "|------|--------|------|" >> "$REPORT"

PASS=0; FAIL=0; ERROR=0

for NAME in "${VISUAL_TESTS[@]}"; do
    echo ""
    echo "=== [$NAME] ==="
    pkill -f 'Corona Simulator' 2>/dev/null; sleep 2

    LOG="$OUT_DIR/${NAME}.log"
    SOLAR2D_TEST="$NAME" bash tests/test_compare.sh "$NAME" tests/bgfx-demo > "$LOG" 2>&1
    EXIT=$?

    if [ $EXIT -eq 0 ]; then
        RESULT="✅ PASS"
        PASS=$((PASS+1))
        NOTE=""
    elif [ $EXIT -eq 1 ]; then
        # 区分：断言失败 vs 截图/启动失败
        if grep -q '条断言失败\|FAIL:' "$LOG" 2>/dev/null; then
            FAIL_LINES=$(grep '^FAIL:' "$LOG" | head -2 | tr '\n' ' ')
            RESULT="❌ FAIL"
            NOTE="$FAIL_LINES"
        else
            RESULT="⚠️ ERROR"
            NOTE=$(tail -3 "$LOG" | tr '\n' ' ')
        fi
        FAIL=$((FAIL+1))
    else
        RESULT="⚠️ ERROR"
        NOTE=$(tail -2 "$LOG" | tr '\n' ' ')
        ERROR=$((ERROR+1))
    fi

    echo "  $RESULT"
    echo "| $NAME | $RESULT | $NOTE |" >> "$REPORT"
done

echo "" >> "$REPORT"
echo "## Summary" >> "$REPORT"
echo "- PASS: $PASS" >> "$REPORT"
echo "- FAIL: $FAIL" >> "$REPORT"
echo "- ERROR: $ERROR" >> "$REPORT"
echo "" >> "$REPORT"
echo "Logs: $OUT_DIR/" >> "$REPORT"

echo ""
echo "============================="
echo "PASS: $PASS  FAIL: $FAIL  ERROR: $ERROR"
echo "Report: $REPORT"
