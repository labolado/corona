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

// Filter: blurLinearHorizontal

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

const float kWeight0 = 0.2270270270;
const float kWeight1 = 0.3162162162;
const float kWeight2 = 0.0702702703;

void main()
{
vec4 color = texture2D(u_FillSampler0, v_TexCoord.xy.st) * kWeight0;

  color += texture2D(u_FillSampler0, (v_TexCoord.xy.st + vec2(v_UserData.x, 0.0) * u_TexelSize.xy)) * kWeight1;
  color += texture2D(u_FillSampler0, (v_TexCoord.xy.st - vec2(v_UserData.x, 0.0) * u_TexelSize.xy)) * kWeight1;
  color += texture2D(u_FillSampler0, (v_TexCoord.xy.st + vec2(v_UserData.y, 0.0) * u_TexelSize.xy)) * kWeight2;
  color += texture2D(u_FillSampler0, (v_TexCoord.xy.st - vec2(v_UserData.y, 0.0) * u_TexelSize.xy)) * kWeight2;

    gl_FragColor = color * v_ColorScale;
}
