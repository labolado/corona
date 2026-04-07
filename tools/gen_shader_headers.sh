#!/bin/bash
##############################################################################
# Generate bgfx shader C headers for all platforms
# Usage:
#   bash tools/gen_shader_headers.sh metal   # Metal headers (iOS/macOS)
#   bash tools/gen_shader_headers.sh essl    # ESSL headers (Android)
#   bash tools/gen_shader_headers.sh all     # Both
##############################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BGFX_DIR="${PROJECT_ROOT}/external/bgfx"
SHADERC="${BGFX_DIR}/tools/bin/darwin/shaderc"
BGFX_INCLUDE="${BGFX_DIR}/src"

EFFECT_DIR="${PROJECT_ROOT}/librtt/Display/Shader/bgfx"
SDF_DIR="${PROJECT_ROOT}/librtt/Renderer/shaders/sdf"
INSTANCED_DIR="${PROJECT_ROOT}/librtt/Renderer/shaders/instanced"
OUTPUT_DIR="${PROJECT_ROOT}/librtt/Renderer"
COMPILED_DIR="${EFFECT_DIR}/compiled"

# Parse args
TARGET="${1:-all}"
FAIL_LOG="/tmp/shader_compile_failures_$(date +%s).log"
> "$FAIL_LOG"

get_profile() {
    local platform=$1 type=$2
    case "$platform" in
        metal) echo "metal metal" ;;
        essl)  echo "android 300_es" ;;
        glsl)  echo "linux 120" ;;
        spirv) echo "vulkan spirv" ;;
    esac
}

compile_one() {
    local sc_file=$1 out_bin=$2 type=$3 platform=$4 include_dir=$5 defines=$6
    local stage="fragment"
    [[ "$type" == "vs" ]] && stage="vertex"

    local params=$(get_profile "$platform" "$type")
    local plat_arg=$(echo "$params" | cut -d' ' -f1)
    local profile=$(echo "$params" | cut -d' ' -f2)
    local varying_dir=$(dirname "$sc_file")
    local varying="${varying_dir}/varying.def.sc"

    local cmd="$SHADERC --type $stage --platform $plat_arg --profile $profile"
    cmd="$cmd -f $sc_file -o $out_bin --varyingdef $varying -i $BGFX_INCLUDE"
    [[ -n "$include_dir" ]] && cmd="$cmd -i $include_dir"
    [[ -n "$defines" ]] && cmd="$cmd $defines"

    if eval "$cmd" 2>/dev/null; then
        return 0
    else
        echo "FAILED: $sc_file → $out_bin" >> "$FAIL_LOG"
        return 1
    fi
}

bin_to_c_array() {
    local bin_file=$1 var_name=$2
    local size=$(wc -c < "$bin_file" | tr -d ' ')
    echo "static const unsigned char ${var_name}[] = {"
    xxd -i < "$bin_file" | sed 's/^/  /'
    echo "};"
    echo "static const unsigned int ${var_name}_size = ${size};"
    echo ""
}

compile_effects() {
    local platform=$1
    local out_dir="${COMPILED_DIR}/${platform}"
    mkdir -p "$out_dir"

    local count=0 ok=0
    echo "--- Compiling effects for ${platform} ---"

    for sc_file in "${EFFECT_DIR}"/fs_*.sc "${EFFECT_DIR}"/vs_*.sc; do
        [[ ! -f "$sc_file" ]] && continue
        local base=$(basename "$sc_file" .sc)
        local type="${base%%_*}"  # vs or fs
        local out_bin="${out_dir}/${base}.bin"
        count=$((count + 1))

        if compile_one "$sc_file" "$out_bin" "$type" "$platform" "$EFFECT_DIR"; then
            ok=$((ok + 1))
        fi
    done

    echo "Effects ${platform}: ${ok}/${count} compiled"
}

compile_sdf() {
    local platform=$1
    local out_dir="${COMPILED_DIR}/${platform}/sdf"
    mkdir -p "$out_dir"

    local count=0 ok=0
    echo "--- Compiling SDF shaders for ${platform} ---"

    for sc_file in "${SDF_DIR}"/fs_sdf_*.sc "${SDF_DIR}"/vs_sdf.sc; do
        [[ ! -f "$sc_file" ]] && continue
        local base=$(basename "$sc_file" .sc)
        local type="${base%%_*}"
        local out_bin="${out_dir}/${base}.bin"
        count=$((count + 1))

        if compile_one "$sc_file" "$out_bin" "$type" "$platform" "$SDF_DIR"; then
            ok=$((ok + 1))
        fi
    done

    echo "SDF ${platform}: ${ok}/${count} compiled"
}

compile_instanced() {
    local platform=$1
    local out_dir="${COMPILED_DIR}/${platform}/instanced"
    mkdir -p "$out_dir"

    local count=0 ok=0
    echo "--- Compiling instanced shaders for ${platform} ---"

    for sc_file in "${INSTANCED_DIR}"/fs_batch_*.sc "${INSTANCED_DIR}"/vs_batch_*.sc; do
        [[ ! -f "$sc_file" ]] && continue
        local base=$(basename "$sc_file" .sc)
        local type="${base%%_*}"
        local out_bin="${out_dir}/${base}.bin"
        count=$((count + 1))

        if compile_one "$sc_file" "$out_bin" "$type" "$platform" "$INSTANCED_DIR"; then
            ok=$((ok + 1))
        fi
    done

    echo "Instanced ${platform}: ${ok}/${count} compiled"
}

compile_default() {
    local platform=$1
    local out_dir="${COMPILED_DIR}/${platform}"
    mkdir -p "$out_dir"

    echo "--- Compiling default shaders for ${platform} ---"
    local ok=0

    # Default vertex + fragment (no masks, no wireframe — just the base)
    for type in vs fs; do
        local sc_file="${EFFECT_DIR}/${type}_default.sc"
        local out_bin="${out_dir}/${type}_default.bin"
        if compile_one "$sc_file" "$out_bin" "$type" "$platform" "$EFFECT_DIR"; then
            ok=$((ok + 1))
        fi
    done

    echo "Default ${platform}: ${ok}/2 compiled"
}

gen_header() {
    local platform=$1 category=$2 bin_dir=$3 header_file=$4
    local count=0

    echo "// Auto-generated: ${category} ${platform} shader data for bgfx" > "$header_file"
    echo "// Generated: $(date '+%Y-%m-%d %H:%M')" >> "$header_file"
    echo "" >> "$header_file"

    for bin_file in "${bin_dir}"/*.bin; do
        [[ ! -f "$bin_file" ]] && continue
        local base=$(basename "$bin_file" .bin)
        local var_name="s_${base}_${platform}"
        bin_to_c_array "$bin_file" "$var_name" >> "$header_file"
        count=$((count + 1))
    done

    echo "Generated ${header_file##*/}: ${count} shaders"
}

gen_effects_header() {
    local platform=$1
    local bin_dir="${COMPILED_DIR}/${platform}"
    local header="${OUTPUT_DIR}/Rtt_BgfxShaderData_effects_${platform}.h"
    local count=0

    echo "// Auto-generated: all ${platform} shader data for bgfx effects" > "$header"
    echo "// Generated: $(date '+%Y-%m-%d %H:%M')" >> "$header"
    echo "" >> "$header"

    # Shader binary arrays
    for bin_file in "${bin_dir}"/fs_composite_*.bin "${bin_dir}"/fs_filter_*.bin \
                    "${bin_dir}"/fs_generator_*.bin "${bin_dir}"/vs_filter_*.bin; do
        [[ ! -f "$bin_file" ]] && continue
        local base=$(basename "$bin_file" .bin)
        local var_name="s_${base}_${platform}"
        bin_to_c_array "$bin_file" "$var_name" >> "$header"
        count=$((count + 1))
    done

    # Lookup table
    echo "struct BgfxShaderEntry { const char* filename; const unsigned char* data; unsigned int size; };" >> "$header"
    echo "" >> "$header"
    echo "static const BgfxShaderEntry s_bgfxShaderTable[] = {" >> "$header"

    for bin_file in "${bin_dir}"/fs_composite_*.bin "${bin_dir}"/fs_filter_*.bin \
                    "${bin_dir}"/fs_generator_*.bin "${bin_dir}"/vs_filter_*.bin; do
        [[ ! -f "$bin_file" ]] && continue
        local base=$(basename "$bin_file" .bin)
        local var_name="s_${base}_${platform}"
        echo "    { \"${base}.bin\", ${var_name}, ${var_name}_size }," >> "$header"
    done

    echo "};" >> "$header"
    echo "static const int s_bgfxShaderTableCount = ${count};" >> "$header"

    echo "Generated effects header: ${count} shaders → ${header##*/}"
}

gen_default_header() {
    local platform=$1
    local bin_dir="${COMPILED_DIR}/${platform}"
    local header="${OUTPUT_DIR}/Rtt_BgfxShaderData_${platform}.h"

    echo "// Auto-generated: default ${platform} shader data" > "$header"
    echo "// Generated: $(date '+%Y-%m-%d %H:%M')" >> "$header"
    echo "" >> "$header"

    for bin_file in "${bin_dir}/vs_default.bin" "${bin_dir}/fs_default.bin"; do
        [[ ! -f "$bin_file" ]] && continue
        local base=$(basename "$bin_file" .bin)
        local var_name="s_${base}_${platform}"
        bin_to_c_array "$bin_file" "$var_name" >> "$header"
    done

    echo "Generated default header → ${header##*/}"
}

gen_sdf_header() {
    local platform=$1
    local bin_dir="${COMPILED_DIR}/${platform}/sdf"
    local header="${OUTPUT_DIR}/Rtt_BgfxShaderData_sdf_${platform}.h"

    echo "// Auto-generated: SDF ${platform} shader data" > "$header"
    echo "// Generated: $(date '+%Y-%m-%d %H:%M')" >> "$header"
    echo "" >> "$header"

    local count=0
    for bin_file in "${bin_dir}"/*.bin; do
        [[ ! -f "$bin_file" ]] && continue
        local base=$(basename "$bin_file" .bin)
        local var_name="s_${base}_${platform}"
        bin_to_c_array "$bin_file" "$var_name" >> "$header"
        count=$((count + 1))
    done

    echo "Generated SDF header: ${count} shaders → ${header##*/}"
}

gen_instanced_header() {
    local platform=$1
    local bin_dir="${COMPILED_DIR}/${platform}/instanced"
    local header="${OUTPUT_DIR}/Rtt_BgfxShaderData_instanced_${platform}.h"

    echo "// Auto-generated: instanced ${platform} shader data" > "$header"
    echo "// Generated: $(date '+%Y-%m-%d %H:%M')" >> "$header"
    echo "" >> "$header"

    local count=0
    for bin_file in "${bin_dir}"/*.bin; do
        [[ ! -f "$bin_file" ]] && continue
        local base=$(basename "$bin_file" .bin)
        local var_name="s_${base}_${platform}"
        bin_to_c_array "$bin_file" "$var_name" >> "$header"
        count=$((count + 1))
    done

    echo "Generated instanced header: ${count} shaders → ${header##*/}"
}

process_platform() {
    local platform=$1
    echo "========================================"
    echo "Processing platform: ${platform}"
    echo "========================================"

    compile_default "$platform"
    compile_effects "$platform"
    compile_sdf "$platform"
    compile_instanced "$platform"

    echo ""
    echo "--- Generating C headers ---"
    gen_default_header "$platform"
    gen_effects_header "$platform"
    gen_sdf_header "$platform"
    gen_instanced_header "$platform"
    echo ""
}

# Main
echo "bgfx Shader Header Generator"
echo "============================="

if [ ! -f "$SHADERC" ]; then
    echo "ERROR: shaderc not found at $SHADERC"
    exit 1
fi

case "$TARGET" in
    metal) process_platform "metal" ;;
    essl)  process_platform "essl" ;;
    all)
        process_platform "metal"
        process_platform "essl"
        ;;
    *) echo "Usage: $0 [metal|essl|all]"; exit 1 ;;
esac

# Report failures
FAIL_COUNT=$(wc -l < "$FAIL_LOG" | tr -d ' ')
if [ "$FAIL_COUNT" -gt 0 ]; then
    echo "WARNING: ${FAIL_COUNT} shaders failed to compile:"
    cat "$FAIL_LOG"
    exit 1
else
    echo "All shaders compiled and headers generated successfully!"
fi
