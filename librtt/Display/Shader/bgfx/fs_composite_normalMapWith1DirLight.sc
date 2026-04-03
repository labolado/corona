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

// Solar2D Composite Fragment Shader: normalMapWith1DirLight

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);
SAMPLER2D(u_FillSampler1, 1);

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
#define CoronaSampler0 u_FillSampler0
#define CoronaSampler1 u_FillSampler1

vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
    float ambientLightIntensity = u_UserData2.x;

    vec3 dirLightColor = (u_UserData0.rgb * u_UserData0.a);
    vec3 dirLightDirection = normalize(u_UserData1.xyz);

    vec4 texColor = texture2D(u_FillSampler0, texCoord);
    vec3 surface_normal = texture2D(u_FillSampler1, texCoord).xyz;
    surface_normal.xyz = normalize((surface_normal.xyz * 2.0) - 1.0);

    float diffuse_intensity = max(dot(dirLightDirection, surface_normal), 0.0);

    texColor.rgb *= (dirLightColor * (diffuse_intensity + ambientLightIntensity));

    #if 0
        vec2 light_position_in_tc = ((dirLightDirection.xy + 1.0) * 0.5);
        float light_distance = distance(texCoord, light_position_in_tc);
        if (light_distance < (1.0 / 92.0))
        {
            return vec4(0.0, 0.0, 0.0, 1.0);
        }
        else if (light_distance < (1.0 / 64.0))
        {
            return vec4(1.0, 1.0, 1.0, 1.0);
        }
    #endif

    return (texColor * v_ColorScale);
}

void main()
{
    gl_FragColor = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
}
