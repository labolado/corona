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

// Filter: median

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

#define s2(a, b)				temp = a; a = min(a, b); b = max(temp, b);
#define mn3(a, b, c)			s2(a, b); s2(a, c);
#define mx3(a, b, c)			s2(b, c); s2(a, c);

#define mnmx3(a, b, c)			mx3(a, b, c); s2(a, b);                                   // 3 exchanges
#define mnmx4(a, b, c, d)		s2(a, b); s2(c, d); s2(a, c); s2(b, d);                   // 4 exchanges
#define mnmx5(a, b, c, d, e)	s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e);           // 6 exchanges
#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_TexCoord.z;
    if (q > 0.0) texCoord = texCoord / q;
vec2 leftTextureCoordinate = texCoord + vec2( - u_TexelSize.x, 0.0 );
	vec2 rightTextureCoordinate = texCoord + vec2( u_TexelSize.x, 0.0 );

	vec2 topTextureCoordinate = texCoord + vec2( 0.0, - u_TexelSize.y );
	vec2 topLeftTextureCoordinate = texCoord + vec2( - u_TexelSize.x, - u_TexelSize.y );
	vec2 topRightTextureCoordinate = texCoord + vec2( u_TexelSize.x, - u_TexelSize.y );

	vec2 bottomTextureCoordinate = texCoord + vec2( 0.0, u_TexelSize.y );
	vec2 bottomLeftTextureCoordinate = texCoord + vec2( - u_TexelSize.x, u_TexelSize.y );
	vec2 bottomRightTextureCoordinate = texCoord + vec2( u_TexelSize.x, u_TexelSize.y );

	vec3 v[6];

	v[0] = texture2D(u_FillSampler0, bottomLeftTextureCoordinate).rgb;
	v[1] = texture2D(u_FillSampler0, topRightTextureCoordinate).rgb;
	v[2] = texture2D(u_FillSampler0, topLeftTextureCoordinate).rgb;
	v[3] = texture2D(u_FillSampler0, bottomRightTextureCoordinate).rgb;
	v[4] = texture2D(u_FillSampler0, leftTextureCoordinate).rgb;
	v[5] = texture2D(u_FillSampler0, rightTextureCoordinate).rgb;

	vec3 temp;

	mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]);

	v[5] = texture2D(u_FillSampler0, bottomTextureCoordinate).rgb;
	mnmx5(v[1], v[2], v[3], v[4], v[5]);

	v[5] = texture2D(u_FillSampler0, topTextureCoordinate).rgb;
	mnmx4(v[2], v[3], v[4], v[5]);

	vec4 middle_sample;
	middle_sample = texture2D(u_FillSampler0, texCoord);

	if (u_TexFlags.x > 0.5)
	    middle_sample = vec4(0.0, 0.0, 0.0, middle_sample.r);

	v[5] = middle_sample.rgb;
	mnmx3(v[3], v[4], v[5]);

    vec4 _masked = vec4(v[4], middle_sample.a);
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    _masked *= v_ColorScale;
    gl_FragColor = _masked;
}
