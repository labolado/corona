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

// Solar2D Default Fragment Shader for bgfx

#include <bgfx_shader.sh>

// Sampler uniforms
SAMPLER2D(u_FillSampler0, 0);
SAMPLER2D(u_FillSampler1, 1);

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
#define CoronaSampler0 u_FillSampler0
#define CoronaSampler1 u_FillSampler1

void main()
{
    // Default fragment: texture color modulated by vertex color scale
    vec4 texColor = texture2D(u_FillSampler0, v_TexCoord.xy);
    gl_FragColor = texColor * v_ColorScale;
}
