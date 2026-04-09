-- test_custom_shader.lua
-- Minimal custom shader test: 3 effects in 3 columns.
-- Usage: SOLAR2D_TEST=custom_shader SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...
-- Works on: macOS GL, macOS bgfx (shaderc), Android bgfx (runtime binary)

local W, H = display.contentWidth, display.contentHeight
display.setDefault("background", 0.2, 0.2, 0.25)

-- ============================================================
-- Effect 1: Simple tint (multiply color by UserData RGB)
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
-- Effect 2: Simple tiling (UV scale via UserData)
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
-- Effect 3: Composite blend (mix two textures 50/50)
-- ============================================================
graphics.defineEffect({
    category = "composite",
    name = "testBlend",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            P_COLOR vec4 a = texture2D(CoronaSampler0, texCoord);
            P_COLOR vec4 b = texture2D(CoronaSampler1, texCoord);
            return CoronaColorScale(mix(a, b, 0.5));
        }
    ]],
})

-- ============================================================
-- Layout: 3 columns
-- ============================================================
local colW = W / 3
local cy = H / 2
local boxSize = math.min(colW - 20, H - 100)

local function label(text, x, y)
    local t = display.newText(text, x, y, native.systemFont, 12)
    t:setFillColor(1, 1, 1)
    return t
end

label("Custom Shader Test", W/2, 15)

-- Column 1: Tint effect — orange-ish tint on a white rect
local r1 = display.newRect(colW * 0.5, cy, boxSize, boxSize)
r1:setFillColor(1, 1, 1)
r1.fill.effect = "filter.custom.testTint"
r1.fill.effect.r = 1.0
r1.fill.effect.g = 0.6
r1.fill.effect.b = 0.2
label("1: Tint (orange)", colW * 0.5, cy + boxSize/2 + 15)

-- Column 2: Tiling effect — checker pattern tiled 3x3
local checker = display.newRect(colW * 1.5, cy, boxSize, boxSize)
-- Create a simple checker via paint
local checkerPaint = {
    type = "image",
    filename = "test_cyan.png",  -- any small test image
}
checker.fill = checkerPaint
checker.fill.effect = "filter.custom.testTiling"
checker.fill.effect.scaleX = 3
checker.fill.effect.scaleY = 3
label("2: Tiling (3x3)", colW * 1.5, cy + boxSize/2 + 15)

-- Column 3: Composite blend — red + blue = purple-ish
local comp = display.newRect(colW * 2.5, cy, boxSize, boxSize)
comp.fill = {
    type = "composite",
    paint1 = { type = "image", filename = "test_red.png" },
    paint2 = { type = "image", filename = "test_blue.png" },
}
comp.fill.effect = "composite.custom.testBlend"
label("3: Blend (red+blue)", colW * 2.5, cy + boxSize/2 + 15)

-- Status line
local backend = system.getInfo("environment") == "simulator" and "sim" or "device"
label("Backend: " .. (os.getenv("SOLAR2D_BACKEND") or "gl") .. " | " .. backend, W/2, H - 10)

print("=== CUSTOM SHADER TEST: 3 effects registered and applied ===")
