$input v_TexCoord, v_ColorScale, v_UserData

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Solar2D Generator Fragment Shader: linearGradient

#include <bgfx_shader.sh>

uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

uniform vec4 u_UserData0;
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
    vec4 fromColor = u_UserData0;
    vec2 fromPos = u_UserData1.xy;
    vec4 toColor = u_UserData2;
    vec2 toPos = u_UserData3.xy;

    vec2 V = (toPos - fromPos);
    float len_V = length(V);
    vec2 N = normalize(V);

    vec2 W = (texCoord - fromPos);

    float d = dot(W, N);

    d = clamp(d, 0.0, len_V);

    float progress = (d / len_V);

    #if 0
        if (progress <= 0.0)
        {
            return vec4(1.0, 0.0, 0.0, 1.0);
        }
        else if (progress >= 1.0)
        {
            return vec4(0.0, 1.0, 0.0, 1.0);
        }
        else
        {
            return vec4(0.0, 0.0, 1.0, 1.0);
        }
    #endif

    return (mix(fromColor, toColor, progress) * v_ColorScale);
}

void main()
{
    gl_FragColor = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
}
