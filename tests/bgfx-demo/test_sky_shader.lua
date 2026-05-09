-- test_sky_shader.lua
-- Diagnose cloud color difference: macOS bgfx (white) vs iOS bgfx (dark gray)
-- Both use Metal, same shader code, but different results
--
-- Usage:
--   SOLAR2D_TEST=sky_shader SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

display.setStatusBar(display.HiddenStatusBar)

local CX, CY = display.contentCenterX, display.contentCenterY
local W, H = display.actualContentWidth, display.actualContentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"

----------------------------------------------------------------------
-- 1. Diagnostic shader: output uniform value as color
----------------------------------------------------------------------
local uniformDiagKernel = {
    language = "glsl",
    category = "filter",
    name = "uniform_diag",
    uniformData = {
        { name = "testColor", default = {1,1,1,1}, type = "vec4", index = 0 },
    },
    fragment = [[
uniform P_COLOR vec4 u_UserData0;
P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
{
    return u_UserData0;
}
]]
}
graphics.defineEffect(uniformDiagKernel)

----------------------------------------------------------------------
-- 2. Diagnostic shader: test noise + fbm output
----------------------------------------------------------------------
local noiseDiagKernel = {
    language = "glsl",
    category = "filter",
    name = "noise_diag",
    uniformData = {
        { name = "wh", default = {1,1}, type = "vec2", index = 0 },
    },
    fragment = [[
uniform P_POSITION vec2 u_UserData0;

P_UV vec2 hash22(P_UV vec2 p)
{
    P_UV vec3 p3 = fract(vec3(p.xyx) * vec3(443.897, 441.423, 437.195));
    p3 += dot(p3, p3.yzx+19.19);
    return -1.0+2.0*fract((p3.xx+p3.yz)*p3.zy);
}

P_UV float noise( P_UV vec2 p )
{
    P_UV vec2 i = floor( p );
    P_UV vec2 f = fract( p );
    P_UV vec2 u = f*f*(3.0-2.0*f);
    return mix( mix( dot( hash22( i + vec2(0.0,0.0) ), f - vec2(0.0,0.0) ),
                     dot( hash22( i + vec2(1.0,0.0) ), f - vec2(1.0,0.0) ), u.x),
                mix( dot( hash22( i + vec2(0.0,1.0) ), f - vec2(0.0,1.0) ),
                     dot( hash22( i + vec2(1.0,1.0) ), f - vec2(1.0,1.0) ), u.x), u.y);
}

P_UV float fbm4(P_UV vec2 p) {
    P_UV mat2 m2 = mat2(1.6,  1.2, -1.2,  1.6);
    P_UV float amp = 0.5;
    P_UV float h = 0.0;
    for (int i = 0; i < 4; i++) {
        P_UV float n = noise(p);
        h += amp * n;
        amp *= 0.5;
        p = m2 * p;
    }
    return 0.5 + 0.5*h;
}

P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
{
    uv -= 0.5;
    uv.x *= u_UserData0.x / u_UserData0.y;

    P_UV vec2 scale = uv * 4.0;
    P_UV float n1 = fbm4(vec2(scale.x - 20.0, scale.y - 50.0));

    // Output: n1 as grayscale, smoothstep cloud mask in red channel
    P_UV float cloud = smoothstep(0.5, 0.8, n1);
    return vec4(cloud, n1, n1, 1.0);
}
]]
}
graphics.defineEffect(noiseDiagKernel)

----------------------------------------------------------------------
-- 3. Full sky shader (from mech_test_copy)
----------------------------------------------------------------------
local skyKernel = {
    language = "glsl",
    category = "filter",
    name = "sky_full",
    uniformData = {
        { name = "wh",         default = {1, 1},           type = "vec2", index = 0 },
        { name = "pos",        default = {0, 0},           type = "vec2", index = 1 },
        { name = "color",      default = {1, 1, 1, 1},     type = "vec4", index = 2 },
        { name = "cloudColor", default = {1, 1, 1, 1},     type = "vec4", index = 3 },
    },
    fragment = [[
uniform P_POSITION vec2 u_UserData0;
uniform P_UV vec2 u_UserData1;
uniform P_COLOR vec4 u_UserData2;
uniform P_POSITION vec4 u_UserData3;

P_UV vec2 hash22(P_UV vec2 p)
{
    P_UV vec3 p3 = fract(vec3(p.xyx) * vec3(443.897, 441.423, 437.195));
    p3 += dot(p3, p3.yzx+19.19);
    return -1.0+2.0*fract((p3.xx+p3.yz)*p3.zy);
}

P_UV float noise( P_UV vec2 p )
{
    P_UV vec2 i = floor( p );
    P_UV vec2 f = fract( p );
    P_UV vec2 u = f*f*(3.0-2.0*f);
    return mix( mix( dot( hash22( i + vec2(0.0,0.0) ), f - vec2(0.0,0.0) ),
                     dot( hash22( i + vec2(1.0,0.0) ), f - vec2(1.0,0.0) ), u.x),
                mix( dot( hash22( i + vec2(0.0,1.0) ), f - vec2(0.0,1.0) ),
                     dot( hash22( i + vec2(1.0,1.0) ), f - vec2(1.0,1.0) ), u.x), u.y);
}

P_UV float fbm4(P_UV vec2 p) {
    P_UV mat2 m2 = mat2(1.6,  1.2, -1.2,  1.6);
    P_UV float amp = 0.5;
    P_UV float h = 0.0;
    for (int i = 0; i < 4; i++) {
        P_UV float n = noise(p);
        h += amp * n;
        amp *= 0.5;
        p = m2 * p;
    }
    return 0.5 + 0.5*h;
}

P_COLOR vec3 getSky(P_UV vec2 uv)
{
    P_UV float py = u_UserData1.y * 0.05;
    P_COLOR vec3 bottomColor = mix(u_UserData2.rgb, vec3(0.0), py * 0.5);
    P_COLOR vec3 skyColor = mix(vec3(0.6, 0.9, 0.87), vec3(0.0), py * 0.5);
    P_COLOR vec3 col = mix(u_UserData2.rgb, vec3(0.0), py);
    return col;
}

P_COLOR vec4 FragmentKernel (P_UV vec2 uv)
{
    P_UV vec2 texCoord = vec2(fract(uv.x - u_UserData1.x), uv.y - u_UserData1.y);
    P_COLOR vec4 texCol = CoronaColorScale(texture2D(CoronaSampler0, texCoord));
    P_COLOR vec3 skyCol = getSky(uv);
    P_COLOR vec3 cloudCol = mix(vec3(1.0), vec3(0.0), u_UserData1.y * 0.7 * 0.02);

    uv -= 0.5;
    uv.x *= u_UserData0.x/u_UserData0.y;
    uv -= u_UserData1;

    P_UV vec2 scale = uv * 4.0;
    P_UV vec2 turbulence = 0.008 * vec2(noise(vec2(uv.x * 10.0, uv.y *10.0)), noise(vec2(uv.x * 10.0, uv.y * 10.0)));
    scale += turbulence;
    P_UV float n1 = fbm4(vec2(scale.x - 20.0, scale.y - 50.0));
    P_COLOR vec3 col = mix( skyCol, cloudCol * u_UserData3.rgb, smoothstep(0.5, 0.8, n1));

    return mix(texCol, vec4(col, 1.0), smoothstep(0.0, 1.5, -texCoord.y));
}
]]
}
graphics.defineEffect(skyKernel)

----------------------------------------------------------------------
-- Layout: 4 rows of test patches
----------------------------------------------------------------------
local rowH = H / 5
local y = 0

-- Title
local title = display.newText({
    text = backend:upper() .. " — Sky Shader Diagnostic",
    x = CX, y = 20, fontSize = 16,
})
title:setFillColor(0)
y = 50

-- Row 1: Full sky shader
local skyRect = display.newRect(CX, y + rowH/2, W - 20, rowH - 10)
skyRect.fill = { type = "image", filename = "bg-village2-1.png" }
skyRect:setFillColor(1, 1, 1, 1)
skyRect.fill.effect = "filter.custom.sky_full"
skyRect.fill.effect.wh = {W, H}
skyRect.fill.effect.pos = {0, 0}
skyRect.fill.effect.color = {0.8, 0.85, 0.9, 1.0}
skyRect.fill.effect.cloudColor = {0.85, 0.85, 0.85, 1.0}
local l1 = display.newText("1: Full sky shader (clouds should be light gray)", CX, y + 5, native.systemFont, 11)
l1:setFillColor(1, 0, 0)
y = y + rowH

-- Row 2: Noise/fbm diagnostic (red=cloud mask, green=raw noise)
local noiseRect = display.newRect(CX, y + rowH/2, W - 20, rowH - 10)
noiseRect.fill = { type = "image", filename = "shape_white.png" }
noiseRect.fill.effect = "filter.custom.noise_diag"
noiseRect.fill.effect.wh = {W, H}
local l2 = display.newText("2: Noise diag (R=cloud mask, G=raw fbm)", CX, y + 5, native.systemFont, 11)
l2:setFillColor(1, 0, 0)
y = y + rowH

-- Row 3: Uniform passthrough - cloudColor {0.85, 0.85, 0.85, 1.0}
local uniRect = display.newRect(CX, y + rowH/2, W/2 - 15, rowH - 10)
uniRect.fill = { type = "image", filename = "shape_white.png" }
uniRect.fill.effect = "filter.custom.uniform_diag"
uniRect.fill.effect.testColor = {0.85, 0.85, 0.85, 1.0}

-- Row 3 right: Lua reference (no shader)
local refRect = display.newRect(CX + W/4, y + rowH/2, W/2 - 15, rowH - 10)
refRect:setFillColor(0.85, 0.85, 0.85, 1.0)

local l3 = display.newText("3: L=uniform(0.85) R=Lua ref(0.85)  should match", CX, y + 5, native.systemFont, 11)
l3:setFillColor(1, 0, 0)
-- reposition left rect
uniRect.x = CX - W/4
y = y + rowH

-- Row 4: CoronaColorScale test
local ccsKernel = {
    language = "glsl",
    category = "filter",
    name = "ccs_diag",
    fragment = [[
P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
{
    P_COLOR vec4 texCol = CoronaColorScale(texture2D(CoronaSampler0, uv));
    return texCol;
}
]]
}
graphics.defineEffect(ccsKernel)

local ccsRect = display.newRect(CX - W/4, y + rowH/2, W/2 - 15, rowH - 10)
ccsRect.fill = { type = "image", filename = "bg-village2-1.png" }
ccsRect.fill.effect = "filter.custom.ccs_diag"

-- Row 4 right: same texture, no shader
local texRef = display.newRect(CX + W/4, y + rowH/2, W/2 - 15, rowH - 10)
texRef.fill = { type = "image", filename = "bg-village2-1.png" }

local l4 = display.newText("4: L=CoronaColorScale R=raw texture  should match", CX, y + 5, native.systemFont, 11)
l4:setFillColor(1, 0, 0)

-- Bring labels to front
title:toFront()
l1:toFront(); l2:toFront(); l3:toFront(); l4:toFront()

print("sky_shader diagnostic loaded, backend=" .. backend)
