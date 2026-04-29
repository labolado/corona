$input a_position, a_texcoord0, a_color0, a_texcoord1
$output v_TexCoord, v_ColorScale, v_UserData, v_feathering_edges, v_MaskUV0, v_MaskUV1, v_MaskUV2

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: iris - Custom Vertex Shader

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

    vec2 center = u_UserData0.xy;
    float xMax = max(center.x, (1.0 - center.x));
    float yMax = max(center.y, (1.0 - center.y));
    float unitOpenness = u_UserData1.x;
    float aspectRatio = u_UserData2.x;
    float unitSmoothness = u_UserData3.x;
    float feathering_edge_thickness = (0.5 * unitSmoothness);
    float a = (xMax * aspectRatio);
    a = (a * a);
    float b = yMax;
    b = (b * b);
    float maximum_distance_to_cover = (sqrt(a + b) + feathering_edge_thickness);
    v_feathering_edges.y = (unitOpenness * maximum_distance_to_cover);
    v_feathering_edges.x = (v_feathering_edges.y - feathering_edge_thickness);


    // Compute mask UVs
    vec3 maskPos = vec3(a_position.xy, 1.0);
    v_MaskUV0 = (mul(u_MaskMatrix0, maskPos)).xy;
    v_MaskUV1 = (mul(u_MaskMatrix1, maskPos)).xy;
    v_MaskUV2 = (mul(u_MaskMatrix2, maskPos)).xy;
    gl_Position = mul(u_ViewProjectionMatrix, vec4(a_position.xy, 0.0, 1.0));
}
