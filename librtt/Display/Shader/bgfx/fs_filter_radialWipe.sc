$input v_TexCoord, v_ColorScale, v_UserData, opennessOffsetMatrix0, opennessOffsetMatrix1, feathering_lower_edge_in_radians, feathering_upper_edge_in_radians

//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// Filter: radialWipe

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
// unitOpennessOffset
// unitSmoothness

// Some Android devices AREN'T able to use "varying mat2".
// 2D rotation matrix (See unitOpennessOffset).
// 2D rotation matrix (See unitOpennessOffset).

const float kPI = 3.14159265359;
const float kTWO_PI = ( 2.0 * kPI );

void main()
{
vec2 center = u_UserData0;

	//WE *SHOULD* BE ABLE TO MOVE THIS TO THE VERTEX SHADER!!!

		// Rotate by unitOpennessOffset.
		mat2 opennessOffsetMatrix;
		opennessOffsetMatrix[ 0 ] = opennessOffsetMatrix0;
		opennessOffsetMatrix[ 1 ] = opennessOffsetMatrix1;

		vec2 rotated_v_TexCoord.xy = ( opennessOffsetMatrix * v_TexCoord.xy );
		vec2 rotated_center = ( opennessOffsetMatrix * center );

		// Vector to the current fragment. This is used to find the angle between
		// the trigonometric circle's origin and this vector. The angle is then
		// used to select a gradient color.
		vec2 V = ( rotated_v_TexCoord.xy - rotated_center );

		// Get the rotation of "V" from the origin.
		//
		//		tan( theta ) = ( opposite / adjacent ).
		//		tan( theta ) = ( V.y / V.x ).
		//
		// Therefore:
		//
		//		theta = atan2(tan( theta ) ).
		//		theta = atan2(V.y / V.x ).
		//
		// "theta" is the angle, in radians, between "V" and the rotation origin.
		//
		// Reference:
		// http://en.wikipedia.org/wiki/Polar_coordinate_system#Converting_between_polar_and_Cartesian_coordinates

		float V_rotation_in_radians = ( atan( V.y, V.x ) + kPI );

	// Get the colors to modulate.
	// We want to use v_TexCoord.xy instead of rotated_v_TexCoord.xy here because
	// we want to sample the texture at its unmodified coordinates.
	// rotated_v_TexCoord.xy is only useful to determine the transparency.
	vec4 color = ( texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale );

	#if 0 // For debugging ONLY.

		// Blend with a plain color only.
		// Diregard the texture color sampled above.
		color = v_ColorScale;

	#elif 0 // For debugging ONLY.

		// Use debug colors only.
		if( V_rotation_in_radians <= feathering_lower_edge_in_radians )
		{
    gl_FragColor = vec4( 1.0, 0.0, 0.0, 1.0 );
		}
		else if( V_rotation_in_radians >= feathering_upper_edge_in_radians )
		{
    gl_FragColor = vec4( 0.0, 1.0, 0.0, 1.0 );
		}
		else // if( ( V_rotation_in_radians > feathering_lower_edge_in_radians ) && ( V_rotation_in_radians < feathering_upper_edge_in_radians ) )
		{
    gl_FragColor = vec4( 0.0, 0.0, 1.0, 1.0 );
		}

	#endif

	// Set the transparency based on the angle of rotation from the origin.
	// This multiplies-in the alpha.
    gl_FragColor = ( color * smoothstep( feathering_lower_edge_in_radians,;
									feathering_upper_edge_in_radians,
									V_rotation_in_radians ) );
}
