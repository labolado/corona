$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

uniform vec4 u_sdfParams;      // x=width, y=height, z=unused, w=strokeWidth
uniform vec4 u_fillColor;
uniform vec4 u_strokeColor;
uniform vec4 u_polyParams;     // x=numVerts (float), y=unused, z=unused, w=unused
uniform vec4 u_polyVerts[8];   // 8 vec4s = 16 vertices max (each vec4: x1,y1,x2,y2)

// Get vertex by index (0..15) from packed vec4 array
// Each vec4 stores 2 vertices: (x0,y0, x1,y1)
vec2 getVert(int idx)
{
    int vecIdx = idx / 2;
    int comp = idx - vecIdx * 2; // 0 or 1 within the vec4

    // bgfx on some backends doesn't support dynamic array indexing,
    // so we unroll with if/else
    vec4 v = u_polyVerts[0];
    if (vecIdx == 1) v = u_polyVerts[1];
    else if (vecIdx == 2) v = u_polyVerts[2];
    else if (vecIdx == 3) v = u_polyVerts[3];
    else if (vecIdx == 4) v = u_polyVerts[4];
    else if (vecIdx == 5) v = u_polyVerts[5];
    else if (vecIdx == 6) v = u_polyVerts[6];
    else if (vecIdx == 7) v = u_polyVerts[7];

    if (comp == 0)
        return v.xy;
    else
        return v.zw;
}

void main()
{
    vec2 p = v_texcoord0.xy * 2.0 - 1.0;

    int numVerts = int(u_polyParams.x);

    // Polygon SDF using winding number method
    // Based on Inigo Quilez's sdPolygon
    float d = dot(p - getVert(0), p - getVert(0));
    float s = 1.0;

    for (int i = 0, j = numVerts - 1; i < numVerts; j = i, i++)
    {
        vec2 vi = getVert(i);
        vec2 vj = getVert(j);
        vec2 e = vj - vi;
        vec2 w = p - vi;
        vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        d = min(d, dot(b, b));

        // Winding number for inside/outside
        // Avoid bvec3/not() — shaderc generates invalid ESSL for not(bvec3)
        bool c1 = p.y >= vi.y;
        bool c2 = p.y < vj.y;
        bool c3 = e.x * w.y > e.y * w.x;
        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) s *= -1.0;
    }

    float dist = s * sqrt(d);

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
