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

// Filter: levels

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

#define GammaCorrection(color, gamma)								pow(color, vec3(1.0 / gamma))

#define LevelsControlInputRange(color, minInput, maxInput)				min(max(color - vec3(minInput), vec3(0.0)) / (vec3(maxInput) - vec3(minInput)), vec3(1.0))
#define LevelsControlInput(color, minInput, gamma, maxInput)				GammaCorrection(LevelsControlInputRange(color, minInput, maxInput), gamma)
#define LevelsControlOutputRange(color, minOutput, maxOutput) 			mix(vec3(minOutput), vec3(maxOutput), color)
#define LevelsControl(color, minInput, gamma, maxInput, minOutput, maxOutput) 	LevelsControlOutputRange(LevelsControlInput(color, minInput, gamma, maxInput), minOutput, maxOutput)

void main()
{
float white = v_UserData.x;
	float black = v_UserData.y;
	float gamma = v_UserData.z;

	vec4 color = texture2D(u_FillSampler0, v_TexCoord.xy);
    gl_FragColor = vec4( LevelsControl( color.rgb, black, gamma, white, 0.0, 1.0 ), color.a );
//	vec4 inPixel = texture2D(u_FillSampler0, v_TexCoord.xy);
//	vec4 color;
//	color.r = (pow(( (inPixel.r * 255.0) - black) / (white - black), gamma) ) / 255.0;
//	color.g = (pow(( (inPixel.g * 255.0) - black) / (white - black), gamma) ) / 255.0;
//	color.b = (pow(( (inPixel.b * 255.0) - black) / (white - black), gamma) ) / 255.0;
//	color.a = 1;
//	return color;
}
