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

// Filter: crystallize

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);
SAMPLER2D(u_MaskSampler0, 2);
SAMPLER2D(u_MaskSampler1, 3);
SAMPLER2D(u_MaskSampler2, 4);

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

// Texture flags: .x = 1.0 for alpha-only texture, .y = mask count (0..3)
uniform vec4 u_TexFlags;

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

const highp float FLT_MAX = 1e38;
/*
float unit_rand_1d( float p )
{
	return fract( sin( p ) * 43758.5453 );
}
*/
vec2 unit_rand_2d( in vec2 p )
{
	p = vec2( dot( p, vec2( 127.1, 311.7 ) ),
				dot( p, vec2( 269.5, 183.3 ) ) );

	return fract( sin( p ) * 43758.5453 );
}

vec2 get_voronoi_tc( in vec2 p,
							in float numTiles )
{
	vec2 n = floor(p * numTiles);
	vec2 f = fract(p * numTiles);

	vec2 seed;
	highp float min_dist_squared = FLT_MAX;

	for( int j=-1; j<=1; j++ )
	{
		for( int i=-1; i<=1; i++ )
		{
			vec2 constant_unit_offset = vec2(float(i),float(j));

			vec2 random_unit_offset = unit_rand_2d( n + constant_unit_offset );

			vec2 random_offset = ( constant_unit_offset + random_unit_offset );

			vec2 r = ( random_offset - f );

			highp float dist_squared = dot( r, r );

			#if 0

				// Branching version.

				if( dist_squared < min_dist_squared )
				{
					min_dist_squared = dist_squared;

					seed = ( n + random_offset);
				}

			#else

				// Branchless version.

				highp float useNewValue = step( dist_squared, min_dist_squared );
				highp float useOldValue = ( 1.0 - useNewValue );

				// useNewValue = 0 : min_dist_squared = min_dist_squared. (No change.)
				// useNewValue = 1 : min_dist_squared = dist_squared. (Update the minimum.)
				min_dist_squared = ( ( useNewValue * dist_squared ) +
										( useOldValue * min_dist_squared ) );

				// useNewValue = 0 : seed = seed. (No change.)
				// useNewValue = 1 : seed = ( n + random_offset). (Update the seed.)
				seed = ( ( useNewValue * ( n + random_offset) ) +
							( useOldValue * seed ) );

			#endif
		}
	}

	return ( seed * ( 1.0 / numTiles ) );
}

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_TexCoord.z;
    if (q > 0.0) texCoord = texCoord / q;
// Tile count. MUST be greater than 1.0 (ie: 2.0 or greater).
	float numTiles = v_UserData.x;

	vec2 tc = get_voronoi_tc( texCoord, numTiles );

	#if 0 // For debugging ONLY.

		// Return a solid color to represent the center of the crystallize.

		float voronoi_distance = distance( tc, texCoord );

		// We DON'T want this to be proportional to u_TexelSize because
		// we want the circle to be of a constant size, NOT proportional
		// to the texture resolution.

		if( voronoi_distance < ( 1.0 / ( numTiles * 16 ) ) )
		{
    vec4 _masked = vec4( 0.0, 0.0, 0.0, 1.0 );
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
		}
		else if( voronoi_distance < ( 1.0 / ( numTiles * 8 ) ) )
		{
    vec4 _masked = vec4( 1.0, 1.0, 1.0, 1.0 );
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
		}

	#endif

	vec4 color = texture2D( u_FillSampler0, tc );

    vec4 _masked = color * v_ColorScale;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
