-- test_composite_tiling.lua: Test composite.texture_tiling on bgfx
-- Reproduces the exact pattern used by tank game's textured_block.lua
-- Usage: SOLAR2D_TEST=composite_tiling

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
display.setDefault("background", 0.5, 0.5, 0.5) -- gray background

-- Register composite.texture_tiling (same as tank game)
graphics.defineEffect({
    language = "glsl",
    category = "composite",
    name = "texture_tiling",
    vertexData = {
        { name = "tilingX", index = 0, default = 1 },
        { name = "tilingY", index = 1, default = 1 },
        { name = "offsetX", index = 2, default = 0 },
        { name = "offsetY", index = 3, default = 0 },
    },
    fragment = [[
    P_COLOR vec4 FragmentKernel( P_UV vec2 uv ){
        P_COLOR vec4 col0 = texture2D(CoronaSampler0, uv);
        uv = fract(uv * CoronaVertexUserData.xy + CoronaVertexUserData.zw);
        P_COLOR vec4 col1 = texture2D(CoronaSampler1, uv);
        return CoronaColorScale(col1 * col0.a);
    }
    ]]
})

-- Register filter.tiling (used by Test 3)
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "tiling",
    vertexData = {
        { name = "tilingX", index = 0, default = 1 },
        { name = "tilingY", index = 1, default = 1 },
    },
    fragment = [[
    P_COLOR vec4 FragmentKernel( P_UV vec2 uv ){
        uv = fract(uv * CoronaVertexUserData.xy);
        return CoronaColorScale(texture2D(CoronaSampler0, uv));
    }
    ]]
})

-- Register simple tint filter (same as tank game's initial state)
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "tint",
    fragment = [[
    P_COLOR vec4 FragmentKernel( P_UV vec2 uv ){
        P_COLOR vec4 col = texture2D(CoronaSampler0, uv);
        return CoronaColorScale(col);
    }
    ]]
})

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
display.newText("Backend: " .. backend, W/2, 15, native.systemFontBold, 14):setFillColor(1,1,0)

-- Test 1: Direct composite fill (no shader switch)
local r1 = display.newRect(W*0.25, H*0.25, 200, 150)
r1.fill = {
    type = "composite",
    paint1 = { type = "image", filename = "desert-ground-1.jpg" },
    paint2 = { type = "image", filename = "desert-track-3.png" },
}
r1.fill.effect = "composite.custom.texture_tiling"
r1.fill.effect.tilingX = 3
r1.fill.effect.tilingY = 2
display.newText("Direct composite", W*0.25, H*0.25 - 90, native.systemFont, 10):setFillColor(1,1,0)

-- Test 2: Dynamic switch (filter.tint -> composite.texture_tiling)
-- This simulates what the tank game does
local r2 = display.newRect(W*0.75, H*0.25, 200, 150)
-- Step 1: Set single image fill + tint (initial state like tank game)
r2.fill = { type = "image", filename = "desert-ground-1.jpg" }
r2.fill.effect = "filter.custom.tint"
-- Step 2: After a timer, switch to composite (simulates changeTexture)
timer.performWithDelay(500, function()
    r2.fill = {
        type = "composite",
        paint1 = { type = "image", filename = "desert-ground-1.jpg" },
        paint2 = { type = "image", filename = "desert-track-3.png" },
    }
    r2.fill.effect = "composite.custom.texture_tiling"
    r2.fill.effect.tilingX = 3
    r2.fill.effect.tilingY = 2
    print("COMPOSITE_TEST: switched r2 to composite")
end)
display.newText("Switch: tint->composite", W*0.75, H*0.25 - 90, native.systemFont, 10):setFillColor(1,1,0)

-- Test 3: filter.tiling only (known working, reference)
local r3 = display.newRect(W*0.25, H*0.65, 200, 150)
r3.fill = { type = "image", filename = "desert-track-3.png" }
r3.fill.effect = "filter.custom.tiling"
r3.fill.effect.tilingX = 3
r3.fill.effect.tilingY = 2
display.newText("filter.tiling only", W*0.25, H*0.65 - 90, native.systemFont, 10):setFillColor(1,1,0)

-- Test 4: No tiling, just the texture (reference)
local r4 = display.newRect(W*0.75, H*0.65, 200, 150)
r4.fill = { type = "image", filename = "desert-track-3.png" }
display.newText("No tiling (ref)", W*0.75, H*0.65 - 90, native.systemFont, 10):setFillColor(1,1,0)

print("COMPOSITE_TEST: READY backend=" .. backend)

-- 等 60 帧确保 composite 切换（500ms timer）完成后截图
local fc = 0
Runtime:addEventListener("enterFrame", function()
    fc = fc + 1
    if fc == 60 then
        print("COMPOSITE_TILING TEST: DONE")
        print("SCREENSHOT_READY")
    end
end)
