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

// Filter: swirl

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
const float radius = 1.0;

    vec2 uv = v_TexCoord.xy - vec2( 0.5, 0.5 );
    float dist = length( uv );

	// step( a, b ) = ( ( a <= b ) ? 1.0 : 0.0 ).
	float useDistLessThanRadius = step( dist, radius );
	float useDistNotLessThanRadius = ( 1.0 - useDistLessThanRadius );

	float percent = (radius - dist) / radius;
	float theta = percent * percent * v_UserData.x * 8.0;
	float s = sin( theta );
	float c = cos( theta );

	vec2 resultDistLessThanRadius = ( vec2( dot( uv, vec2( c, -s ) ),
													dot( uv, vec2( s,  c ) ) ) +
											vec2( 0.5, 0.5 ) );
	vec2 resultDistNotLessThanRadius = v_TexCoord.xy;

    uv = ( ( useDistLessThanRadius * resultDistLessThanRadius ) +
			( useDistNotLessThanRadius * resultDistNotLessThanRadius ) );

    gl_FragColor = texture2D( u_FillSampler0, uv ) * v_ColorScale;
}
