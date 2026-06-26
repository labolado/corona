$input v_texcoord, v_color, v_shape

#include <bgfx_shader.sh>

vec2 getShapeVert(int id, int i)
{
    if (id == 0)
    {
        if (i == 0) return vec2( 0.0000, -0.8000);
        if (i == 1) return vec2( 0.7608, -0.2472);
        if (i == 2) return vec2( 0.4702,  0.6472);
        if (i == 3) return vec2(-0.4702,  0.6472);
        return vec2(-0.7608, -0.2472);
    }
    if (id == 1)
    {
        if (i == 0) return vec2( 0.0000, -0.8500);
        if (i == 1) return vec2( 0.2057, -0.2832);
        if (i == 2) return vec2( 0.8084, -0.2627);
        if (i == 3) return vec2( 0.3329,  0.1082);
        if (i == 4) return vec2( 0.4996,  0.6877);
        if (i == 5) return vec2( 0.0000,  0.3500);
        if (i == 6) return vec2(-0.4996,  0.6877);
        if (i == 7) return vec2(-0.3329,  0.1082);
        if (i == 8) return vec2(-0.8084, -0.2627);
        return vec2(-0.2057, -0.2832);
    }
    if (id == 2)
    {
        if (i == 0) return vec2(-0.8000,  0.2200);
        if (i == 1) return vec2( 0.0000,  0.2200);
        if (i == 2) return vec2( 0.0000,  0.6500);
        if (i == 3) return vec2( 0.8000,  0.0000);
        if (i == 4) return vec2( 0.0000, -0.6500);
        if (i == 5) return vec2( 0.0000, -0.2200);
        return vec2(-0.8000, -0.2200);
    }
    if (id == 3)
    {
        if (i == 0) return vec2( 0.0000, -0.8000);
        if (i == 1) return vec2( 0.6928, -0.4000);
        if (i == 2) return vec2( 0.6928,  0.4000);
        if (i == 3) return vec2( 0.0000,  0.8000);
        if (i == 4) return vec2(-0.6928,  0.4000);
        return vec2(-0.6928, -0.4000);
    }
    if (id == 4)
    {
        if (i == 0) return vec2( 0.0000, -0.8500);
        if (i == 1) return vec2( 0.2687, -0.2687);
        if (i == 2) return vec2( 0.8500,  0.0000);
        if (i == 3) return vec2( 0.2687,  0.2687);
        if (i == 4) return vec2( 0.0000,  0.8500);
        if (i == 5) return vec2(-0.2687,  0.2687);
        if (i == 6) return vec2(-0.8500,  0.0000);
        return vec2(-0.2687, -0.2687);
    }
    if (id == 5)
    {
        if (i == 0) return vec2(-0.5000,  0.8000);
        if (i == 1) return vec2( 0.8000,  0.8000);
        if (i == 2) return vec2( 0.8000,  0.1000);
        if (i == 3) return vec2( 0.1500,  0.1000);
        if (i == 4) return vec2( 0.1500, -0.8000);
        return vec2(-0.5000, -0.8000);
    }
    if (id == 6)
    {
        if (i == 0) return vec2( 0.0000, -0.8500);
        if (i == 1) return vec2( 0.7361,  0.4250);
        return vec2(-0.7361,  0.4250);
    }

    if (i == 0) return vec2( 0.0000, -0.8200);
    if (i == 1) return vec2( 0.2100, -0.3637);
    if (i == 2) return vec2( 0.7101, -0.4100);
    if (i == 3) return vec2( 0.4200,  0.0000);
    if (i == 4) return vec2( 0.7101,  0.4100);
    if (i == 5) return vec2( 0.2100,  0.3637);
    if (i == 6) return vec2( 0.0000,  0.8200);
    if (i == 7) return vec2(-0.2100,  0.3637);
    if (i == 8) return vec2(-0.7101,  0.4100);
    return vec2(-0.4200,  0.0000);
}

float sdPoly(vec2 p, int id, int n)
{
    vec2 v0 = getShapeVert(id, 0);
    float d = dot(p - v0, p - v0);
    float s = 1.0;
    int j = n - 1;

    for (int i = 0; i < 10; ++i)
    {
        if (i >= n) break;

        vec2 vi = getShapeVert(id, i);
        vec2 vj = getShapeVert(id, j);
        vec2 e = vj - vi;
        vec2 w = p - vi;
        vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        d = min(d, dot(b, b));

        bool c1 = p.y >= vi.y;
        bool c2 = p.y < vj.y;
        bool c3 = e.x * w.y > e.y * w.x;
        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3))
        {
            s = -s;
        }

        j = i;
    }

    return s * sqrt(d);
}

void main()
{
    float c = v_shape.x;
    float s = v_shape.y;
    int shapeId = int(v_shape.z + 0.5);
    int n = min(int(v_shape.w + 0.5), 10);

    vec2 p = vec2(
        c * v_texcoord.x - s * v_texcoord.y,
        s * v_texcoord.x + c * v_texcoord.y
    );

    float d = sdPoly(p, shapeId, n);
    float aa = max(fwidth(d), 0.0001);
    float alpha = smoothstep(-aa, aa, d);
    vec4 color = v_color * alpha;

    if (color.a < 0.001)
    {
        discard;
    }

    gl_FragColor = color;
}
