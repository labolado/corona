$input a_position, a_texcoord0, a_color0, a_texcoord1, a_indices
$output v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2

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

#include <bgfx_shader.sh>

// Uniforms
uniform mat4 u_ViewProjectionMatrix;

// Per-vertex mask matrix arrays (008 mask per-vertex encoding).
// Replaces single u_MaskMatrix0/1/2; default shader looks up matrix
// per-vertex via a_indices.{x,y,z} (0..15). Filter VS still uses the
// old single u_MaskMatrix* names with their own uniform handles.
#define MASK_MATRIX_ARRAY_SIZE 16
uniform mat3 u_MaskMatricesArr0[MASK_MATRIX_ARRAY_SIZE];
uniform mat3 u_MaskMatricesArr1[MASK_MATRIX_ARRAY_SIZE];
uniform mat3 u_MaskMatricesArr2[MASK_MATRIX_ARRAY_SIZE];

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
    // NOTE: This overwrites a_texcoord1.w with the q coefficient. Effect shaders
    // that read CoronaVertexUserData.w (monotone, sunbeams, lenticularHalo) will
    // receive q instead of the original .w value. On non-2.5D objects q=1.0;
    // on 2.5D offset objects q=1.5~3.0 which may affect those effects visually.
    // See Issue #18 for full analysis.
    v_UserData = vec4(a_texcoord1.xyz, a_texcoord0.z);

    // Compute mask UVs using per-vertex array indices into mat3 arrays.
    // a_indices arrives as Uint8 unnormalized — value range 0..15.
    vec3 maskPos = vec3(a_position.xy, 1.0);
    int idx0 = int(a_indices.x);
    int idx1 = int(a_indices.y);
    int idx2 = int(a_indices.z);
    v_MaskUV0 = (mul(u_MaskMatricesArr0[idx0], maskPos)).xy;
    v_MaskUV1 = (mul(u_MaskMatricesArr1[idx1], maskPos)).xy;
    v_MaskUV2 = (mul(u_MaskMatricesArr2[idx2], maskPos)).xy;

    // Transform to clip space
    gl_Position = mul(u_ViewProjectionMatrix, vec4(a_position.xy, 0.0, 1.0));
}
