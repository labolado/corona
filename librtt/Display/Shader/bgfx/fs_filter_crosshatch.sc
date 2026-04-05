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

// Filter: crosshatch

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

const float THRESHOLD = 1.0;

const vec3 kWeights = vec3( 0.2125, 0.7154, 0.0721 );

void main()
{
vec4 texColor = texture2D( u_FillSampler0, v_TexCoord.xy );

	float luminance = dot( texColor.rgb, kWeights );

	if( luminance < 1.0 )
	{
		if( mod( ( gl_FragCoord.x + gl_FragCoord.y ), floor(v_UserData.x) ) < THRESHOLD )
		{
    gl_FragColor = v_ColorScale;
		}
	}

	if( luminance < 0.75 )
	{
		if( mod( ( gl_FragCoord.x - gl_FragCoord.y ), floor(v_UserData.x) ) < THRESHOLD )
		{
    gl_FragColor = v_ColorScale;
		}
	}

	if( luminance < 0.50 )
	{
		if( mod( ( gl_FragCoord.x + gl_FragCoord.y - 5.0 ), floor(v_UserData.x) ) < THRESHOLD )
		{
    gl_FragColor = v_ColorScale;
		}
	}

	if( luminance < 0.25 )
	{
		if( mod( ( gl_FragCoord.x - gl_FragCoord.y - 5.0 ), floor(v_UserData.x) ) < THRESHOLD )
		{
    gl_FragColor = v_ColorScale;
		}
	}

    gl_FragColor = vec4( 0.0 );
}
