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

// Filter: sobel

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
// This is our 3 x 3 kernel.
	vec4 samples[9];

	samples[ 0 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(-0.0028125, 0.0028125) );
	samples[ 1 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(0.00, 0.0028125) );
	samples[ 2 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(0.0028125, 0.0028125) );
	samples[ 3 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(-0.0028125, 0.00 ) );
	samples[ 4 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st ); // This will drive our alpha.
	samples[ 5 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(0.0028125, 0.0028125) );
	samples[ 6 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(-0.0028125, -0.0028125) );
	samples[ 7 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(0.00, -0.0028125) );
	samples[ 8 ] = texture2D( u_FillSampler0, v_TexCoord.xy.st + vec2(0.0028125, -0.0028125) );

	// Horizontal and vertical weighting.
	//
	//     -1 -2 -1       1  0 -1
	// H =  0  0  0  V =  2  0 -2
	//      1  2  1       1  0 -1
	vec4 horiz_edge = samples[2] + ( 2.0 * samples[5]) + samples[8] - (samples[0] + (2.0*samples[3]) + samples[6]);
	vec4 vert_edge = samples[0] + ( 2.0 * samples[1]) + samples[2] - (samples[6] + (2.0*samples[7]) + samples[8]);

    gl_FragColor = vec4( sqrt((horiz_edge.rgb * horiz_edge.rgb) + (vert_edge.rgb * vert_edge.rgb)), samples[ 4 ].a ) * v_ColorScale;
}
