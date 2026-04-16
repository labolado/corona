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

// Filter: zoomBlur

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
    float q = v_TexCoord.z;
    if (q > 0.0) texCoord = texCoord / q;
vec2 origin = vec2( v_UserData.x, v_UserData.y );
	float unitIntensity = v_UserData.z;

	vec2 samplingOffset = ( u_TexelSize.xy * (origin - texCoord) * ( 32.0 * unitIntensity ) );

	vec4 fragmentColor = texture2D(u_FillSampler0, texCoord) * 0.18;
	fragmentColor += texture2D(u_FillSampler0, texCoord + samplingOffset) * 0.15;
	fragmentColor += texture2D(u_FillSampler0, texCoord + (2.0 * samplingOffset)) *  0.12;
	fragmentColor += texture2D(u_FillSampler0, texCoord + (3.0 * samplingOffset)) * 0.09;
	fragmentColor += texture2D(u_FillSampler0, texCoord + (4.0 * samplingOffset)) * 0.05;
	fragmentColor += texture2D(u_FillSampler0, texCoord - samplingOffset) * 0.15;
	fragmentColor += texture2D(u_FillSampler0, texCoord - (2.0 * samplingOffset)) *  0.12;
	fragmentColor += texture2D(u_FillSampler0, texCoord - (3.0 * samplingOffset)) * 0.09;
	fragmentColor += texture2D(u_FillSampler0, texCoord - (4.0 * samplingOffset)) * 0.05;

	//return ( texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale );
    vec4 _masked = fragmentColor;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
