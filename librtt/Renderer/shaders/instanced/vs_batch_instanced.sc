$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

void main()
{
    // Instance data (80 bytes = 5 x vec4):
    // i_data0 = model matrix col 0
    // i_data1 = model matrix col 1
    // i_data2 = model matrix col 3 (translation; col 2 is always 0,0,1,0 for 2D)
    // i_data3 = UV rect (u0, v0, u1, v1)
    // i_data4 = color (r, g, b, a)
    mat4 model = mtxFromCols(i_data0, i_data1, vec4(0.0, 0.0, 1.0, 0.0), i_data2);
    gl_Position = mul(u_viewProj, mul(model, vec4(a_position, 1.0)));

    // Remap unit quad texcoords [0,1] to atlas frame UVs
    vec4 uvRect = i_data3;
    v_texcoord0 = vec2(
        mix(uvRect.x, uvRect.z, a_texcoord0.x),
        mix(uvRect.y, uvRect.w, a_texcoord0.y)
    );

    // Per-instance color
    v_color0 = i_data4;
}
