$input v_TexCoord, v_ColorScale, v_UserData, fromPos, N

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: linearWipe

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);

// Time and data uniforms (packed in vec4 as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

// User data uniforms
uniform vec2 u_UserData0;
uniform vec4 u_UserData1;  // use .x for scalar
uniform vec4 u_UserData2;  // use .x for scalar
uniform vec4 u_UserData3;

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

// direction
// unitSmoothness
// unitProgress

void main()
{
// unitSmoothness is [ 0.0 .. 1.0 ].
	//
	//      0: hard.
	//      1: smooth.
	float unitSmoothness = u_UserData1;

	// "W" : A vector from the "fromPos" to the current fragment.
	//WE *SHOULD* BE ABLE TO MOVE THIS TO THE VERTEX SHADER!!!
	vec2 W = ( v_TexCoord.xy - fromPos );

	// "d" : The progress of "W" along "N".
	float d = dot( W, N );

	// Keep "d" within reasonable bounds.
	d = clamp( d, 0.0, unitSmoothness );

	// "progress" : The unitized progress of "d" along "V".
	float progress = ( d / unitSmoothness );

	// Get the colors to modulate.
	// We want to use v_TexCoord.xy instead of "translated_v_TexCoord.xy" here because
	// we want to sample the texture at its unmodified coordinates.
	// "translated_v_TexCoord.xy" is only useful to determine the transparency.
	vec4 color = ( texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale );

	#if 0 // For debugging ONLY.

		// Blend with a plain color only.
		// Diregard the texture color sampled above.
		color = v_ColorScale;

	#elif 0 // For debugging ONLY.

		// Use debug colors only.
		if( progress <= 0.0 )
		{
    gl_FragColor = vec4( 1.0, 0.0, 0.0, 1.0 );
		}
		else if( progress >= 1.0 )
		{
    gl_FragColor = vec4( 0.0, 1.0, 0.0, 1.0 );
		}
		else // if( ( progress > 0.0 ) && ( progress < 1.0 ) )
		{
    gl_FragColor = vec4( 0.0, 0.0, 1.0, 1.0 );
		}

	#endif

	// Linear interpolation between colors.
	// This multiplies-in the alpha.
    gl_FragColor = mix( color,;
				vec4( 0.0 ),
				progress );
}
