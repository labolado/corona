$input v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Solar2D Composite Fragment Shader: normalMapWith1PointLight

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

float GetDistanceAttenuation(in vec3 attenuationFactors, in float light_distance)
{
    float constant_attenuation_factor = attenuationFactors.x;
    float linear_attenuation_factor = attenuationFactors.y;
    float quadratic_attenuation_factor = attenuationFactors.z;

    float constant_attenuation = constant_attenuation_factor;
    float linear_attenuation = (linear_attenuation_factor * light_distance);
    float quadratic_attenuation = (quadratic_attenuation_factor * light_distance * light_distance);

    return (1.0 / (constant_attenuation + linear_attenuation + quadratic_attenuation));
}

vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
    vec3 pointLightPos = u_UserData1.xyz;
    float ambientLightIntensity = u_UserData2.x;

    vec3 pointLightColor = (u_UserData0.rgb * u_UserData0.a);

    vec4 texColor = texture2D(u_FillSampler0, texCoord);
    vec3 surface_normal = texture2D(u_FillSampler1, texCoord).xyz;
    surface_normal.xyz = normalize((surface_normal.xyz * 2.0) - 1.0);

    vec3 fragment_to_light = (pointLightPos - vec3(texCoord, 0.0));
    vec3 light_direction = normalize(fragment_to_light);

    float attenuation = GetDistanceAttenuation(u_UserData3.xyz, length(fragment_to_light));

    float diffuse_intensity = max(dot(light_direction, surface_normal), 0.0);
    diffuse_intensity *= attenuation;

    texColor.rgb *= (pointLightColor * (diffuse_intensity + ambientLightIntensity));

    #if 0
        float light_distance = distance(texCoord, pointLightPos.xy);
        const float inner_threshold = (1.0 / 92.0);
        const float outer_threshold = (1.0 / 64.0);
        if (light_distance < inner_threshold)
        {
            if (pointLightPos.z >= 0.0)
            {
                return vec4(0.0, 1.0, 0.0, 1.0);
            }
            else
            {
                return vec4(1.0, 0.0, 0.0, 1.0);
            }
        }
        else if (light_distance < outer_threshold)
        {
            return vec4(1.0, 1.0, 1.0, 1.0);
        }
    #endif

    #if 0
        return vec4(attenuation);
    #endif

    return (texColor * v_ColorScale);
}

void main()
{
    gl_FragColor = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
}
