-- test_road.lua: Road texture dual-layer test for Issue #13
-- Simulates tank game's GodotBezierTrack terrain rendering
-- Usage: SOLAR2D_TEST=road

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight

-- Background sky
display.newRect(W/2, H/2, W, H):setFillColor(0.8, 0.9, 1.0)

-- Register tiling filter (same as tank project's Shader.filters.tiling)
graphics.defineEffect({
    language = "glsl", category = "filter", name = "tiling",
    vertexData = {
        { name = "tilingX", index = 0, default = 1 },
        { name = "tilingY", index = 1, default = 1 },
        { name = "rotation", index = 2, default = 0 },
    },
    fragment = [[
    P_COLOR vec4 FragmentKernel( P_UV vec2 uv ){
        uv *= CoronaVertexUserData.xy;
        uv = fract(uv);
        P_COLOR vec4 col = texture2D( CoronaSampler0, uv );
        return CoronaColorScale(col);
    }
    ]],
})

-- Diagnostic: output vertex data as color
graphics.defineEffect({
    language = "glsl", category = "filter", name = "tiling_diag",
    vertexData = {
        { name = "tilingX", index = 0, default = 1 },
        { name = "tilingY", index = 1, default = 1 },
    },
    fragment = [[
    P_COLOR vec4 FragmentKernel( P_UV vec2 uv ){
        return vec4(CoronaVertexUserData.x, CoronaVertexUserData.y, 0.0, 1.0);
    }
    ]],
})

-- Helper: load texture (same as tank's Image.loadRepeatTexture)
local function loadRepeatTexture(filename, wrap)
    display.setDefault("textureWrapX", wrap or "clampToEdge")
    display.setDefault("textureWrapY", wrap or "clampToEdge")
    local tex = graphics.newTexture({type="image", filename=filename})
    display.setDefault("textureWrapX", "clampToEdge")
    display.setDefault("textureWrapY", "clampToEdge")
    return tex
end

-- ============================================================
-- Full-screen dual-layer terrain (simulates tank game exactly)
-- Bottom half = ground+track, like the actual game level
-- ============================================================
local groundH = H * 0.45   -- 45% of screen = big ground area
local trackH = 110          -- doubled from level data (55*2) for visibility
local gw = W               -- full screen width, no margin

-- Ground mesh (deep brown, fills bottom portion)
local groundY = H - groundH/2
local groundMesh = display.newMesh{
    x = W/2, y = groundY,
    mode = "indexed",
    vertices = {
        -gw/2, -groundH/2,
         gw/2, -groundH/2,
         gw/2,  groundH/2,
        -gw/2,  groundH/2,
    },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { 0,0, 1,0, 1,1, 0,1 },
}
local groundTex = loadRepeatTexture("castle-ground1.jpg", "clampToEdge")
groundMesh.fill = { type = "image", filename = groundTex.filename, baseDir = groundTex.baseDir }
groundMesh.fill.effect = "filter.custom.tiling"

-- Track mesh (light sand strip on top of ground)
local trackY = H - groundH + trackH/2
local trackMesh = display.newMesh{
    x = W/2, y = trackY,
    mode = "indexed",
    vertices = {
        -gw/2, -trackH/2,
         gw/2, -trackH/2,
         gw/2,  trackH/2,
        -gw/2,  trackH/2,
    },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { 0,0, 1,0, 1,1, 0,1 },
}
local trackTex = loadRepeatTexture("spring-track.png", "clampToEdge")
trackMesh.fill = { type = "image", filename = trackTex.filename, baseDir = trackTex.baseDir }
trackMesh.fill.effect = "filter.custom.tiling"

-- Diagnostic bar at very bottom
local diagH = 60
local diagMesh = display.newMesh{
    x = W/2, y = H - diagH/2,
    mode = "indexed",
    vertices = {
        -gw/2, -diagH/2,
         gw/2, -diagH/2,
         gw/2,  diagH/2,
        -gw/2,  diagH/2,
    },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { 0,0, 1,0, 1,1, 0,1 },
}
diagMesh.fill = { type = "image", filename = "castle-ground1.jpg" }
diagMesh.fill.effect = "filter.custom.tiling_diag"

-- Labels
local title = display.newText("Road Texture Test - Issue #13", W/2, 20, native.systemFontBold, 16)
title:setFillColor(0, 0, 0)
local info = display.newText("Ground(brown) + Track(sand) + Diag(yellow)", W/2, 42, native.systemFont, 12)
info:setFillColor(0, 0, 0)

print("=== Road Texture Test v3 ===")
print("Ground: full-width brown mesh, 45% height")
print("Track: light sand strip on top of ground, 110px height")
print("Diag: yellow bar at bottom = vertex data correct")
