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

// Filter: bulge

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

void main()
{
float intensity = v_UserData.x;

	// Convert from Cartesian coordiates to polar coordinates.
	//
	// Reference:
	// http://en.wikipedia.org/wiki/Polar_coordinate_system#Converting_between_polar_and_Cartesian_coordinates

		// This is the same as:
		// vec2 V_from_center_to_fragment = ( v_TexCoord.xy - vec2( 0.5, 0.5 ) );
		// float radius = length( V_from_center_to_fragment );
		float radius = sqrt( ( v_TexCoord.xy.x - 0.5 ) * ( v_TexCoord.xy.x - 0.5 ) + ( v_TexCoord.xy.y - 0.5 ) * ( v_TexCoord.xy.y - 0.5 ) );

		// This is the same as:
		// float angle = atan2(V_from_center_to_fragment );
		float angle = atan2(v_TexCoord.xy.x - 0.5, v_TexCoord.xy.y - 0.5 );

	// Tweak.
	float length = pow( radius, intensity );

	// Convert from polar coordinates to Cartesian coordiates.
	float u = length * cos( angle ) + 0.5;
	float v = length * sin( angle ) + 0.5;

    vec4 _masked = texture2D( u_FillSampler0, vec2( v, u ) ) * v_ColorScale;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
