$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

uniform vec4 u_sdfParams;   // x=width, y=height, z=cornerRadius, w=strokeWidth
uniform vec4 u_fillColor;
uniform vec4 u_strokeColor;

void main()
{
    vec2 p = v_texcoord0.xy * 2.0 - 1.0;
    
    // Box SDF with corner radius
    float cr = u_sdfParams.z / max(u_sdfParams.x, u_sdfParams.y) * 2.0;
    vec2 d = abs(p) - (1.0 - cr);
    float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - cr;
    
    float aa = fwidth(dist) * 1.0;
    float fillMask = 1.0 - smoothstep(-aa, aa, dist);
    
    float strokeW = u_sdfParams.w / max(u_sdfParams.x, u_sdfParams.y) * 2.0;
    vec4 color = u_fillColor * fillMask;
    if (strokeW > 0.0)
    {
        float innerFill = 1.0 - smoothstep(-aa, aa, dist + strokeW);
        float strokeOuter = fillMask;
        float strokeMask = strokeOuter * (1.0 - innerFill);
        color = u_fillColor * innerFill + u_strokeColor * strokeMask;
    }
    
    if (color.a < 0.001) discard;
    gl_FragColor = color;
}
