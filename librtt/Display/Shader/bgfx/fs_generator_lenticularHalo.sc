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

// Solar2D Generator Fragment Shader: lenticularHalo

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
    float f_intermediate3 = (sin(noise(f_intermediate2) * 32.0) * 0.5);

    f0 += (f0 * (f_intermediate3 + (dist * 0.1) + 0.0));

    return f0;
}

float Hue_2_RGB(float v1, float v2, float vH)
{
    float ret;

    if (vH < 0.0)
        vH += 1.0;

    if (vH > 1.0)
        vH -= 1.0;

    if ((6.0 * vH) < 1.0)
        ret = (v1 + (v2 - v1) * 6.0 * vH);
    else if ((2.0 * vH) < 1.0)
        ret = (v2);
    else if ((3.0 * vH) < 2.0)
        ret = (v1 + (v2 - v1) * ((2.0 / 3.0) - vH) * 6.0);
    else
        ret = v1;

    return ret;
}

vec3 shift_hue(in vec3 input_color, in float hue_shift_in_degrees)
{
    float Cmax, Cmin;
    float D;
    float H, S, L;
    float R, G, B;

    R = input_color.r;
    G = input_color.g;
    B = input_color.b;

    Cmax = max(R, max(G, B));
    Cmin = min(R, min(G, B));

    L = (Cmax + Cmin) / 2.0;

    if (Cmax == Cmin)
    {
        H = 0.0;
        S = 0.0;
    }
    else
    {
        D = Cmax - Cmin;

        if (L < 0.5)
        {
            S = D / (Cmax + Cmin);
        }
        else
        {
            S = D / (2.0 - (Cmax + Cmin));
        }

        if (R == Cmax)
        {
            H = (G - B) / D;
        }
        else if (G == Cmax)
        {
            H = 2.0 + (B - R) / D;
        }
        else
        {
            H = 4.0 + (R - G) / D;
        }

        H = H / 6.0;
    }

    float hue_shift_in_radians = radians(hue_shift_in_degrees);
    H += hue_shift_in_radians;

    #if 0
        S = 1.0;
        L = 0.8;
    #endif

    if (H < 0.0)
    {
        H = H + 1.0;
    }

    H = clamp(H, 0.0, 1.0);
    S = clamp(S, 0.0, 1.0);
    L = clamp(L, 0.0, 1.0);

    float var_2, var_1;

    if (S == 0.0)
    {
        R = L;
        G = L;
        B = L;
    }
    else
    {
        if (L < 0.5)
        {
            var_2 = L * (1.0 + S);
        }
        else
        {
            var_2 = (L + S) - (S * L);
        }

        var_1 = 2.0 * L - var_2;

        R = Hue_2_RGB(var_1, var_2, H + (1.0 / 3.0));
        G = Hue_2_RGB(var_1, var_2, H);
        B = Hue_2_RGB(var_1, var_2, H - (1.0 / 3.0));
    }

    return vec3(R, G, B);
}

vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
    float aspectRatio = v_UserData.z;

    vec2 position = vec2((v_UserData.x * aspectRatio), v_UserData.y);

    vec2 tc = vec2((texCoord.x * aspectRatio), texCoord.y);

    float seed = v_UserData.w;

    float intensity = sunbeam(seed, tc, position);

    float d = distance(tc, position);

    const float MAX_DIST = 1.4142135623730951;
    intensity -= pow(((4.0 * d) - MAX_DIST), 2.0);
    intensity = max(intensity, 0.0);

    vec3 start_color = vec3(1.0, 0.0, 0.0);

    float hue_shift_in_degrees = ((max((d - 0.1), 0.0) / MAX_DIST) * 1.4 * -180.0);

    vec3 color = shift_hue(start_color, hue_shift_in_degrees);

    #if 0
        return vec4(color, 1.0);
    #else
        return vec4(color, 1.0) * intensity;
    #endif
}

void main()
{
    gl_FragColor = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
}
