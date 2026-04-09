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

// Filter: scatter

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

// This function is duplicated in these:
//		kernel_filter_dissolve_gl.lua.
//		kernel_filter_random_gl.lua.
float rand( in vec2 seed )
{
	return fract( sin( dot( seed,
							vec2( 12.9898,
									78.233 ) ) ) * 43758.5453 );
}

void main()
{
vec2 rnd = vec2( rand( v_TexCoord.xy ),
								rand( v_TexCoord.xy.yx ));

	vec4 texColor = texture2D( u_FillSampler0,
										( v_TexCoord.xy + ( rnd * v_UserData.x * 0.25 ) ) );

    gl_FragColor = texColor * v_ColorScale;
 // FRAGMENT_SHADER_SUPPORTS_HIGHP
}
