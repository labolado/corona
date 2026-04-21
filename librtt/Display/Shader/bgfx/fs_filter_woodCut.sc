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

// Filter: woodCut

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);
SAMPLER2D(u_MaskSampler0, 2);
SAMPLER2D(u_MaskSampler1, 3);
SAMPLER2D(u_MaskSampler2, 4);

// Time and data uniforms (packed in vec4 as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

// User data uniforms
uniform vec4 u_UserData0;
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

// Texture flags: .x = 1.0 for alpha-only texture, .y = mask count (0..3)
uniform vec4 u_TexFlags;

// Solar2D macros for shader compatibility
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
    float q = v_UserData.w;
    if (q > 0.0) texCoord = texCoord / q;
vec2 o_x = vec2(u_TexelSize.x, 0.0);
	vec2 o_y = vec2(0.0, u_TexelSize.y);
	vec2 pp = texCoord - o_y;
	float t00 = texture2D(u_FillSampler0, pp - o_x).x;
	float t01 = texture2D(u_FillSampler0, pp).x;
	float t02 = texture2D(u_FillSampler0, pp + o_x).x;
	pp = texCoord;
	float t10 = texture2D(u_FillSampler0, pp - o_x).x;

	float t12 = texture2D(u_FillSampler0, pp + o_x).x;
	pp = texCoord + o_y;
	float t20 = texture2D(u_FillSampler0, pp - o_x).x;
	float t21 = texture2D(u_FillSampler0, pp).x;
	float t22 = texture2D(u_FillSampler0, pp + o_x).x;
	float s_x = t20 + t22 - t00 - t02 + 2.0 * (t21 - t01);
	float s_y = t22 + t02 - t00 - t20 + 2.0 * (t12 - t10);
	float dist = (s_x * s_x + s_y * s_y);

	// The result is mostly black, with white highlights.
	// step( a, b ) = ( ( a <= b ) ? 1.0 : 0.0 ).
    vec4 _masked = vec4( step( (v_UserData.x * v_UserData.x), dist ) ) * v_ColorScale;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
