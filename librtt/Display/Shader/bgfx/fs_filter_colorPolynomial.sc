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

// Filter: colorPolynomial

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);

// Time and data uniforms (packed in vec4 as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

// User data uniforms
uniform mat4 u_UserData0;
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

// coefficients

void main()
{
mat4 coefficients = u_UserData0;

	vec4 input_color = texture2D( u_FillSampler0, v_TexCoord.xy );

	vec4 redCoefficients = coefficients[ 0 ];
	vec4 greenCoefficients = coefficients[ 1 ];
	vec4 blueCoefficients = coefficients[ 2 ];
	vec4 alphaCoefficients = coefficients[ 3 ];

	float r = ( redCoefficients.x +
					( redCoefficients.y * input_color.r ) +
					( redCoefficients.z * input_color.r * input_color.r ) +
					( redCoefficients.w * input_color.r * input_color.r * input_color.r ) );

	float g = ( greenCoefficients.x +
						( greenCoefficients.y * input_color.g ) +
						( greenCoefficients.z * input_color.g * input_color.g ) +
						( greenCoefficients.w * input_color.g * input_color.g * input_color.g ) );

	float b = ( blueCoefficients.x +
						( blueCoefficients.y * input_color.b ) +
						( blueCoefficients.z * input_color.b * input_color.b ) +
						( blueCoefficients.w * input_color.b * input_color.b * input_color.b ) );

	float a = ( alphaCoefficients.x +
						( alphaCoefficients.y * input_color.a ) +
						( alphaCoefficients.z * input_color.a * input_color.a ) +
						( alphaCoefficients.w * input_color.a * input_color.a * input_color.a ) );

    gl_FragColor = vec4( r, g, b, a ) * v_ColorScale;
}
