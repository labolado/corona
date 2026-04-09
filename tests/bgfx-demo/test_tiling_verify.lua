-- test_tiling_verify.lua
-- Minimal reproduction: solid red texture with tiling shader
-- GL renders (254,0,0), bgfx renders (247,47,32) - WHY?

local W, H = display.contentWidth, display.contentHeight

graphics.defineEffect({
    category = "filter",
    name = "tiling",
    vertexData = {
        { name = "tilingX", index = 0, default = 1 },
        { name = "tilingY", index = 1, default = 1 },
    },
    fragment = [[
        P_COLOR vec4 FragmentKernel( P_UV vec2 uv ){
            uv *= CoronaVertexUserData.xy;
            uv = fract(uv);
            P_COLOR vec4 col = texture2D( CoronaSampler0, uv );
            return CoronaColorScale(col);
        }
    ]]
})

display.setDefault("background", 0, 0, 0)

-- Test 1: solid1-1.jpg with tiling via graphics.newTexture (tank's path)
local tex1 = graphics.newTexture({type="image", filename="solid1-1.jpg"})
local r1 = display.newRect(W*0.25, H*0.3, 200, 150)
if tex1 then
    r1.fill = { type="image", filename=tex1.filename, baseDir=tex1.baseDir }
    r1.fill.effect = "filter.custom.tiling"
    r1.fill.effect.tilingX = 3
    r1.fill.effect.tilingY = 3
end
display.newText("newTexture+tiling", W*0.25, H*0.15, native.systemFont, 10):setFillColor(1,1,0)

-- Test 2: solid1-1.jpg with tiling via direct fill
local r2 = display.newRect(W*0.75, H*0.3, 200, 150)
r2.fill = { type="image", filename="solid1-1.jpg" }
r2.fill.effect = "filter.custom.tiling"
r2.fill.effect.tilingX = 3
r2.fill.effect.tilingY = 3
display.newText("direct+tiling", W*0.75, H*0.15, native.systemFont, 10):setFillColor(1,1,0)

-- Test 3: solid1-1.jpg NO tiling, via newTexture
local tex3 = graphics.newTexture({type="image", filename="solid1-1.jpg"})
local r3 = display.newRect(W*0.25, H*0.7, 200, 150)
if tex3 then
    r3.fill = { type="image", filename=tex3.filename, baseDir=tex3.baseDir }
end
display.newText("newTexture only", W*0.25, H*0.55, native.systemFont, 10):setFillColor(1,1,0)

-- Test 4: solid1-1.jpg NO tiling, direct
local r4 = display.newRect(W*0.75, H*0.7, 200, 150)
r4.fill = { type="image", filename="solid1-1.jpg" }
display.newText("direct only", W*0.75, H*0.55, native.systemFont, 10):setFillColor(1,1,0)

-- Test 5: pure red rect (no texture, reference)
local r5 = display.newRect(W*0.5, H*0.92, 100, 30)
r5:setFillColor(254/255, 0, 0)
display.newText("pure red ref", W*0.5, H*0.87, native.systemFont, 10):setFillColor(1,1,0)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
display.newText("Backend: " .. backend, W*0.5, 15, native.systemFont, 12):setFillColor(1,1,0)
print("RED_TEX_TEST: READY backend=" .. backend)
