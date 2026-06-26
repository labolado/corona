$input a_position, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_texcoord, v_color, v_shape

#include <bgfx_shader.sh>

void main()
{
    mat4 model = mtxFromCols(i_data0, i_data1, vec4(0.0, 0.0, 1.0, 0.0), i_data2);
    gl_Position = mul(u_viewProj, mul(model, vec4(a_position, 0.0, 1.0)));

    v_texcoord = a_position;
    v_color = i_data4;
    v_shape = vec4(i_data3.xy, i_data3.z, i_data3.w);
}
