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

// Solar2D Generator Fragment Shader: sunbeams

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

float unit_rand_1d(in float v)
{
    return fract(sin(v) * 43758.5453);
}

vec2 unit_rand_2d(in vec2 v)
{
    v = vec2(dot(v, vec2(127.1, 311.7)), dot(v, vec2(269.5, 183.3)));
    return fract(sin(v) * 43758.5453);
}

float noise(in float v)
{
    float v0 = unit_rand_1d(floor(v));
    float v1 = unit_rand_1d(ceil(v));
    float m = fract(v);
    float p = mix(v0, v1, m);
    return p;
}

float sunbeam(float seed, vec2 uv, vec2 pos)
{
    vec2 main = uv - pos;
    vec2 uvd = uv * (length(uv));

    float ang = atan2(main.x, main.y);
    float dist = length(main);
    dist = pow(dist, 0.1);

    float f0 = 1.0 / (length(uv - pos) * 16.0 + 1.0);

    float f_intermediate0 = ((pos.x + pos.y) * 2.2);
    float f_intermediate1 = (ang * 4.0);
    float f_intermediate2 = (seed + f_intermediate0 + f_intermediate1 + 5.954);
    float f_intermediate3 = (sin(noise(f_intermediate2) * 16.0) * 0.1);

    f0 += (f0 * (f_intermediate3 + (dist * 0.1) + 0.8));

    return f0;
}

vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
    float aspectRatio = v_UserData.z;

    vec2 center_pos = vec2((v_UserData.x * aspectRatio), v_UserData.y);

    vec2 tc = vec2((texCoord.x * aspectRatio), texCoord.y);

    float seed = v_UserData.w;

    float intensity = sunbeam(seed, tc, center_pos);

    float d = distance(tc, center_pos);
    intensity -= (0.5 * d);

    #if 0
        return vec4(intensity);
    #else
        return (vec4(1.4, 1.2, 1.0, 1.0) * intensity);
    #endif
}

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_TexCoord.z;
    if (q > 0.0) texCoord = texCoord / q;
    vec4 _masked = FragmentKernel(texCoord, v_ColorScale, v_UserData);
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
