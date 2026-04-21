$input v_TexCoord, v_ColorScale, v_UserData, v_opennessOffsetMatrix0, v_opennessOffsetMatrix1, v_feathering_edges_radians, v_MaskUV0, v_MaskUV1, v_MaskUV2

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: radialWipe

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

const float kPI = 3.14159265359;
const float kTWO_PI = (2.0 * kPI);

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_UserData.w;
    if (q > 0.0) texCoord = texCoord / q;
    vec2 center = u_UserData0.xy;

    mat2 opennessOffsetMatrix;
    opennessOffsetMatrix[0] = v_opennessOffsetMatrix0;
    opennessOffsetMatrix[1] = v_opennessOffsetMatrix1;

    vec2 rotated_texCoord = (opennessOffsetMatrix * texCoord);
    vec2 rotated_center = (opennessOffsetMatrix * center);

    vec2 V = (rotated_texCoord - rotated_center);

    float V_rotation_in_radians = (atan2(V.y, V.x) + kPI);

    vec4 color = texture2D(u_FillSampler0, texCoord);

    if (u_TexFlags.x > 0.5)
        color = vec4(0.0, 0.0, 0.0, color.r);

    color = color * v_ColorScale;

    #if 0
        color = v_ColorScale;
    #elif 0
        if (V_rotation_in_radians <= v_feathering_edges_radians.x)
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
        else if (V_rotation_in_radians >= v_feathering_edges_radians.y)
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

    vec4 _masked = (color * smoothstep(v_feathering_edges_radians.x, v_feathering_edges_radians.y, V_rotation_in_radians));
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
