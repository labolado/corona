$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

uniform vec4 u_sdfParams;   // x=width, y=height, z=unused, w=strokeWidth
uniform vec4 u_fillColor;
uniform vec4 u_strokeColor;

void main()
{
    // Map UV [0,1] to [-1,1]
    vec2 p = v_texcoord0.xy * 2.0 - 1.0;
    
    // Circle SDF: distance from center
    float d = length(p) - 1.0;
    
    // Anti-aliased edge using screen-space derivatives
    float aa = fwidth(d) * 1.0;
    
    // Fill: inside the shape
    float fillMask = 1.0 - smoothstep(-aa, aa, d);
    
    // Stroke: ring around the edge
    float strokeW = u_sdfParams.w / max(u_sdfParams.x, u_sdfParams.y) * 2.0;
    float strokeOuter = 1.0 - smoothstep(-aa, aa, d);
    float strokeInner = 1.0 - smoothstep(-aa, aa, d + strokeW);
    float strokeMask = strokeOuter * (1.0 - strokeInner);
    
    // Combine: stroke on top of fill
    vec4 color = u_fillColor * fillMask;
    if (strokeW > 0.0)
    {
        // Fill only inside stroke
        float innerFill = 1.0 - smoothstep(-aa, aa, d + strokeW);
        color = u_fillColor * innerFill + u_strokeColor * strokeMask;
    }
    
    // Discard fully transparent pixels
    if (color.a < 0.001) discard;
    
    gl_FragColor = color;
}
