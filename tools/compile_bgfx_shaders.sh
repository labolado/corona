#!/bin/bash

##############################################################################
#
# This file is part of the Solar2D game engine.
# With contributions from Dianchu Technology
# For overview and more information on licensing please refer to README.md 
# Home page: https://github.com/coronalabs/corona
# Contact: support@coronalabs.com
#
##############################################################################

# Solar2D bgfx Shader Compilation Script
# 
# This script compiles Solar2D's shader shell files for multiple bgfx backends:
# - Metal (iOS/macOS)
# - ESSL (Android OpenGL ES)
# - GLSL (Desktop OpenGL)
# - SPIRV (Vulkan)
#
# Prerequisites:
# - bgfx tools must be built first (shaderc executable)
# - bgfx shader include path must be available

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Paths
BGFX_DIR="${PROJECT_ROOT}/external/bgfx"
BGFX_TOOLS_DIR="${BGFX_DIR}/tools/bin"
SHADER_DIR="${PROJECT_ROOT}/librtt/Display/Shader/bgfx"
OUTPUT_DIR="${SHADER_DIR}/compiled"

# Shader types to compile
SHADER_TYPES=("vs" "fs")

# Versions (mask counts + wireframe)
VERSIONS=("mask0" "mask1" "mask2" "mask3" "wireframe")

# Platforms to compile for
PLATFORMS=("metal" "essl" "glsl" "spirv")

##############################################################################
# Helper Functions
##############################################################################

print_error() {
    echo -e "${RED}ERROR: $1${NC}" >&2
}

print_warning() {
    echo -e "${YELLOW}WARNING: $1${NC}"
}

print_success() {
    echo -e "${GREEN}$1${NC}"
}

print_info() {
    echo "INFO: $1"
}

##############################################################################
# Check Prerequisites
##############################################################################

check_prerequisites() {
    print_info "Checking prerequisites..."
    
    # Check if bgfx directory exists
    if [ ! -d "$BGFX_DIR" ]; then
        print_error "bgfx directory not found at: $BGFX_DIR"
        print_error "Please ensure bgfx submodule is initialized:"
        print_error "  git submodule update --init --recursive external/bgfx"
        exit 1
    fi
    
    # Check for shaderc
    SHADERC=""
    
    # Try different possible locations for shaderc
    if [ -f "${BGFX_TOOLS_DIR}/shaderc" ]; then
        SHADERC="${BGFX_TOOLS_DIR}/shaderc"
    elif [ -f "${BGFX_TOOLS_DIR}/shadercDebug" ]; then
        SHADERC="${BGFX_TOOLS_DIR}/shadercDebug"
    elif [ -f "${BGFX_TOOLS_DIR}/shadercRelease" ]; then
        SHADERC="${BGFX_TOOLS_DIR}/shadercRelease"
    elif [ -f "${PROJECT_ROOT}/.build/shaderc" ]; then
        SHADERC="${PROJECT_ROOT}/.build/shaderc"
    elif command -v shaderc &> /dev/null; then
        SHADERC="shaderc"
    fi
    
    if [ -z "$SHADERC" ]; then
        print_error "shaderc not found!"
        print_error ""
        print_error "Please build bgfx tools first:"
        print_error "  1. cd external/bgfx"
        print_error "  2. make tools (Linux/macOS)"
        print_error "  3. Or build with your platform's build system"
        print_error ""
        print_error "Expected shaderc location: ${BGFX_TOOLS_DIR}/shaderc"
        exit 1
    fi
    
    print_success "Found shaderc: $SHADERC"
    
    # Check for bgfx shader includes
    BGFX_INCLUDE="${BGFX_DIR}/src"
    if [ ! -d "$BGFX_INCLUDE" ]; then
        print_error "bgfx shader includes not found at: $BGFX_INCLUDE"
        exit 1
    fi
    
    print_success "Found bgfx includes: $BGFX_INCLUDE"
    
    # Create output directory
    mkdir -p "$OUTPUT_DIR"
}

##############################################################################
# Get Platform Parameters
##############################################################################

get_platform_params() {
    local platform=$1
    local type=$2  # vs or fs
    
    local profile=""
    local platform_arg=""
    
    case "$platform" in
        "metal")
            platform_arg="metal"
            if [ "$type" = "vs" ]; then
                profile="metal"
            else
                profile="metal"
            fi
            ;;
        "essl")
            platform_arg="android"
            profile="100_es"
            ;;
        "glsl")
            platform_arg="linux"
            if [ "$type" = "vs" ]; then
                profile="120"
            else
                profile="120"
            fi
            ;;
        "spirv")
            platform_arg="vulkan"
            profile="spirv"
            ;;
        *)
            print_error "Unknown platform: $platform"
            exit 1
            ;;
    esac
    
    echo "$platform_arg $profile"
}

##############################################################################
# Compile Shader
##############################################################################

compile_shader() {
    local type=$1        # vs or fs
    local version=$2     # mask0, mask1, etc.
    local platform=$3    # metal, essl, etc.
    
    local input_file="${SHADER_DIR}/${type}_default.sc"
    local varying_def="${SHADER_DIR}/varying.def.sc"
    local output_file="${OUTPUT_DIR}/${type}_${version}.${platform}"
    
    # Determine shader stage
    local stage=""
    if [ "$type" = "vs" ]; then
        stage="vertex"
    else
        stage="fragment"
    fi
    
    # Get platform-specific parameters
    local params=$(get_platform_params "$platform" "$type")
    local platform_arg=$(echo "$params" | cut -d' ' -f1)
    local profile=$(echo "$params" | cut -d' ' -f2)
    
    # Build compile command
    local cmd="$SHADERC"
    cmd="$cmd --type $stage"
    cmd="$cmd --platform $platform_arg"
    cmd="$cmd --profile $profile"
    cmd="$cmd -f $input_file"
    cmd="$cmd -o $output_file"
    cmd="$cmd --varyingdef $varying_def"
    cmd="$cmd -i $BGFX_INCLUDE"
    
    # Add mask count defines
    local mask_count=0
    case "$version" in
        "mask0") mask_count=0 ;;
        "mask1") mask_count=1 ;;
        "mask2") mask_count=2 ;;
        "mask3") mask_count=3 ;;
        "wireframe") mask_count=0 ;;  # Wireframe uses mask0 setup
    esac
    
    cmd="$cmd --define \"MASK_COUNT=$mask_count\""
    
    # Add wireframe define for wireframe version
    if [ "$version" = "wireframe" ]; then
        cmd="$cmd --define \"WIRE_FRAME=1\""
    fi
    
    # Execute compile command
    print_info "Compiling ${type}_${version} for ${platform}..."
    
    if eval "$cmd" 2>&1; then
        print_success "  -> ${output_file#$PROJECT_ROOT/}"
        return 0
    else
        print_error "Failed to compile ${type}_${version} for ${platform}"
        return 1
    fi
}

##############################################################################
# Main
##############################################################################

main() {
    echo "========================================================================"
    echo "Solar2D bgfx Shader Compilation"
    echo "========================================================================"
    echo ""
    
    # Check prerequisites
    check_prerequisites
    echo ""
    
    # Statistics
    local total=0
    local success=0
    local failed=0
    
    # Compile all combinations
    for platform in "${PLATFORMS[@]}"; do
        echo "------------------------------------------------------------------------"
        print_info "Platform: $platform"
        echo "------------------------------------------------------------------------"
        
        for type in "${SHADER_TYPES[@]}"; do
            for version in "${VERSIONS[@]}"; do
                total=$((total + 1))
                
                if compile_shader "$type" "$version" "$platform"; then
                    success=$((success + 1))
                else
                    failed=$((failed + 1))
                    # Continue on error, but track failures
                fi
            done
        done
        echo ""
    done
    
    # Summary
    echo "========================================================================"
    echo "Compilation Summary"
    echo "========================================================================"
    print_info "Total:   $total"
    print_success "Success: $success"
    
    if [ $failed -gt 0 ]; then
        print_error "Failed:  $failed"
        exit 1
    fi
    
    echo ""
    print_success "All shaders compiled successfully!"
    print_info "Output directory: $OUTPUT_DIR"
    
    return 0
}

# Run main
main "$@"
