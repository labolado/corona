-- #079 v2 regression: INDEXED-TRIANGLE mesh + vertexExtension.
-- display.newMesh with indices => kIndexedTriangles => storeOnGPU=true =>
-- ExecuteDrawIndexed. This exercises the indexed path that the strip/pool tests
-- do NOT (TestSpriteShader rects are strips). A wrong stride here garbles the
-- per-vertex outlineColor.

print("=== vertexext079 IDX test (vertex extension on INDEXED mesh) ===")
display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0, 0, 0.12)

graphics.defineVertexExtension({
    name = "MegaMechanicalData",
    { name = "wh",           type = "float", componentCount = 2 },
    { name = "tiling",       type = "float", componentCount = 2 },
    { name = "outlineColor", type = "float", componentCount = 3 },
    { name = "outlineWidth", type = "float", componentCount = 1 },
})

graphics.defineEffect({
    language = "glsl", category = "filter", name = "extcolor079idx",
    vertexExtension = "MegaMechanicalData",
    vertex = [[
        varying P_COLOR vec4 v_outlineColor;
        P_POSITION vec2 VertexKernel( P_POSITION vec2 position )
        {
            v_outlineColor = vec4( CoronaOutlineColor, 1.0 );
            return position;
        }
    ]],
    fragment = [[
        varying P_COLOR vec4 v_outlineColor;
        P_COLOR vec4 FragmentKernel( P_UV vec2 uv )
        {
            return CoronaColorScale( v_outlineColor );
        }
    ]],
})

local function makeExtQuadMesh( cx, cy, sz, cr, cg, cb )
    local h = sz * 0.5
    -- A quad as an INDEXED mesh (2 triangles) => kIndexedTriangles.
    local m = display.newMesh{
        x = cx, y = cy,
        mode = "indexed",
        vertices = { -h,-h,  h,-h,  h,h,  -h,h },
        indices  = { 1,2,3, 1,3,4 },
        uvs      = { 0,0, 1,0, 1,1, 0,1 },
    }
    m.fill = { type = "image", filename = "test_checker.png" }
    m.fillExtension = "MegaMechanicalData"
    local n = m.fillVertexCount or 0
    for i = 1, n do
        m.fillExtendedData:setAttributeValue( i, "wh", sz, sz )
        m.fillExtendedData:setAttributeValue( i, "tiling", 1, 1 )
        m.fillExtendedData:setAttributeValue( i, "outlineColor", cr, cg, cb )
        m.fillExtendedData:setAttributeValue( i, "outlineWidth", 3 )
    end
    m.fill.effect = "filter.custom.extcolor079idx"
    return m, n
end

local c1,n1 = makeExtQuadMesh( display.contentCenterX-110, display.contentCenterY-110, 130, 1,0,0 ) -- RED
local c2,n2 = makeExtQuadMesh( display.contentCenterX+110, display.contentCenterY-110, 130, 0,1,0 ) -- GREEN
local c3,n3 = makeExtQuadMesh( display.contentCenterX-110, display.contentCenterY+110, 130, 0,0,1 ) -- BLUE
local c4,n4 = makeExtQuadMesh( display.contentCenterX+110, display.contentCenterY+110, 130, 1,1,0 ) -- YELLOW
print("vertexext079idx: n=" .. tostring(n1) .. " (indexed mesh, stored-on-GPU)")
print("EXPECT: four solid squares — TL RED, TR GREEN, BL BLUE, BR YELLOW (no garbage)")

local frameN = 0
Runtime:addEventListener("enterFrame", function()
    frameN = frameN + 1
    if frameN == 30 then
        display.save( display.getCurrentStage(), {
            filename = "idx079_capture.png",
            baseDir = system.DocumentsDirectory,
        } )
        print("=== captured idx079_capture.png ===")
    end
end)
