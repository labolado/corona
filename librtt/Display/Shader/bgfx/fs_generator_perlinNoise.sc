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

// Solar2D Generator Fragment Shader: perlinNoise

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

vec4 mod289(in vec4 x)
{
    return (x - floor(x * (1.0 / 289.0)) * 289.0);
}

vec4 permute(in vec4 x)
{
    return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(in vec4 r)
{
    return (1.79284291400159 - (0.85373472095314 * r));
}

vec2 fade(in vec2 t)
{
    return (t * t * t * (t * (t * 6.0 - 15.0) + 10.0));
}

float cnoise(in vec2 P)
{
    vec4 P_i = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 P_f = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    P_i = mod289(P_i);
    vec4 i_x = P_i.xzxz;
    vec4 i_y = P_i.yyww;
    vec4 f_x = P_f.xzxz;
    vec4 f_y = P_f.yyww;

    vec4 i = permute(permute(i_x) + i_y);

    vec4 g_x = fract(i * (1.0 / 41.0)) * 2.0 - 1.0;
    vec4 g_y = abs(g_x) - 0.5;
    vec4 t_x = floor(g_x + 0.5);
    g_x = g_x - t_x;

    vec2 g_00 = vec2(g_x.x, g_y.x);
    vec2 g_10 = vec2(g_x.y, g_y.y);
    vec2 g_01 = vec2(g_x.z, g_y.z);
    vec2 g_11 = vec2(g_x.w, g_y.w);

    vec4 norm = taylorInvSqrt(vec4(dot(g_00, g_00), dot(g_01, g_01), dot(g_10, g_10), dot(g_11, g_11)));
    g_00 *= norm.x;
    g_01 *= norm.y;
    g_10 *= norm.z;
    g_11 *= norm.w;

    float n_00 = dot(g_00, vec2(f_x.x, f_y.x));
    float n_10 = dot(g_10, vec2(f_x.y, f_y.y));
    float n_01 = dot(g_01, vec2(f_x.z, f_y.z));
    float n_11 = dot(g_11, vec2(f_x.w, f_y.w));

    vec2 fade_xy = fade(P_f.xy);
    vec2 n_x = mix(vec2(n_00, n_01), vec2(n_10, n_11), fade_xy.x);
    float n_xy = mix(n_x.x, n_x.y, fade_xy.y);

    return 2.3 * n_xy;
}

vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
    vec4 color1 = u_UserData0;
    vec4 color2 = u_UserData1;
    float scale = u_UserData2.x;
    vec4 colorDiff = (color2 - color1);

    float n1 = ((cnoise(texCoord * scale) + 1.0) / 2.0);

    return ((color1 + (colorDiff * n1)) * v_ColorScale);
}

void main()
{
    vec4 _masked = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
