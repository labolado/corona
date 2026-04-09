-- test_custom_shader.lua
-- Custom shader test: 5 effects covering all categories.
-- Usage: SOLAR2D_TEST=custom_shader SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...
-- Also reachable from the default 11-scene nav (scene 11).

local W, H = display.contentWidth, display.contentHeight
display.setDefault("background", 0.2, 0.2, 0.25)

-- ============================================================
-- Effect 1: Filter — simple tint (multiply color by UserData RGB)
-- ============================================================
graphics.defineEffect({
    category = "filter",
    name = "testTint",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            P_COLOR vec4 color = texture2D(CoronaSampler0, texCoord);
            color.rgb = color.rgb * CoronaVertexUserData.rgb;
            return CoronaColorScale(color);
        }
    ]],
    vertexData = {
        { name = "r", default = 1, index = 0 },
        { name = "g", default = 1, index = 1 },
        { name = "b", default = 1, index = 2 },
    },
})

-- ============================================================
-- Effect 2: Composite — blend two textures (fix: vec4 literal for mix)
-- ============================================================
graphics.defineEffect({
    category = "composite",
    name = "testBlend",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            P_COLOR vec4 a = texture2D(CoronaSampler0, texCoord);
            P_COLOR vec4 b = texture2D(CoronaSampler1, texCoord);
            return CoronaColorScale(a * 0.5 + b * 0.5);
        }
    ]],
})

-- ============================================================
-- Effect 3: Generator — procedural checkerboard (no input texture)
-- ============================================================
graphics.defineEffect({
    category = "generator",
    name = "testChecker",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            P_UV float cx = floor(texCoord.x * CoronaVertexUserData.x);
            P_UV float cy = floor(texCoord.y * CoronaVertexUserData.y);
            P_COLOR float s = cx + cy;
            P_COLOR float checker = s - 2.0 * floor(s * 0.5);
            P_COLOR vec4 c1 = vec4(0.1, 0.8, 0.9, 1.0);
            P_COLOR vec4 c2 = vec4(0.9, 0.2, 0.4, 1.0);
            return CoronaColorScale(c1 + (c2 - c1) * checker);
        }
    ]],
    vertexData = {
        { name = "cols", default = 8, index = 0 },
        { name = "rows", default = 8, index = 1 },
    },
})

-- ============================================================
-- Effect 4: Filter with custom vertex shader — wave distortion
-- ============================================================
graphics.defineEffect({
    category = "filter",
    name = "testWave",
    vertex = [[
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            P_UV float wave = sin(position.y * 0.05 + CoronaTotalTime * 3.0) * CoronaVertexUserData.x;
            return vec2(position.x + wave, position.y);
        }
    ]],
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            P_COLOR vec4 color = texture2D(CoronaSampler0, texCoord);
            return CoronaColorScale(color);
        }
    ]],
    vertexData = {
        { name = "amplitude", default = 10, index = 0 },
    },
})

-- ============================================================
-- Effect 5: Filter — tiling (UV repeat)
-- ============================================================
graphics.defineEffect({
    category = "filter",
    name = "testTiling",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            P_UV vec2 uv = fract(texCoord * CoronaVertexUserData.xy);
            P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
            return CoronaColorScale(color);
        }
    ]],
    vertexData = {
        { name = "scaleX", default = 2, index = 0 },
        { name = "scaleY", default = 2, index = 1 },
    },
})

-- ============================================================
-- Layout: 5 items in a grid (3 top, 2 bottom)
-- ============================================================
local cols = 3
local rowH = H / 2
local colW = W / cols
local boxSize = math.min(colW - 20, rowH - 50)

local function label(text, x, y)
    local t = display.newText(text, x, y, native.systemFont, 11)
    t:setFillColor(1, 1, 1)
    return t
end

label("Custom Shader Test (5 effects)", W/2, 12)

-- Row 1, Col 1: Tint
local r1 = display.newRect(colW * 0.5, rowH * 0.5, boxSize, boxSize)
r1:setFillColor(1, 1, 1)
r1.fill.effect = "filter.custom.testTint"
r1.fill.effect.r = 1.0
r1.fill.effect.g = 0.6
r1.fill.effect.b = 0.2
label("1: Tint", colW * 0.5, rowH * 0.5 + boxSize/2 + 10)

-- Row 1, Col 2: Composite blend
local comp = display.newRect(colW * 1.5, rowH * 0.5, boxSize, boxSize)
comp.fill = {
    type = "composite",
    paint1 = { type = "image", filename = "test_red.png" },
    paint2 = { type = "image", filename = "test_blue.png" },
}
comp.fill.effect = "composite.custom.testBlend"
label("2: Blend", colW * 1.5, rowH * 0.5 + boxSize/2 + 10)

-- Row 1, Col 3: Generator
local gen = display.newRect(colW * 2.5, rowH * 0.5, boxSize, boxSize)
gen.fill = { type = "image", filename = "test_cyan.png" }
gen.fill.effect = "generator.custom.testChecker"
gen.fill.effect.cols = 8
gen.fill.effect.rows = 8
label("3: Generator", colW * 2.5, rowH * 0.5 + boxSize/2 + 10)

-- Row 2, Col 1: Wave (custom VS)
local wave = display.newRect(colW * 0.5, rowH * 1.5, boxSize, boxSize)
wave.fill = { type = "image", filename = "test_cyan.png" }
wave.fill.effect = "filter.custom.testWave"
wave.fill.effect.amplitude = 15
label("4: Wave (VS)", colW * 0.5, rowH * 1.5 + boxSize/2 + 10)

-- Row 2, Col 2: Tiling
local tile = display.newRect(colW * 1.5, rowH * 1.5, boxSize, boxSize)
tile.fill = { type = "image", filename = "test_cyan.png" }
tile.fill.effect = "filter.custom.testTiling"
tile.fill.effect.scaleX = 3
tile.fill.effect.scaleY = 3
label("5: Tiling", colW * 1.5, rowH * 1.5 + boxSize/2 + 10)

-- Status
local backend = system.getInfo("environment") == "simulator" and "sim" or "device"
label("Backend: " .. (os.getenv("SOLAR2D_BACKEND") or "gl") .. " | " .. backend, W/2, H - 8)

print("=== CUSTOM SHADER TEST: 5 effects (filter, composite, generator, custom-VS, tiling) ===")
