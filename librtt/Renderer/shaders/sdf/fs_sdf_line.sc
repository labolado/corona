$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

uniform vec4 u_sdfParams;   // x=width, y=height, z=unused, w=strokeWidth (line width)
uniform vec4 u_fillColor;
uniform vec4 u_strokeColor;
uniform vec4 u_lineParams;  // x=x0, y=y0, z=x1, w=y1 (endpoints in normalized [-1,1] space)

void main()
{
    // Map UV [0,1] to [-1,1]
    vec2 p = v_texcoord0.xy * 2.0 - 1.0;

    // Line segment endpoints in normalized space
    vec2 a = u_lineParams.xy;
    vec2 b = u_lineParams.zw;

    // Capsule SDF: distance from point to line segment
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float d = length(pa - ba * h);

    // Line radius in normalized space
    // strokeWidth is the line width in pixels, normalize by the larger bbox dimension
    float lineR = u_sdfParams.w / max(u_sdfParams.x, u_sdfParams.y) * 2.0;

    // Distance from the capsule edge
    float dist = d - lineR;

    // Anti-aliased edge
    float aa = fwidth(dist) * 1.0;
    float mask = 1.0 - smoothstep(-aa, aa, dist);

    vec4 color = u_fillColor * mask;

    if (color.a < 0.001) discard;

    gl_FragColor = color;
}
