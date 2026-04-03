$input a_position, a_texcoord0, a_color0, a_texcoord1
$output v_TexCoord, v_ColorScale, v_UserData, v_fromPos, v_N

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: linearWipe - Custom Vertex Shader

#include <bgfx_shader.sh>

uniform mat4 u_ViewProjectionMatrix;

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

    v_N = normalize(u_UserData0.xy);
    float unitSmoothness = u_UserData1.x;
    float unitProgress = u_UserData2.x;
    vec2 A;
    vec2 B;
    if ((v_N.x >= 0.0) && (v_N.y >= 0.0))
    {
        A = vec2(0.0, 0.0);
        B = vec2(1.0, 1.0);
    }
    else if ((v_N.x <= 0.0) && (v_N.y >= 0.0))
    {
        A = vec2(1.0, 0.0);
        B = vec2(0.0, 1.0);
    }
    else if ((v_N.x <= 0.0) && (v_N.y <= 0.0))
    {
        A = vec2(1.0, 1.0);
        B = vec2(0.0, 0.0);
    }
    else
    {
        A = vec2(0.0, 1.0);
        B = vec2(1.0, 0.0);
    }
    vec2 V = (B - A);
    float distance_to_cover = (abs(dot(V, v_N)) + unitSmoothness);
    float fromPos_progress = ((unitProgress * distance_to_cover) - unitSmoothness);
    v_fromPos = (A + (v_N * fromPos_progress));

    gl_Position = mul(u_ViewProjectionMatrix, vec4(a_position.xy, 0.0, 1.0));
}
