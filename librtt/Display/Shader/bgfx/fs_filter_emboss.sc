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

// Filter: emboss

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
vec4 sample0 = texture2D( u_FillSampler0, v_TexCoord.xy - u_TexelSize.xy );
	vec4 sample1 = texture2D( u_FillSampler0, v_TexCoord.xy + u_TexelSize.xy );

	vec4 result = vec4( 0.5, 0.5, 0.5, ( ( sample0.a + sample1.a ) * 0.5 ) );
	result.rgb -= sample0.rgb * 5.0 * v_UserData.x;
	result.rgb += sample1.rgb * 5.0 * v_UserData.x;
	result.rgb = vec3( ( result.r + result.g + result.b ) * ( 1.0 / 3.0 ) );

	// Pre-multiply alpha.
	result.rgb *= result.a;

    gl_FragColor = result * v_ColorScale;
}
