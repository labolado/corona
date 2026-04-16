// KERNEL_MODE
// Filter: brightness

vec4 FragmentKernel(vec2 texCoord, vec4 COLOR_SCALE, vec4 USER_DATA)
{
    // texColor has pre-multiplied alpha.
    vec4 texColor = texture2D(u_FillSampler0, texCoord);

    if (u_TexFlags.x > 0.5)
        texColor = vec4(0.0, 0.0, 0.0, texColor.r);

    // pre-multiply brightness as well
    float brightness = USER_DATA.x * texColor.a;

    // Add the brightness.
    texColor.rgb += brightness;

    return texColor * COLOR_SCALE;
}
