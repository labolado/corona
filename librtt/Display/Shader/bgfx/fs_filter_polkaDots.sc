$input v_TexCoord, v_ColorScale, v_UserData, slot_size, sample_uv_offset, minimum_full_radius

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: polkaDots

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
float aspectRatio = v_UserData.z;

	// aspectRatio = ( object.width / object.height )
	//WE *SHOULD* BE ABLE TO MOVE THIS TO THE VERTEX SHADER!!!
	vec2 tc = vec2( ( v_TexCoord.xy.x * aspectRatio ),
							v_TexCoord.xy.y );

	vec2 uv = ( sample_uv_offset + ( floor( tc / slot_size ) * slot_size ) );

	// Distance from the current pixel to the sampling point
	// (center of the circle).
	float dist = distance( tc, uv );

	//// Brightness.
	//
	// We want maximum brightness near the origin.
	// We want minimum brightness near the edges of the star.
	float unitized_dist = ( ( minimum_full_radius - dist ) / minimum_full_radius );

	// Use exponential ease-out to smooth the edges.
	// We could use "smoothstep()" instead, to have smooth edge of a
	// specific width.
	float brightness = ( 1.0 - pow( 2.0, ( -10.0 * unitized_dist ) ) );
	//
	////

	//// Visibility.
	//
	// step( a, b ) = ( ( a <= b ) ? 1.0 : 0.0 ).
	float visibility = step( dist, minimum_full_radius );
	//
	////

	vec4 color = texture2D( u_FillSampler0, uv );

    gl_FragColor = color * v_ColorScale * visibility * brightness;
}
