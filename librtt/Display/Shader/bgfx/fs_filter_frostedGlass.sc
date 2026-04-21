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

// Filter: frostedGlass

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

vec4 mod289( in vec4 x )
{
    return ( x - floor( x * ( 1.0 / 289.0 ) ) * 289.0 );
}
 
vec4 permute( in vec4 x )
{
    return mod289( ( ( x * 34.0 ) + 1.0 ) * x );
}

vec4 taylorInvSqrt( in vec4 r )
{
    return ( 1.79284291400159 - ( 0.85373472095314 * r ) );
}
 
vec2 fade( in vec2 t )
{
    return ( t * t * t * ( t * ( t * 6.0 - 15.0 ) + 10.0 ) );
}

// This function is duplicated in these:
//		kernel_filter_marble_gl.lua
//		kernel_filter_perlinNoise_gl.lua
// Classic Perlin noise
float cnoise( in vec2 P )
{
    vec4 P_i = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 P_f = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    P_i = mod289(P_i); // To avoid truncation effects in permutation
    vec4 i_x = P_i.xzxz;
    vec4 i_y = P_i.yyww;
    vec4 f_x = P_f.xzxz;
    vec4 f_y = P_f.yyww;
     
    vec4 i = permute(permute(i_x) + i_y);
     
    vec4 g_x = fract(i * (1.0 / 41.0)) * 2.0 - 1.0 ;
    vec4 g_y = abs(g_x) - 0.5;
    vec4 t_x = floor(g_x + 0.5);
    g_x = g_x - t_x;

    vec2 g_00 = vec2(g_x.x,g_y.x);
    vec2 g_10 = vec2(g_x.y,g_y.y);
    vec2 g_01 = vec2(g_x.z,g_y.z);
    vec2 g_11 = vec2(g_x.w,g_y.w);
     
	vec4 norm = taylorInvSqrt( vec4( dot( g_00, g_00 ),
											dot( g_01, g_01 ),
											dot( g_10, g_10 ),
											dot( g_11, g_11 ) ) );
    g_00 *= norm.x;  
    g_01 *= norm.y;  
    g_10 *= norm.z;  
    g_11 *= norm.w;  
     
    float n_00 = dot(g_00, vec2(f_x.x, f_y.x));
    float n_10 = dot(g_10, vec2(f_x.y, f_y.y));
    float n_01 = dot(g_01, vec2(f_x.z, f_y.z));
    float n_11 = dot(g_11, vec2(f_x.w, f_y.w));
     
    vec2 fade_xy = fade(P_f.xy);
    vec2 n_x = mix(vec2(n_00, n_01), vec2(n_10, n_11), fade_xy.x);
    float n_xy = mix(n_x.x, n_x.y, fade_xy.y);

    return 2.3 * n_xy;
}

vec3 get_fragment_normal( in vec2 texCoord,
								in vec4 texelSize,
								in float scale )
{
	// We're generating the fragment normal from 3 height values
	// sampled from a Perlin-noise-based height map.

	vec2 texCoord0 = texCoord;
	vec2 texCoord1 = texCoord + vec2( texelSize.x, 0.0 );
	vec2 texCoord2 = texCoord + vec2( 0.0, texelSize.y );

	float height0 = ( ( cnoise( texCoord0 * scale ) + 1.0 ) * 0.5 );
	float height1 = ( ( cnoise( texCoord1 * scale ) + 1.0 ) * 0.5 );
	float height2 = ( ( cnoise( texCoord2 * scale ) + 1.0 ) * 0.5 );

	vec3 v0 = vec3( texCoord0, height0 );
	vec3 v1 = vec3( texCoord1, height1 );
	vec3 v2 = vec3( texCoord2, height2 );

	vec3 n = cross( ( v1 - v0 ),
							( v2 - v0 ) );

	return normalize( n );
}

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_UserData.w;
    if (q > 0.0) texCoord = texCoord / q;
float scale = v_UserData.x;

	vec3 n = get_fragment_normal( texCoord,
											u_TexelSize,
											scale );

	vec3 incident = vec3( 0.0, 0.0, 1.0 );
	float intensity = dot( n, incident );

	#if 0 // For debugging ONLY.

		// This will repeat the texture hozirontally,
		// and scroll it from right to left.
		texCoord.x = fract( texCoord.x + ( u_TotalTime * 0.1 ) );

	#endif

	// This ISN'T what we want.
	//vec3 r = refract( vec3( 0.0, 0.0, 1.0 ), n, 1.3330 );

	//! TOFIX: We SHOULDN'T hard code "0.01".
	vec4 texColor = texture2D( u_FillSampler0, texCoord + ( n.xy * 0.01 ) );

	if (u_TexFlags.x > 0.5)
	    texColor = vec4(0.0, 0.0, 0.0, texColor.r);

	#if 0 // Experiment.

    vec4 _masked = vec4( ( texColor.rgb * ( 0.5 + intensity ) ), 1.0 ) * v_ColorScale;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;

	#endif

    vec4 _masked = texColor;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
