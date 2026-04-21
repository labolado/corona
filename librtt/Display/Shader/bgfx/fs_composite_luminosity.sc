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

// Solar2D Composite Fragment Shader: luminosity

#include <bgfx_shader.sh>

// Sampler uniforms (composite uses two textures)
SAMPLER2D(u_FillSampler0, 0);
SAMPLER2D(u_FillSampler1, 1);
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
#define CoronaSampler0 u_FillSampler0
#define CoronaSampler1 u_FillSampler1

const vec4 kWeights = vec4(0.2125, 0.7154, 0.0721, 1.0);

 vec4 FragmentKernel(vec2 texCoord, vec4 v_ColorScale, vec4 v_UserData)
{
 vec4 base = texture2D(u_FillSampler0, texCoord);

 if (u_TexFlags.x > 0.5)
     base = vec4(0.0, 0.0, 0.0, base.r);
 vec4 blend = texture2D(u_FillSampler1, texCoord);

 // Luminance of blend
 float luminance = dot(blend, kWeights);

 // TODO
 vec4 result;

 return mix(base, result, v_UserData.x) * v_ColorScale;
}

void main()
{
    // Perspective-correct texture mapping
    vec2 texCoord = v_TexCoord.xy;
    float q = v_UserData.w;
    if (q > 0.0) texCoord = texCoord / q;
    vec4 _masked = FragmentKernel(texCoord, v_ColorScale, v_UserData);
    if (u_TexFlags.y > 0.5)
        _masked *= texture2D(u_MaskSampler0, v_MaskUV0).r;
    if (u_TexFlags.y > 1.5)
        _masked *= texture2D(u_MaskSampler1, v_MaskUV1).r;
    if (u_TexFlags.y > 2.5)
        _masked *= texture2D(u_MaskSampler2, v_MaskUV2).r;
    gl_FragColor = _masked;
}
