$input v_TexCoord, v_ColorScale, v_UserData, slot_size, sample_uv_offset, transform0, transform1, transform2

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: opTile

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

// Some Android devices AREN'T able to use "varying mat3".

void main()
{
float scale = v_UserData.z;

	////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////

	mat3 transform;
	transform[ 0 ] = transform0;
	transform[ 1 ] = transform1;
	transform[ 2 ] = transform2;

	// Apply the "angle" parameter.
	//WE *SHOULD* BE ABLE TO MOVE THIS TO THE VERTEX SHADER!!!
	vec2 tc = ( transform * vec3( v_TexCoord.xy, 1.0 ) ).xy;

	// IDEA: WE COULD ONLY ROTATE THE CENTRAL SAMPLING POINT,
	// AND KEEP THE ADJACENT PIXELS SAMPLED AXIS-ALIGNED!!!!!
	vec2 center_uv = ( sample_uv_offset + ( floor( tc / slot_size ) * slot_size ) );

	// Apply the "scale" parameter.
	vec2 uv = mix( center_uv, tc, scale );

    gl_FragColor = texture2D( u_FillSampler0, uv ) * v_ColorScale;
}
