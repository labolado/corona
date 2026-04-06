-- test_crash_repro.lua
-- Reproduce the crash: canvas texture + blinked_outline shader
-- This mimics what CourseBuild does in the tank course scene

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0.3, 0.3, 0.3)

-- Register blinked_outline shader (same as tank project)
local blinkKernel = {}
blinkKernel.language = "glsl"
blinkKernel.category = "filter"
blinkKernel.name = "blinked_outline"
blinkKernel.isTimeDependent = true
blinkKernel.uniformData = {
    { name = "outlineWidth", default = 1, min = 0, max = 10000, type="float", index = 0 },
    { name = "color1", default = { 0, 0, 0, 0.2 }, min = { 0, 0, 0, 0 }, max = { 1, 1, 1, 1 }, type="vec4", index = 1 },
    { name = "color2", default = { 1, 1, 1, 0.2 }, min = { 0, 0, 0, 0 }, max = { 1, 1, 1, 1 }, type="vec4", index = 2 }
}
blinkKernel.vertex = [[
uniform P_COLOR vec4 u_UserData1;
uniform P_COLOR vec4 u_UserData2;
varying P_COLOR vec4 outlineColor;

P_POSITION vec2 VertexKernel( P_POSITION vec2 position )
{
    P_DEFAULT float value = mod(floor(CoronaTotalTime * 1.0), 2.0);
    if (value < 0.001) {
        outlineColor = u_UserData2;
    }
    else {
        outlineColor = u_UserData1;
    }
    return position;
}
]]
blinkKernel.fragment = [[
varying P_COLOR vec4 outlineColor;
uniform P_UV float u_UserData0;

P_COLOR vec4 FragmentKernel (P_UV vec2 uv)
{
    P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
    P_UV float w = u_UserData0 * CoronaTexelSize.x;
    P_UV float h = u_UserData0 * CoronaTexelSize.y;
    P_COLOR float a;
    P_COLOR float maxa = color.a;
    P_COLOR float mina = color.a;
    a = texture2D(CoronaSampler0, uv + vec2(0, -h)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(0, h)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(-w, 0)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(w, 0)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(-w, h)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(-w, -h)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(w, -h)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(w, h)).a; maxa = max(a, maxa); mina = min(a, mina);
    color = mix(vec4(0.0), outlineColor, maxa - mina);
    return CoronaColorScale(color);
}
]]
graphics.defineEffect(blinkKernel)
print("blinked_outline shader registered")

-- Also register colored_outline
local colorKernel = {}
colorKernel.language = "glsl"
colorKernel.category = "filter"
colorKernel.name = "colored_outline"
colorKernel.isTimeDependent = true
colorKernel.vertex = [[
varying P_COLOR vec4 outlineColor;
P_POSITION vec2 VertexKernel( P_POSITION vec2 position )
{
    P_DEFAULT float value = mod(floor(CoronaTotalTime * 1.0), 3.0);
    if (value < 0.001) {
        outlineColor = vec4(0.5, 0.4, 0.4, 0.9);
    } else if ((value - 1.0) < 0.001) {
        outlineColor = vec4(0.2, 1.0, 0.2, 0.9);
    } else {
        outlineColor = vec4(0.157, 0.835, 0.835, 0.9);
    }
    return position;
}
]]
colorKernel.fragment = [[
varying P_COLOR vec4 outlineColor;
P_COLOR vec4 FragmentKernel (P_UV vec2 uv)
{
    P_COLOR vec4 col = texture2D(CoronaSampler0, fract(uv));
    return CoronaColorScale( outlineColor * col.a );
}
]]
graphics.defineEffect(colorKernel)
print("colored_outline shader registered")

-- Create objects like CourseBuild does:
-- 1. Create a display object (block/image)
-- 2. Draw it into a canvas texture
-- 3. Create a rect filled with the canvas texture
-- 4. Apply blinked_outline shader

local function createOutlineFromObject(obj, x, y)
    local w_pad, h_pad = 64, 64
    local width = obj.contentWidth + w_pad
    local height = obj.contentHeight + w_pad

    -- Canvas texture (like CourseBuild line 120)
    local tex = graphics.newTexture({ type="canvas", width=width, height=height,
                                       pixelWidth = width * 0.5, pixelHeight = height * 0.5 })
    tex:draw(obj)
    tex:invalidate()

    -- Rect with canvas as fill (like CourseBuild line 123-124)
    local outline = display.newRect(x, y, tex.width, tex.height)
    outline.fill = {type = "image", filename = tex.filename, baseDir = tex.baseDir}
    outline.texture = tex -- prevent GC
    return outline
end

-- Create some test objects with outlines
local objects = {}
for i = 1, 5 do
    local block = display.newRoundedRect(0, 0, 80, 60, 8)
    block:setFillColor(0.2 + i*0.1, 0.3, 0.5)
    block.strokeWidth = 2
    block:setStrokeColor(0.8, 0.8, 0.8)

    local x = 100 + (i-1) * 100
    local y = H/2

    local outline = createOutlineFromObject(block, x, y)

    -- Apply blinked_outline shader (like CourseBuild line 136-139)
    outline.fill.effect = "filter.custom.blinked_outline"
    outline.fill.effect.outlineWidth = 2
    outline.fill.effect.color1 = {1, 0.38, 0.11, 0.9}
    outline.fill.effect.color2 = {0.1, 0.57, 0, 0.67}

    objects[i] = outline
    print("Created outline " .. i)
end

-- Also create one with colored_outline
local circle = display.newCircle(0, 0, 40)
circle:setFillColor(0.8, 0.3, 0.3)
local outlineC = createOutlineFromObject(circle, W/2, H/2 + 100)
outlineC.fill.effect = "filter.custom.colored_outline"
print("Created colored_outline object")

print("TEST COMPLETE - all objects created successfully")
print("If you see this, the crash does NOT happen with standalone canvas+outline")
