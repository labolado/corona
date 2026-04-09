-- test_filltex.lua: UV range reproduction test
display.setDefault("background", 0.85, 0.75, 0.6)

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

local W, H = display.contentWidth, display.contentHeight

-- Test 1: UV [0,1] — control
local m1 = display.newMesh{
    x = W * 0.5, y = H * 0.2,
    mode = "indexed",
    vertices = { -100,-40, 100,-40, 100,40, -100,40 },
    uvs = { 0,0, 1,0, 1,1, 0,1 },
    indices = { 1,2,3, 1,3,4 },
}
m1.fill = { type="image", filename="grass1.png" }
m1.fill.effect = "filter.custom.tiling"

-- Test 2: UV [0,3] — mild overshoot
local m2 = display.newMesh{
    x = W * 0.5, y = H * 0.45,
    mode = "indexed",
    vertices = { -100,-40, 100,-40, 100,40, -100,40 },
    uvs = { 0,0, 3,0, 3,3, 0,3 },
    indices = { 1,2,3, 1,3,4 },
}
m2.fill = { type="image", filename="grass1.png" }
m2.fill.effect = "filter.custom.tiling"

-- Test 3: UV [-4,5] x [0,9] — tank terrain range
local m3 = display.newMesh{
    x = W * 0.5, y = H * 0.7,
    mode = "indexed",
    vertices = { -100,-40, 100,-40, 100,40, -100,40 },
    uvs = { -4.3,0.2, 4.9,0.2, 4.9,9.4, -4.3,9.4 },
    indices = { 1,2,3, 1,3,4 },
}
m3.fill = { type="image", filename="grass1.png" }
m3.fill.effect = "filter.custom.tiling"

display.newText("UV [0,1]", W*0.5, H*0.08, native.systemFont, 16)
display.newText("UV [0,3]", W*0.5, H*0.33, native.systemFont, 16)
display.newText("UV [-4,5]x[0,9]", W*0.5, H*0.58, native.systemFont, 16)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
display.newText("Backend: " .. backend, W*0.5, H*0.92, native.systemFont, 14)
print("FILLTEX: " .. backend)
