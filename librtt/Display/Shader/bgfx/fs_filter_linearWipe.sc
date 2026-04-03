$input v_TexCoord, v_ColorScale, v_UserData, v_fromPos, v_N

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: linearWipe

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);

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

void main()
{
    float unitSmoothness = u_UserData1.x;

    vec2 W = (v_TexCoord.xy - v_fromPos);

    float d = dot(W, v_N);

    d = clamp(d, 0.0, unitSmoothness);

    float progress = (d / unitSmoothness);

    vec4 color = (texture2D(u_FillSampler0, v_TexCoord.xy) * v_ColorScale);

    #if 0
        color = v_ColorScale;
    #elif 0
        if (progress <= 0.0)
        {
            gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
        else if (progress >= 1.0)
        {
            gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
        else
        {
            gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);
        }
    #endif

    gl_FragColor = mix(color, vec4(0.0), progress);
}
