//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Solar2D default vertex shader shared kernel.
//
// Each vs_default_mN.sc declares its own $input/$output (bgfx shaderc
// parses those directives before GLSL preprocessing, so they cannot be
// gated by #if), then #defines MASK_COUNT and includes this header.
//
// MASK_COUNT controls whether the corresponding a_texcoord2/3/4 attribute
// is consumed and forwarded to v_MaskUV0/1/2. Mask UVs are pre-computed
// on the CPU (Renderer::BakeMaskUVsIntoVertices) and written into the
// vertex stream.

#ifndef MASK_COUNT
#define MASK_COUNT 0
#endif

#include <bgfx_shader.sh>

uniform mat4 u_ViewProjectionMatrix;

// Time uniforms (packed in vec4.x as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;

uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

// User data uniforms
uniform vec4 u_UserData0;
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

// Solar2D macros for shader compatibility
#define CoronaVertexUserData a_texcoord1
#define CoronaTexCoord a_texcoord0.xy
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

void main()
{
    // Pass through texture coordinates (keep z=0.0 to avoid Metal varying issue)
    v_TexCoord = vec3(a_texcoord0.xy, 0.0);

    // Pass through color scale
    v_ColorScale = a_color0;

    // Pass through user data, pack q-coordinate into .w for perspective-correct UV.
    v_UserData = vec4(a_texcoord1.xyz, a_texcoord0.z);

    // Mask UVs come from the vertex stream (pre-baked CPU-side); kMaskCount0
    // binary does not declare a_texcoord2/3/4, so v_MaskUV0/1/2 are written
    // with zeros to satisfy the fragment shader's $input contract.
#if MASK_COUNT > 0
    v_MaskUV0 = a_texcoord2;
#else
    v_MaskUV0 = vec2(0.0, 0.0);
#endif
#if MASK_COUNT > 1
    v_MaskUV1 = a_texcoord3;
#else
    v_MaskUV1 = vec2(0.0, 0.0);
#endif
#if MASK_COUNT > 2
    v_MaskUV2 = a_texcoord4;
#else
    v_MaskUV2 = vec2(0.0, 0.0);
#endif

    // Transform to clip space
    gl_Position = mul(u_ViewProjectionMatrix, vec4(a_position.xy, 0.0, 1.0));
}
