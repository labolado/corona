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

// Filter: invert

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
vec4 texColor = texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale;

	// "texColor" has premultiplied alphas:
	//
	// texColor: ( ( r * a ), ( g * a ), ( b * a ), a )
	//
	// We only want to invert the RGB values, NOT the alpha.
	//
	// Therefore, we need to:
	//
	//		(1) Undo the effect of alpha on RGB.
	//		(2) Invert.
	//		(3) Reapply the alpha.
	//
	// In other words:
	//
	//		rgb_without_alpha = ( rgb / alpha );
	//		inverted_rgb_without_alpha = ( 1.0 - rgb_without_alpha );
	//		final_rgb_with_premultiplied_alpha = ( inverted_rgb_without_alpha * alpha );
	//
	//			Or:
	//
	//				vec3 result_rgb = ( ( 1.0 - ( rgb / a ) ) * a );
	//				vec3 result_rgb = ( a - rgb );
	//				vec4 result_rgba = ( ( a - rgb ), a );

    gl_FragColor = vec4( ( texColor.a - texColor.rgb ), texColor.a );
}
