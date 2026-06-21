-- #079 regression (FAN path): vertex-format extension on display.newCircle
-- (triangle-fan fill) — the REAL ball_hero2 marble path.
--
-- ball_hero2 marbles are CIRCLES with an image fill: storeOnGPU=false →
-- pool/batch path, and primitiveType == kTriangleFan → the bgfx ExecuteDraw
-- TriangleFan branch. The rect-based test (test_vertexext079.lua) only
-- exercised the "normal" (else) branch, not the fan branch. This test feeds
-- known per-vertex outlineColor through a custom effect declaring a
-- vertexExtension and renders it on circles, so a wrong fan-path stride/offset
-- shows up as scrambled colors / garbage; a correct render shows the exact
-- per-vertex colors.

print("=== vertexext079 FAN test (vertex extension on triangle-fan / circle / pool path) ===")
display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0, 0, 0.12)

-- Mirror ball_hero2's MegaMechanicalData (wh vec2 / tiling vec2 /
-- outlineColor vec3 / outlineWidth float).
graphics.defineVertexExtension({
    name = "MegaMechanicalData",
    { name = "wh",           type = "float", componentCount = 2 },
    { name = "tiling",       type = "float", componentCount = 2 },
    { name = "outlineColor", type = "float", componentCount = 3 },
    { name = "outlineWidth", type = "float", componentCount = 1 },
})

-- Discriminating effect: FS outputs the per-vertex outlineColor varying.
graphics.defineEffect({
    language = "glsl", category = "filter", name = "extcolor079fan",
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

local function makeExtCircle( cx, cy, radius, cr, cg, cb )
    -- A CIRCLE with an IMAGE fill (storeOnGPU=false → pool/batch path,
    -- primitiveType == kTriangleFan → fan branch). This is the real marble path.
    local c = display.newCircle( cx, cy, radius )
    c.fill = { type = "image", filename = "test_checker.png" }
    c.fillExtension = "MegaMechanicalData"
    local n = c.fillVertexCount or 0
    for i = 1, n do
        c.fillExtendedData:setAttributeValue( i, "wh", radius*2, radius*2 )
        c.fillExtendedData:setAttributeValue( i, "tiling", 1, 1 )
        c.fillExtendedData:setAttributeValue( i, "outlineColor", cr, cg, cb )
        c.fillExtendedData:setAttributeValue( i, "outlineWidth", 3 )
    end
    c.fill.effect = "filter.custom.extcolor079fan"
    return c, n
end

-- Multiple circles → multiple fans land in the SAME pool buffer at increasing
-- vertex offsets → exercises the non-zero cmd.offset case (where a double
-- offset bug in the fan branch would show).
local c1, n1 = makeExtCircle( display.contentCenterX - 110, display.contentCenterY - 110, 70, 1, 0, 0 ) -- RED
local c2, n2 = makeExtCircle( display.contentCenterX + 110, display.contentCenterY - 110, 70, 0, 1, 0 ) -- GREEN
local c3, n3 = makeExtCircle( display.contentCenterX - 110, display.contentCenterY + 110, 70, 0, 0, 1 ) -- BLUE
local c4, n4 = makeExtCircle( display.contentCenterX + 110, display.contentCenterY + 110, 70, 1, 1, 0 ) -- YELLOW

print("vertexext079fan: c1 n=" .. tostring(n1) .. " c2 n=" .. tostring(n2)
    .. " c3 n=" .. tostring(n3) .. " c4 n=" .. tostring(n4))
print("EXPECT: four solid circles — top-left RED, top-right GREEN, bottom-left BLUE, bottom-right YELLOW")

-- Keep rendering every frame so the bgfx swap chain stays filled (static scenes
-- otherwise present once and the multi-buffer chain shows the init clear color).
local t = 0
Runtime:addEventListener("enterFrame", function()
    t = t + 1
    c1.rotation = t  -- trivial per-frame change to force redraw
end)

print("SCREENSHOT_READY")
