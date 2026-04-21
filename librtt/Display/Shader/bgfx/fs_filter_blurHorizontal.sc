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

// Filter: blurHorizontal

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

const float kPI = 3.14159265359;

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_UserData.w;
    if (q > 0.0) texCoord = texCoord / q;
float blurSize = v_UserData.x;
	float sigma = v_UserData.y;

	float num_blur_pixels_per_side = ( floor( blurSize ) / 2.0 );

	// Direction.
	vec2 blur_multiply_dir = vec2(1.0, 0.0);

	vec3 incremental_gaussian;
	incremental_gaussian.x = 1.0 / (sqrt(2.0 * kPI) * sigma);
	incremental_gaussian.y = exp(-0.5 / (sigma * sigma));
	incremental_gaussian.z = incremental_gaussian.y * incremental_gaussian.y;

	vec4 avg_value = vec4(0.0, 0.0, 0.0, 0.0);
	float coefficient_sum = 0.0;

	// Center.
	avg_value += texture2D(u_FillSampler0, texCoord.st) * incremental_gaussian.x;
	coefficient_sum += incremental_gaussian.x;
	incremental_gaussian.xy *= incremental_gaussian.yz;

	// Sample on each side of the center.
	for( float i = 1.0;
			i <= 64.0; i++) // 64: This is half the blurSize.
	{ 
		if( i > num_blur_pixels_per_side )
		{
			break;
		}

		avg_value += texture2D(u_FillSampler0, texCoord.st - i * u_TexelSize.xy *
								blur_multiply_dir) * incremental_gaussian.x;
		avg_value += texture2D(u_FillSampler0, texCoord.st + i * u_TexelSize.xy *
								blur_multiply_dir) * incremental_gaussian.x;
		coefficient_sum += 2.0 * incremental_gaussian.x;
		incremental_gaussian.xy *= incremental_gaussian.yz;
	}

    vec4 result = avg_value / coefficient_sum;
    if (u_TexFlags.x > 0.5)
        result = vec4(0.0, 0.0, 0.0, result.r);
    vec4 _masked = result * v_ColorScale;
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
