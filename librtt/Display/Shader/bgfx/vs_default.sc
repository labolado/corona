//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Solar2D Default Vertex Shader for bgfx
// Based on shell_default_gl.lua vertex section

// Input attributes from vertex buffer
$input a_position, a_texcoord0, a_color0, a_texcoord1

// Output varyings to fragment shader
$output v_TexCoord, v_ColorScale, v_UserData

#if MASK_COUNT > 0
$output v_MaskUV0
#endif
#if MASK_COUNT > 1
$output v_MaskUV1
#endif
#if MASK_COUNT > 2
$output v_MaskUV2
#endif

// Uniforms
uniform mat4 u_ViewProjectionMatrix;

// Time uniforms (packed in vec4.x as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;      // Total time in .x component
uniform vec4 u_DeltaTime;      // Delta time in .x component

uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;   // Content scale in .xy components
uniform vec4 u_ContentSize;

// Mask matrices (conditionally compiled)
#if MASK_COUNT > 0
uniform mat3 u_MaskMatrix0;
#endif
#if MASK_COUNT > 1
uniform mat3 u_MaskMatrix1;
#endif
#if MASK_COUNT > 2
uniform mat3 u_MaskMatrix2;
#endif

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

// Vertex kernel function prototype
vec2 VertexKernel(vec2 position);

void main()
{
    // Pass through texture coordinates
    v_TexCoord = vec3(a_texcoord0.xy, 0.0);
    
    // Pass through color scale
    v_ColorScale = a_color0;
    
    // Pass through user data
    v_UserData = a_texcoord1;
    
    // Call vertex kernel to transform position
    vec2 position = VertexKernel(a_position.xy);
    
    // Compute mask UVs (conditionally compiled)
    #if MASK_COUNT > 0
    v_MaskUV0 = (u_MaskMatrix0 * vec3(position, 1.0)).xy;
    #endif
    
    #if MASK_COUNT > 1
    v_MaskUV1 = (u_MaskMatrix1 * vec3(position, 1.0)).xy;
    #endif
    
    #if MASK_COUNT > 2
    v_MaskUV2 = (u_MaskMatrix2 * vec3(position, 1.0)).xy;
    #endif
    
    // Transform to clip space
    gl_Position = mul(u_ViewProjectionMatrix, vec4(position, 0.0, 1.0));
}

// Default vertex kernel (pass-through)
// This can be overridden by custom shaders
vec2 VertexKernel(vec2 position)
{
    return position;
}
