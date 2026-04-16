$input v_TexCoord, v_ColorScale, v_UserData, v_fromPos, v_N, v_MaskUV0, v_MaskUV1, v_MaskUV2

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

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_TexCoord.z;
    if (q > 0.0) texCoord = texCoord / q;
    float unitSmoothness = u_UserData1.x;

    vec2 W = (texCoord - v_fromPos);

    float d = dot(W, v_N);

    d = clamp(d, 0.0, unitSmoothness);

    float progress = (d / unitSmoothness);

    vec4 color = (texture2D(u_FillSampler0, texCoord) * v_ColorScale);

    #if 0
        color = v_ColorScale;
    #elif 0
        if (progress <= 0.0)
        {
            vec4 _masked = vec4(1.0, 0.0, 0.0, 1.0);
            if (u_TexFlags.y > 0.5)
                _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
            if (u_TexFlags.y > 1.5)
                _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
            if (u_TexFlags.y > 2.5)
                _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
            gl_FragColor = _masked;
        }
        else if (progress >= 1.0)
        {
            vec4 _masked = vec4(0.0, 1.0, 0.0, 1.0);
            if (u_TexFlags.y > 0.5)
                _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
            if (u_TexFlags.y > 1.5)
                _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
            if (u_TexFlags.y > 2.5)
                _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
            gl_FragColor = _masked;
        }
        else
        {
            vec4 _masked = vec4(0.0, 0.0, 1.0, 1.0);
            if (u_TexFlags.y > 0.5)
                _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
            if (u_TexFlags.y > 1.5)
                _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
            if (u_TexFlags.y > 2.5)
                _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
            gl_FragColor = _masked;
        }
    #endif

    vec4 _masked = mix(color, vec4(0.0), progress);
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
