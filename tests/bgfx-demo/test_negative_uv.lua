-- test_negative_uv.lua: Issue #13 — actual level 1 textures
-- Ground: castle-ground1.jpg (800x393), Track: spring-track.png (400x55 palette)
-- SOLAR2D_TEST=negative_uv

local W, H = display.contentWidth, display.contentHeight
display.setDefault("background", 0.53, 0.81, 0.98)

graphics.defineEffect({
    category = "filter", name = "tiling",
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

display.setDefault("textureWrapX", "clampToEdge")
display.setDefault("textureWrapY", "clampToEdge")
local groundTex = graphics.newTexture({type="image", filename="castle-ground1.jpg"})
local trackTex = graphics.newTexture({type="image", filename="spring-track.png"})

local backend = os.getenv("SOLAR2D_BACKEND") or "?"
display.newText("Issue #13 [" .. backend .. "]", W/2, 15, native.systemFontBold, 12):setFillColor(0,0,0)

-- Ground (UV from tank level 1 diagnostic)
local groundH = H * 0.45
local ground = display.newMesh{
    x = W/2, y = H - groundH/2,
    mode = "indexed",
    vertices = { -W/2,-groundH/2, W/2,-groundH/2, W/2,groundH/2, -W/2,groundH/2 },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { -4.343,0.217, 4.930,0.217, 4.930,9.440, -4.343,9.440 },
}
ground.fill = { type="image", filename=groundTex.filename, baseDir=groundTex.baseDir }
ground.fill.effect = "filter.custom.tiling"

-- Track (sand strip on top of ground)
local trackH = 50
local track = display.newMesh{
    x = W/2, y = H - groundH - trackH/2 + trackH,
    mode = "indexed",
    vertices = { -W/2,-trackH/2, W/2,-trackH/2, W/2,trackH/2, -W/2,trackH/2 },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { 0,0, 16.649,0, 16.649,1, 0,1 },
}
track.fill = { type="image", filename=trackTex.filename, baseDir=trackTex.baseDir }
track.fill.effect = "filter.custom.tiling"

print("NEG_UV: " .. backend .. " ground=" .. tostring(groundTex) .. " track=" .. tostring(trackTex))
