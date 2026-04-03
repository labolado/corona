$input v_TexCoord, v_ColorScale, v_UserData, v_opennessOffsetMatrix0, v_opennessOffsetMatrix1, v_feathering_edges_radians

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

const float kPI = 3.14159265359;
const float kTWO_PI = (2.0 * kPI);

void main()
{
    vec2 center = u_UserData0.xy;

    mat2 opennessOffsetMatrix;
    opennessOffsetMatrix[0] = v_opennessOffsetMatrix0;
    opennessOffsetMatrix[1] = v_opennessOffsetMatrix1;

    vec2 rotated_texCoord = (opennessOffsetMatrix * v_TexCoord.xy);
    vec2 rotated_center = (opennessOffsetMatrix * center);

    vec2 V = (rotated_texCoord - rotated_center);

    float V_rotation_in_radians = (atan2(V.y, V.x) + kPI);

    vec4 color = (texture2D(u_FillSampler0, v_TexCoord.xy) * v_ColorScale);

    #if 0
        color = v_ColorScale;
    #elif 0
        if (V_rotation_in_radians <= v_feathering_edges_radians.x)
        {
            gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
        else if (V_rotation_in_radians >= v_feathering_edges_radians.y)
        {
            gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
        else
        {
            gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);
        }
    #endif

    gl_FragColor = (color * smoothstep(v_feathering_edges_radians.x, v_feathering_edges_radians.y, V_rotation_in_radians));
}
