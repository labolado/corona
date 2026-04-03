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

// Filter: step

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);

// Time and data uniforms (packed in vec4 as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

// User data uniforms
uniform vec4 u_UserData0;  // use .x for scalar
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;  // use .x for scalar
uniform vec4 u_UserData3;

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

// lowerThreshold
// lowerColor
// higherThreshold
// higherColor

void main()
{
vec4 texColorResult;
	vec4 texColor = texture2D( u_FillSampler0, v_TexCoord.xy );
	vec4 colorDelta = ( u_UserData3 - u_UserData1 ); // higherColor - lowerColor.

	// This sets the color to lowerColor.
	// step( a, b ) = ( ( a <= b ) ? 1.0 : 0.0 ).
	texColorResult = ( u_UserData1 * step( u_UserData0, texColor.x ) );

	// This sets the color to higherColor.
	// step( a, b ) = ( ( a <= b ) ? 1.0 : 0.0 ).
	texColorResult += ( colorDelta * step( u_UserData2, texColor.x ) );

    gl_FragColor = texColorResult * v_ColorScale;
}
