// KERNEL_MODE
// Composite: add

vec4 Add(in vec4 base, in vec4 blend)
{
    return base + blend;
}

vec4 FragmentKernel(vec2 texCoord, vec4 COLOR_SCALE, vec4 USER_DATA)
{
    vec4 base = texture2D(u_FillSampler0, texCoord);

    if (u_TexFlags.x > 0.5)
        base = vec4(0.0, 0.0, 0.0, base.r);
    vec4 blend = texture2D(u_FillSampler1, texCoord);

    vec4 result = Add(base, blend);

    return mix(base, result, USER_DATA.x) * COLOR_SCALE;
}
