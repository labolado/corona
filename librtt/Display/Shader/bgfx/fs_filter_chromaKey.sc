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

// Filter: chromaKey

#include <bgfx_shader.sh>

SAMPLER2D(u_FillSampler0, 0);

// Time and data uniforms (packed in vec4 as bgfx doesn't have float uniforms)
uniform vec4 u_TotalTime;
uniform vec4 u_DeltaTime;
uniform vec4 u_TexelSize;
uniform vec4 u_ContentScale;
uniform vec4 u_ContentSize;

// User data uniforms
uniform vec4 u_UserData0;  // use .x for scalar
uniform vec4 u_UserData1;  // use .x for scalar
uniform vec4 u_UserData2;
uniform vec4 u_UserData3;

// Solar2D macros for shader compatibility
#define CoronaColorScale(color) (v_ColorScale * (color))
#define CoronaVertexUserData v_UserData
#define CoronaTotalTime u_TotalTime.x
#define CoronaDeltaTime u_DeltaTime.x
#define CoronaTexelSize u_TexelSize
#define CoronaContentScale u_ContentScale.xy

// sensitivity
// smoothing
// color

void main()
{
float sensitivity = u_UserData0; // threshold
    float smoothing = u_UserData1;
    vec4 color = u_UserData2;

    vec4 texColor = ( texture2D( u_FillSampler0, v_TexCoord.xy ) * v_ColorScale );
    //vec4 texColor = texture2D( u_FillSampler0, v_TexCoord.xy );

    float maskY = 0.2989 * color.r + 0.5866 * color.g + 0.1145 * color.b;
    float maskCr = 0.7132 * (color.r - maskY);
    float maskCb = 0.5647 * (color.b - maskY);
    
    float Y = 0.2989 * texColor.r + 0.5866 * texColor.g + 0.1145 * texColor.b;
    float Cr = 0.7132 * (texColor.r - Y);
    float Cb = 0.5647 * (texColor.b - Y);

    // float blendValue = 1.0 - smoothstep(sensitivity - smoothing, sensitivity , abs(Cr - maskCr) + abs(Cb - maskCb));
    float blendValue = smoothstep(sensitivity, sensitivity + smoothing, distance(vec2(Cr, Cb), vec2(maskCr, maskCb)));

	//return vec4(texColor.rgb, texColor.a * blendValue);
    gl_FragColor = texColor * blendValue;
}
