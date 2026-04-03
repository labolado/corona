//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Solar2D Default Fragment Shader for bgfx
// Based on shell_default_gl.lua and kernel_default_gl.lua

// Input varyings from vertex shader
$input v_TexCoord, v_ColorScale, v_UserData

#if MASK_COUNT > 0
$input v_MaskUV0
#endif
#if MASK_COUNT > 1
$input v_MaskUV1
#endif
#if MASK_COUNT > 2
$input v_MaskUV2
#endif

// Sampler uniforms
uniform sampler2D u_FillSampler0;
uniform sampler2D u_FillSampler1;

// Mask samplers (conditionally compiled)
#if MASK_COUNT > 0
uniform sampler2D u_MaskSampler0;
#endif
#if MASK_COUNT > 1
uniform sampler2D u_MaskSampler1;
#endif
#if MASK_COUNT > 2
uniform sampler2D u_MaskSampler2;
#endif

// Time and data uniforms (packed in vec4 as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;      // Total time in .x component
uniform vec4 u_DeltaTime;      // Delta time in .x component
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;   // Content scale in .xy components
uniform vec4 u_ContentSize;

// User data uniforms
uniform vec4 u_UserData0;
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy
#define CoronaSampler0 u_FillSampler0
#define CoronaSampler1 u_FillSampler1

// Fragment kernel function prototype
vec4 FragmentKernel(vec2 texCoord);

void main()
{
    // Call fragment kernel to get base color
    vec4 result = FragmentKernel(v_TexCoord.xy);
    
    // Apply mask modulation (conditionally compiled)
    #if MASK_COUNT > 0
    result *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    #endif
    
    #if MASK_COUNT > 1
    result *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    #endif
    
    #if MASK_COUNT > 2
    result *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    #endif
    
    // Output final color
    gl_FragColor = result;
}

// Default fragment kernel
// Returns texture color modulated by vertex color scale
// Based on kernel_default_gl.lua
vec4 FragmentKernel(vec2 texCoord)
{
    vec4 texColor = texture2D(u_FillSampler0, texCoord);
    return texColor * v_ColorScale;
}
