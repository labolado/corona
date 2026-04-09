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

// Filter: monotone

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

const vec3 kWeights = vec3( 0.2125, 0.7154, 0.0721 );

void main()
{
vec4 texColor = texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale;

    float luminance = dot( texColor.rgb, kWeights );
	
    vec4 desat = vec4( vec3( luminance ), 1.0 );
	
	// overlay
    vec4 outputColor = vec4(
        (desat.r < 0.5 ? (2.0 * desat.r * v_UserData.r) : (1.0 - 2.0 * (1.0 - desat.r) * (1.0 - v_UserData.r))),
        (desat.g < 0.5 ? (2.0 * desat.g * v_UserData.g) : (1.0 - 2.0 * (1.0 - desat.g) * (1.0 - v_UserData.g))),
        (desat.b < 0.5 ? (2.0 * desat.b * v_UserData.b) : (1.0 - 2.0 * (1.0 - desat.b) * (1.0 - v_UserData.b))),
        1.0
    );

	float intensity = v_UserData.a;

    gl_FragColor = vec4( mix(texColor.rgb, outputColor.rgb, intensity), texColor.a);
}
