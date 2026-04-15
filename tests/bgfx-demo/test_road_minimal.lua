-- test_road_minimal.lua: Issue #13 — precise reproduction
-- Key factors: different background texture (solid1-1.jpg) with tiling BEFORE ground mesh
-- 4 quadrants testing combinations
-- SOLAR2D_TEST=road_minimal

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
display.setDefault("background", 1, 0, 0)  -- RED to detect invisible meshes

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
    ]]
})

-- UV diagnostic shader: output fract(uv) as color
graphics.defineEffect({
    language = "glsl", category = "filter", name = "uv_diag",
    vertexData = {
        { name = "tilingX", index = 0, default = 1 },
        { name = "tilingY", index = 1, default = 1 },
    },
    fragment = [[
    P_COLOR vec4 FragmentKernel( P_UV vec2 uv ){
        P_UV vec2 scaled = uv * CoronaVertexUserData.xy;
        P_UV vec2 f = fract(scaled);
        return vec4(f.x, f.y, 0.5, 1.0);
    }
    ]]
})

display.setDefault("textureWrapX", "clampToEdge")
display.setDefault("textureWrapY", "clampToEdge")

local hw, hh = W/2, H/2

-- ============================================================
-- Full-screen background tiling (BEFORE ground meshes!)
-- Game does: Image:repeatFill(bkg, "solid1-1.jpg")
-- This creates a tiling fill with effect.tilingX/Y on full screen rect
-- ============================================================
local bkg = display.newRect(W/2, H/2, W*2, H*2)
local bkgTex = graphics.newTexture({type="image", filename="solid1-1.jpg"})
bkg.fill = {type="image", filename=bkgTex.filename, baseDir=bkgTex.baseDir}
bkg.fill.effect = "filter.custom.tiling"
-- solid1-1.jpg is tiny, so tiling values are huge
bkg.fill.effect.tilingX = (W*2) / 64  -- assume 64x64
bkg.fill.effect.tilingY = (H*2) / 64
bkg:toBack()

-- ============================================================
-- A (top-left): castle-ground1.jpg with game UV range
-- Standard test
-- ============================================================
local meshA = display.newMesh{
    x = hw/2, y = hh/2+10,
    mode = "indexed",
    vertices = { -hw/2+4,-hh/2+14, hw/2-4,-hh/2+14, hw/2-4,hh/2-4, -hw/2+4,hh/2-4 },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { -4.343,0.217, 4.930,0.217, 4.930,9.440, -4.343,9.440 },
}
local texA = graphics.newTexture({type="image", filename="castle-ground1.jpg"})
meshA.fill = {type="image", filename=texA.filename, baseDir=texA.baseDir}
meshA.fill.effect = "filter.custom.tiling"
meshA.fill.scaleX = 1; meshA.fill.scaleY = 1
meshA:setFillColor(1, 1, 1, 1)

-- ============================================================
-- B (top-right): desert-ground-1.jpg (different texture, same UV)
-- ============================================================
local meshB = display.newMesh{
    x = hw+hw/2, y = hh/2+10,
    mode = "indexed",
    vertices = { -hw/2+4,-hh/2+14, hw/2-4,-hh/2+14, hw/2-4,hh/2-4, -hw/2+4,hh/2-4 },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { -4.343,0.217, 4.930,0.217, 4.930,9.440, -4.343,9.440 },
}
local texB = graphics.newTexture({type="image", filename="desert-ground-1.jpg"})
meshB.fill = {type="image", filename=texB.filename, baseDir=texB.baseDir}
meshB.fill.effect = "filter.custom.tiling"
meshB.fill.scaleX = 1; meshB.fill.scaleY = 1
meshB:setFillColor(1, 1, 1, 1)

-- ============================================================
-- C (bottom-left): UV diagnostic — shows fract(uv) as color
-- GL and bgfx should produce IDENTICAL gradient
-- ============================================================
local meshC = display.newMesh{
    x = hw/2, y = hh+hh/2+10,
    mode = "indexed",
    vertices = { -hw/2+4,-hh/2+14, hw/2-4,-hh/2+14, hw/2-4,hh/2-4, -hw/2+4,hh/2-4 },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { -4.343,0.217, 4.930,0.217, 4.930,9.440, -4.343,9.440 },
}
meshC.fill = {type="image", filename=texA.filename, baseDir=texA.baseDir}
meshC.fill.effect = "filter.custom.uv_diag"

-- ============================================================
-- D (bottom-right): Same texture as A but with fill.effect.tilingX
-- instead of fill.scaleX (game's _changeMaterial vs _groundChangeMaterial)
-- ============================================================
local meshD = display.newMesh{
    x = hw+hw/2, y = hh+hh/2+10,
    mode = "indexed",
    vertices = { -hw/2+4,-hh/2+14, hw/2-4,-hh/2+14, hw/2-4,hh/2-4, -hw/2+4,hh/2-4 },
    indices = { 1, 2, 3, 1, 3, 4 },
    uvs = { -4.343,0.217, 4.930,0.217, 4.930,9.440, -4.343,9.440 },
}
local texD = graphics.newTexture({type="image", filename="castle-ground1.jpg"})
meshD.fill = {type="image", filename=texD.filename, baseDir=texD.baseDir}
meshD.fill.effect = "filter.custom.tiling"
-- Use effect.tilingX instead of fill.scaleX (like Image:_changeMaterial)
meshD.fill.effect.tilingX = 1
meshD.fill.effect.tilingY = 1
meshD:setFillColor(1, 1, 1, 1)

-- Labels
local labels = {"A:ground(scaleX)", "B:desert(scaleX)", "C:UV diagnostic", "D:ground(tilingX)"}
local lx = {hw/2, hw+hw/2, hw/2, hw+hw/2}
local ly = {12, 12, hh+12, hh+12}
for i, lbl in ipairs(labels) do
    display.newText(lbl, lx[i], ly[i], native.systemFontBold, 9):setFillColor(1,1,0)
end

print("=== Road Minimal: 4-quad with solid1-1 background ===")
