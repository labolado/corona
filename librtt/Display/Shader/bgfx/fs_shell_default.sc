// SHELL_TEMPLATE — do not compile directly
// This file is concatenated with kernel .sc files by compile_shaders.sh
//
// Kernels must define:
//   vec4 FragmentKernel(vec2 texCoord, vec4 COLOR_SCALE, vec4 USER_DATA)
//
// Available to kernels:
//   u_FillSampler0, u_FillSampler1 (textures)
//   COLOR_SCALE, USER_DATA (passed as params from varyings)
//   CoronaColorScale(), CoronaVertexUserData, CoronaTotalTime, etc. (macros)
//   u_TexFlags.x > 0.5 means alpha-only texture (kernel handles swizzle)
//   u_UserData0..3 (extra uniform data)

$input v_TexCoord, v_ColorScale, v_UserData, v_MaskUV0, v_MaskUV1, v_MaskUV2

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);
SAMPLER2D(u_FillSampler1, 1);
SAMPLER2D(u_MaskSampler0, 2);
SAMPLER2D(u_MaskSampler1, 3);
SAMPLER2D(u_MaskSampler2, 4);

uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

uniform vec4 u_UserData0;
uniform vec4 u_UserData1;
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

// .x = 1.0 for alpha-only texture, .y = mask count (0..3)
uniform vec4 u_TexFlags;

#define CoronaColorScale(color) (COLOR_SCALE * (color))
#define CoronaVertexUserData USER_DATA
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy
#define CoronaSampler0 u_FillSampler0
#define CoronaSampler1 u_FillSampler1

// === KERNEL_PLACEHOLDER ===

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_UserData.w;
    if (q > 0.0) texCoord = texCoord / q;

    vec4 result = FragmentKernel(texCoord, v_ColorScale, v_UserData);

    // Mask sampling
    if (u_TexFlags.y > 0.5)
        result *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        result *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        result *= texture2D(u_MaskSampler2, v_MaskUV2).r;

    gl_FragColor = result;
}
