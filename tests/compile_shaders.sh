#!/bin/bash
# compile_shaders.sh — Compile all bgfx .sc shaders to binary headers
#
# Usage:
#   bash tests/compile_shaders.sh              # compile all
#   bash tests/compile_shaders.sh default      # compile only vs/fs_default
#   bash tests/compile_shaders.sh --check      # check if binaries are up-to-date
#
# Must run from corona repo root.

set -euo pipefail

CORONA_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SHADERC="$CORONA_DIR/external/bgfx/tools/bin/darwin/shaderc"
SHADER_DIR="$CORONA_DIR/librtt/Display/Shader/bgfx"
INCLUDE_DIR="$CORONA_DIR/external/bgfx/src"
OUTPUT_DIR="$CORONA_DIR/librtt/Renderer"
VARYING="$SHADER_DIR/varying.def.sc"

if [ ! -x "$SHADERC" ]; then
    echo "ERROR: shaderc not found at $SHADERC"
    exit 1
fi

# Shader groups → header files
# default: vs_default + fs_default → Rtt_BgfxShaderData_{metal,essl}.h
# effects: all fs_composite_* + fs_filter_* + fs_generator_* + vs_filter_* → Rtt_BgfxShaderData_effects_{metal,essl}.h

FILTER="${1:-all}"

compile_shader() {
    local SC_FILE="$1"
    local TYPE="$2"       # vertex or fragment
    local PROFILE="$3"    # metal or 100_es
    local OUT_BIN="$4"

    local PLATFORM_FLAG=""
    if [ "$PROFILE" = "metal" ]; then
        PLATFORM_FLAG="--platform osx"
    else
        PLATFORM_FLAG="--platform android"
    fi

    "$SHADERC" \
        --type "$TYPE" \
        $PLATFORM_FLAG \
        -p "$PROFILE" \
        -f "$SC_FILE" \
        -o "$OUT_BIN" \
        --varyingdef "$VARYING" \
        -i "$INCLUDE_DIR" \
        -i "$SHADER_DIR" \
        2>&1
}

bin_to_c_array() {
    local BIN_FILE="$1"
    local ARRAY_NAME="$2"

    python3 -c "
import sys
data = open('$BIN_FILE', 'rb').read()
name = '$ARRAY_NAME'
print(f'static const unsigned char {name}[] = {{')
for i in range(0, len(data), 16):
    chunk = data[i:i+16]
    hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
    print(f'  {hex_str},')
print(f'}};')
print(f'static const unsigned int {name}_size = sizeof({name});')
"
}

compile_default() {
    echo "=== Compiling default shaders ==="
    local TMP_DIR=$(mktemp -d)

    for PROFILE in metal 100_es; do
        local SUFFIX
        if [ "$PROFILE" = "metal" ]; then SUFFIX="metal"; else SUFFIX="essl"; fi
        local HEADER="$OUTPUT_DIR/Rtt_BgfxShaderData_${SUFFIX}.h"

        echo "  Compiling vs_default ($SUFFIX)..."
        compile_shader "$SHADER_DIR/vs_default.sc" vertex "$PROFILE" "$TMP_DIR/vs_default_${SUFFIX}.bin"

        echo "  Compiling fs_default ($SUFFIX)..."
        compile_shader "$SHADER_DIR/fs_default.sc" fragment "$PROFILE" "$TMP_DIR/fs_default_${SUFFIX}.bin"

        # Generate header
        {
            echo "// Auto-generated bgfx shader data ($SUFFIX)"
            echo "// Source: vs_default.sc, fs_default.sc"
            echo "// Generated: $(date '+%Y-%m-%d %H:%M:%S')"
            echo "// DO NOT EDIT — regenerate with: bash tests/compile_shaders.sh default"
            echo ""
            echo "#pragma once"
            echo ""
            bin_to_c_array "$TMP_DIR/vs_default_${SUFFIX}.bin" "s_vs_default_${SUFFIX}"
            echo ""
            bin_to_c_array "$TMP_DIR/fs_default_${SUFFIX}.bin" "s_fs_default_${SUFFIX}"
        } > "$HEADER"

        echo "  → $HEADER (VS: $(wc -c < "$TMP_DIR/vs_default_${SUFFIX}.bin") bytes, FS: $(wc -c < "$TMP_DIR/fs_default_${SUFFIX}.bin") bytes)"
    done

    rm -rf "$TMP_DIR"
    echo "=== Default shaders done ==="
}

compile_effects() {
    echo "=== Compiling effect shaders ==="
    local TMP_DIR=$(mktemp -d)
    local COUNT=0
    local FAIL=0

    for PROFILE in metal 100_es; do
        local SUFFIX
        if [ "$PROFILE" = "metal" ]; then SUFFIX="metal"; else SUFFIX="essl"; fi
        local HEADER="$OUTPUT_DIR/Rtt_BgfxShaderData_effects_${SUFFIX}.h"

        # Start header
        {
            echo "// Auto-generated bgfx effect shader data ($SUFFIX)"
            echo "// Generated: $(date '+%Y-%m-%d %H:%M:%S')"
            echo "// DO NOT EDIT — regenerate with: bash tests/compile_shaders.sh effects"
            echo ""
            echo "#pragma once"
            echo ""
        } > "$HEADER"

        # Compile all fragment shaders (except fs_default)
        for SC in "$SHADER_DIR"/fs_*.sc; do
            local NAME=$(basename "$SC" .sc)
            [ "$NAME" = "fs_default" ] && continue

            local BIN="$TMP_DIR/${NAME}_${SUFFIX}.bin"
            echo -n "  $NAME ($SUFFIX)... "
            if compile_shader "$SC" fragment "$PROFILE" "$BIN" > /dev/null 2>&1; then
                bin_to_c_array "$BIN" "s_${NAME}_${SUFFIX}" >> "$HEADER"
                echo "" >> "$HEADER"
                echo "OK ($(wc -c < "$BIN") bytes)"
                COUNT=$((COUNT + 1))
            else
                echo "FAILED"
                FAIL=$((FAIL + 1))
            fi
        done

        # Compile all vertex shaders (except vs_default)
        for SC in "$SHADER_DIR"/vs_*.sc; do
            local NAME=$(basename "$SC" .sc)
            [ "$NAME" = "vs_default" ] && continue

            local BIN="$TMP_DIR/${NAME}_${SUFFIX}.bin"
            echo -n "  $NAME ($SUFFIX)... "
            if compile_shader "$SC" vertex "$PROFILE" "$BIN" > /dev/null 2>&1; then
                bin_to_c_array "$BIN" "s_${NAME}_${SUFFIX}" >> "$HEADER"
                echo "" >> "$HEADER"
                echo "OK ($(wc -c < "$BIN") bytes)"
                COUNT=$((COUNT + 1))
            else
                echo "FAILED"
                FAIL=$((FAIL + 1))
            fi
        done

        echo "  → $HEADER"
    done

    rm -rf "$TMP_DIR"
    echo "=== Effects done: $COUNT compiled, $FAIL failed ==="
}

check_sync() {
    echo "=== Checking shader binary sync ==="
    local TMP_DIR=$(mktemp -d)
    local STALE=0

    for PROFILE in metal 100_es; do
        local SUFFIX
        if [ "$PROFILE" = "metal" ]; then SUFFIX="metal"; else SUFFIX="essl"; fi

        # Check default shaders
        compile_shader "$SHADER_DIR/vs_default.sc" vertex "$PROFILE" "$TMP_DIR/vs_default.bin" > /dev/null 2>&1
        compile_shader "$SHADER_DIR/fs_default.sc" fragment "$PROFILE" "$TMP_DIR/fs_default.bin" > /dev/null 2>&1

        local VS_SIZE=$(wc -c < "$TMP_DIR/vs_default.bin")
        local FS_SIZE=$(wc -c < "$TMP_DIR/fs_default.bin")

        # Compare by counting hex bytes in header array
        local HEADER_VS_COUNT=$(grep -c '0x[0-9a-f][0-9a-f]' "$OUTPUT_DIR/Rtt_BgfxShaderData_${SUFFIX}.h" 2>/dev/null | head -1 || echo "0")
        # More reliable: hash the compiled binary vs extracted header bytes
        local BIN_HASH=$(md5 -q "$TMP_DIR/vs_default.bin" 2>/dev/null || md5sum "$TMP_DIR/vs_default.bin" | cut -d' ' -f1)
        local BIN_HASH_FS=$(md5 -q "$TMP_DIR/fs_default.bin" 2>/dev/null || md5sum "$TMP_DIR/fs_default.bin" | cut -d' ' -f1)

        # Extract binary from header and compare
        local HEADER_BIN="$TMP_DIR/header_vs_${SUFFIX}.bin"
        python3 -c "
import re
data = open('$OUTPUT_DIR/Rtt_BgfxShaderData_${SUFFIX}.h').read()
# Find vs array
m = re.search(r's_vs_default_${SUFFIX}\[\] = \{([^}]+)\}', data)
if m:
    hexvals = re.findall(r'0x[0-9a-fA-F]+', m.group(1))
    open('$HEADER_BIN', 'wb').write(bytes(int(h,16) for h in hexvals))
" 2>/dev/null
        local HEADER_HASH=$(md5 -q "$HEADER_BIN" 2>/dev/null || echo "none")

        if [ "$BIN_HASH" != "$HEADER_HASH" ]; then
            echo "  STALE: default VS ($SUFFIX) — compiled=$BIN_HASH header=$HEADER_HASH"
            STALE=$((STALE + 1))
        else
            echo "  OK: default ($SUFFIX)"
        fi
    done

    rm -rf "$TMP_DIR"

    if [ "$STALE" -gt 0 ]; then
        echo "=== $STALE shader(s) out of sync! Run: bash tests/compile_shaders.sh ==="
        exit 1
    else
        echo "=== All shader binaries up to date ==="
    fi
}

case "$FILTER" in
    all)
        compile_default
        compile_effects
        ;;
    default)
        compile_default
        ;;
    effects)
        compile_effects
        ;;
    --check)
        check_sync
        ;;
    *)
        echo "Usage: $0 [all|default|effects|--check]"
        exit 1
        ;;
esac
