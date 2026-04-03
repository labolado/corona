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

// Solar2D Generator Fragment Shader: radialGradient

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
    float aspectRatio = u_UserData0.x;

    vec2 center = vec2((u_UserData1.x * aspectRatio), u_UserData1.y);
    float innerRadius = u_UserData1.z;
    float outerRadius = u_UserData1.w;

    vec4 color1 = u_UserData2;
    vec4 color2 = u_UserData3;

    float one_over_radius_range = (1.0 / (outerRadius - innerRadius));

    vec2 pos = vec2((texCoord.x * aspectRatio), texCoord.y);

    float dist0 = distance(pos, center);

    float dist1 = ((dist0 - innerRadius) * one_over_radius_range);

    #if 0
        if (dist0 <= innerRadius)
        {
            return vec4(1.0, 0.0, 0.0, 1.0);
        }
        else if (dist0 >= outerRadius)
        {
            return vec4(0.0, 1.0, 0.0, 1.0);
        }
        else
        {
            return vec4(0.0, 0.0, 1.0, 1.0);
        }
    #endif

    return (mix(color1, color2, dist1) * v_ColorScale);
}

void main()
{
    gl_FragColor = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
}
