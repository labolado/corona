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

// Filter: hue

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

vec3 rgb2hsv( vec3 c )
{
	vec4 K = vec4( 0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0 );
	vec4 p = mix( vec4( c.bg, K.wz ), vec4( c.gb, K.xy ), step( c.b, c.g ) );
	vec4 q = mix( vec4( p.xyw, c.r ), vec4( c.r, p.yzx ), step( p.x, c.r ) );

	float d = ( q.x - min( q.w, q.y ) );
	float e = 1.0e-10;

	return vec3( abs( q.z + ( q.w - q.y ) / ( 6.0 * d + e ) ), d / ( q.x + e ), q.x );
}

vec3 hsv2rgb( vec3 c )
{
	vec4 K = vec4( 1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0 );
	vec3 p = abs( fract( c.xxx + K.xyz ) * 6.0 - K.www );

	return c.z * mix( K.xxx, clamp( p - K.xxx, 0.0, 1.0 ), c.y );
}

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_TexCoord.z;
    if (q > 0.0) texCoord = texCoord / q;
// texColor has pre-multiplied alpha.
	vec4 texColor = texture2D( u_FillSampler0, texCoord );

	if (u_TexFlags.x > 0.5)
	    texColor = vec4(0.0, 0.0, 0.0, texColor.r);

	if ( texColor.a == 0.0 )
	{
    vec4 _masked = texColor;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
    return;
	}

	// Remove the alpha from the texColor.
	vec4 texColorWithoutPremultipliedAlpha = vec4( ( texColor.rgb / texColor.a ), texColor.a );

	//// Shift the hue.
	//
	vec3 hsv = rgb2hsv( texColorWithoutPremultipliedAlpha.rgb );

	hsv.r += ( v_UserData.x / -360.0 );

	vec3 rgb = hsv2rgb( hsv );
	//
	////

	// Re-apply the pre-multiplied alpha.
	vec4 rgbWithPremultipliedAlpha = vec4( rgb * texColor.a, texColor.a );

    vec4 _masked = rgbWithPremultipliedAlpha * v_ColorScale;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
