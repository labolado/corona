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

// Solar2D Generator Fragment Shader: random

#include <bgfx_shader.sh>

SAMPLER2D(u_MaskSampler0, 2);
SAMPLER2D(u_MaskSampler1, 3);
SAMPLER2D(u_MaskSampler2, 4);

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

// Texture flags: .x = 1.0 for alpha-only texture, .y = mask count (0..3)
uniform vec4 u_TexFlags;

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

// This function is duplicated in these:
//		kernel_filter_dissolve_gl.lua.
//		kernel_filter_scatter_gl.lua.
 float rand(vec2 seed)
{
	return fract(sin(dot(seed,
							vec2(12.9898,
									78.233))) * 43758.5453);
}

 vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
	 float time = fract(u_TotalTime.x);

	 float v0 = rand(vec2((time + texCoord.x),
									(time + texCoord.y)));

	#if 0

		// Grayscale.

		return (vec4(vec3(v0), 1.0) * v_ColorScale);

	#else

		// Any color.

		 float v1 = rand(vec2((time + texCoord.x + u_TexelSize.x),
									(time + texCoord.y)));

		 float v2 = rand(vec2((time + texCoord.x),
									(time + texCoord.y + u_TexelSize.y)));

		return (vec4(v0, v1, v2, 1.0) * v_ColorScale);

	#endif
}

void main()
{
    vec4 _masked = FragmentKernel(v_TexCoord.xy, v_ColorScale, v_UserData);
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
