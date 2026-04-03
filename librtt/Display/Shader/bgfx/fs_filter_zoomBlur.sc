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

// Filter: zoomBlur

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);

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

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

void main()
{
vec2 origin = vec2( v_UserData.x, v_UserData.y );
	float unitIntensity = v_UserData.z;

	vec2 samplingOffset = ( u_TexelSize.xy * (origin - v_TexCoord.xy) * ( 32.0 * unitIntensity ) );

	vec4 fragmentColor = texture2D(u_FillSampler0, v_TexCoord.xy) * 0.18;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy + samplingOffset) * 0.15;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy + (2.0 * samplingOffset)) *  0.12;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy + (3.0 * samplingOffset)) * 0.09;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy + (4.0 * samplingOffset)) * 0.05;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy - samplingOffset) * 0.15;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy - (2.0 * samplingOffset)) *  0.12;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy - (3.0 * samplingOffset)) * 0.09;
	fragmentColor += texture2D(u_FillSampler0, v_TexCoord.xy - (4.0 * samplingOffset)) * 0.05;

	//return ( texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale );
    gl_FragColor = fragmentColor;
}
