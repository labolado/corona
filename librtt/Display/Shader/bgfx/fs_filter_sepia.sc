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

// Filter: sepia

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
const vec3 LUMINANCE_WEIGHTS = vec3( 0.22, 0.707, 0.071 );
	const vec3 lightColor = vec3( 1.0, 0.9, 0.5 );
	const vec3 darkColor = vec3( 0.2, 0.05, 0.0 );

	vec4 texColor = texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale;
	float luminance = dot( LUMINANCE_WEIGHTS, texColor.xyz );
	vec3 sepia = lightColor * luminance + ( -darkColor * luminance + darkColor );

	vec3 result = mix( texColor.rgb, sepia.rgb, v_UserData.x );

	// Pre-multiply alpha.
	result.rgb *= texColor.a;

    gl_FragColor = vec4( result.rgb, texColor.a );
}
