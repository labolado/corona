$input v_TexCoord, v_ColorScale, v_UserData, feathering_lower_edge, feathering_upper_edge

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: iris

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
uniform vec4 u_UserData3;  // use .x for scalar

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

// center
// unitOpenness
// aspectRatio
// unitSmoothness

void main()
{
// aspectRatio = ( object.width / object.height )
	float aspectRatio = u_UserData2;

	vec2 center = vec2( ( u_UserData0.x * aspectRatio ),
								u_UserData0.y );

	// Current fragment position in texture-space.
	//WE *SHOULD* BE ABLE TO MOVE THIS TO THE VERTEX SHADER!!!
	vec2 pos = vec2( ( v_TexCoord.xy.x * aspectRatio ),
								v_TexCoord.xy.y );

	// Distance from the center to the current fragment.
	float dist = distance( pos,
								center );

	// Get the colors to modulate.
	// We want to use v_TexCoord.xy instead of "pos" here because
	// we want to sample the texture at its unmodified coordinates.
	// "pos" is only useful to determine the transparency.
	vec4 color = ( texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale );

	#if 0 // For debugging ONLY.

		// Blend with a plain color only.
		// Diregard the texture color sampled above.
		color = v_ColorScale;

	#elif 0 // For debugging ONLY.

		// Use debug colors only.
		if( dist <= feathering_lower_edge )
		{
    gl_FragColor = vec4( 1.0, 0.0, 0.0, 1.0 );
		}
		else if( dist >= feathering_upper_edge )
		{
    gl_FragColor = vec4( 0.0, 1.0, 0.0, 1.0 );
		}
		else // if( ( dist > feathering_lower_edge ) && ( dist < feathering_upper_edge ) )
		{
    gl_FragColor = vec4( 0.0, 0.0, 1.0, 1.0 );
		}

	#endif

    gl_FragColor = ( color * smoothstep( feathering_lower_edge,;
									feathering_upper_edge,
									dist ) );
}
