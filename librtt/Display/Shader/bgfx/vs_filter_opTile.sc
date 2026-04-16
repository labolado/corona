$input a_position, a_texcoord0, a_color0, a_texcoord1
$output v_TexCoord, v_ColorScale, v_UserData, v_slot_size, v_sample_uv_offset, v_transform0, v_transform1, v_transform2, v_MaskUV0, v_MaskUV1, v_MaskUV2

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: opTile - Custom Vertex Shader

#include <bgfx_shader.sh>

uniform mat4 u_ViewProjectionMatrix;
uniform mat3 u_MaskMatrix0;
uniform mat3 u_MaskMatrix1;
uniform mat3 u_MaskMatrix2;

uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

uniform vec4 u_UserData0;
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

#define CoronaVertexUserData a_texcoord1
#define CoronaTexCoord a_texcoord0.xy
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

void main()
{
    v_TexCoord = vec3(a_texcoord0.xy, 0.0);
    v_ColorScale = a_color0;
    v_UserData = a_texcoord1;

    float numPixels = a_texcoord1.x;
    float rotation_in_radians = radians(a_texcoord1.y);
    v_slot_size = (u_TexelSize.zw * numPixels);
    v_sample_uv_offset = (v_slot_size * 0.5);

    mat3 translate0 = mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(-0.5, -0.5, 1.0));
    float s = sin(rotation_in_radians);
    float c = cos(rotation_in_radians);
    mat3 rotate = mat3(vec3(c, (-s), 0.0), vec3(s, c, 0.0), vec3(0.0, 0.0, 1.0));
    mat3 translate1 = mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.5, 0.5, 1.0));
    mat3 transform = (translate1 * rotate * translate0);
    v_transform0 = transform[0];
    v_transform1 = transform[1];
    v_transform2 = transform[2];


    // Compute mask UVs
    vec3 maskPos = vec3(a_position.xy, 1.0);
    v_MaskUV0 = (mul(u_MaskMatrix0, maskPos)).xy;
    v_MaskUV1 = (mul(u_MaskMatrix1, maskPos)).xy;
    v_MaskUV2 = (mul(u_MaskMatrix2, maskPos)).xy;
    gl_Position = mul(u_ViewProjectionMatrix, vec4(a_position.xy, 0.0, 1.0));
}
