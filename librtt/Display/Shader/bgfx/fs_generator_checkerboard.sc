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

// Solar2D Generator Fragment Shader: checkerboard

#include <bgfx_shader.sh>

// Time and data uniforms
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

 vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
	 vec4 color1 = u_UserData0;
	 vec4 color2 = u_UserData1;
	 float xStep = u_UserData2;
	 float yStep = u_UserData3;

	// "xStep" is the number of time the pattern is repeated by texCoord.x.
	// "yStep" is the number of time the pattern is repeated by texCoord.y.
	//
	// If "xStep" is 1.0, then the pattern will follow the natural progression
	// of texCoord.x, along the X axis, only once.

	 float total = (floor(texCoord.x * xStep) +
							floor(texCoord.y * yStep));

	bool is_even = (mod(total, 2.0) < 0.001);

	return ((is_even ? color1 : color2) * v_ColorScale);
}

void main()
{
    gl_FragColor = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
}
