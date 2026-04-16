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

// Solar2D Generator Fragment Shader: marchingAnts

#include <bgfx_shader.sh>

SAMPLER2D(u_MaskSampler0, 2);
SAMPLER2D(u_MaskSampler1, 3);
SAMPLER2D(u_MaskSampler2, 4);

uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

uniform vec4 u_UserData0;
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

// Texture flags: .x = 1.0 for alpha-only texture, .y = mask count (0..3)
uniform vec4 u_TexFlags;

#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData, vec2 fragCoord)
{
    float pixelOnPeriod1 = (6.0 / u_ContentScale.x);
    float pixelOffPeriod1 = (6.0 / u_ContentScale.x);
    float pixelOnPeriod2 = (6.0 / u_ContentScale.x);
    float pixelOffPeriod2 = (6.0 / u_ContentScale.x);
    float pixelSum2 = (pixelOnPeriod2 + pixelOffPeriod2);
    float rotation_in_radians = radians(-45.0);
    float s = sin(rotation_in_radians);
    float c = cos(rotation_in_radians);
    float translation = (u_TotalTime.x * (16.0 / u_ContentScale.x));

    float pixelSum1 = (pixelOnPeriod1 + pixelOffPeriod1);
    float pixelSumAll = (pixelSum1 + pixelSum2);
    float pixelSum1_and_pixelOnPeriod2 = (pixelSum1 + pixelOnPeriod2);
    float pixelOffPeriod1_and_pixelOnPeriod2 = (pixelOffPeriod1 + pixelOnPeriod2);

    mat3 transform;
    transform[0] = vec3(c, (-s), 0.0);
    transform[1] = vec3(s, c, 0.0);
    transform[2] = vec3(translation, 0.0, 1.0);

    vec2 tc = (transform * vec3(fragCoord, 1.0)).xy;

    float pixel_x = mod(tc.x, pixelSumAll);

    float gray;

    #if 0
        if (pixel_x < pixelSum1)
        {
            gray = step(pixel_x, pixelOnPeriod1);
        }
        else
        {
            gray = step((pixel_x - pixelSum1), pixelOnPeriod2);
        }
    #else
        float threshold_delta = (pixelOffPeriod1_and_pixelOnPeriod2 * step(pixel_x, pixelSum1));
        float threshold = (pixelSum1_and_pixelOnPeriod2 - threshold_delta);
        gray = step(pixel_x, threshold);
    #endif

    return (vec4(vec3(gray), 1.0) * v_ColorScale);
}

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_TexCoord.z;
    if (q > 0.0) texCoord = texCoord / q;
    vec4 _masked = FragmentKernel(texCoord, v_ColorScale, v_UserData, gl_FragCoord.xy);
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
