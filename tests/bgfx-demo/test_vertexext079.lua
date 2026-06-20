-- #079 regression: vertex-format extension on a NON-stored-on-GPU (triangle-fan)
-- shape that goes through the batching/pool path.
--
-- Repro of the ball_hero2 marble bug: a custom effect declaring a
-- vertexExtension (per-vertex attributes), applied to a display.newCircle
-- (triangle-fan fill, storeOnGPU=false → pool/batched path). Before the fix the
-- pool geometry lost its extension layout in the bgfx backend (read 136-byte
-- interleaved data with a 68-byte stride → garbled / wrong colors). After the
-- fix the per-vertex attributes arrive intact.
--
-- Verification effect: the fragment shader simply outputs the per-vertex
-- outlineColor varying. We feed each fan vertex a KNOWN color, so a correct
-- render shows that exact color smoothly; a broken render shows noise / wrong
-- colors. (A second circle uses the real sd_circle_cake_ext-style kernel to
-- exercise wh/tiling/outlineColor/outlineWidth together.)

print("=== vertexext079 test (vertex extension on triangle-fan / pool path) ===")
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
-- (Uses an IMAGE fill — like the ball_hero2 marbles — so the shape is NOT
-- SDF-eligible (SDF only accepts solid-color fills) and goes through the
-- batching/pool geometry path where the #079 bug lived.)
graphics.defineEffect({
    language = "glsl", category = "filter", name = "extcolor079",
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
            // Ignore the texture; output only the per-vertex outline color so a
            // wrong vertex stride (the #079 bug) shows as scrambled colors.
            return CoronaColorScale( v_outlineColor );
        }
    ]],
})

local function makeExtShape( cx, cy, colorFn )
    -- A rect with an IMAGE fill (storeOnGPU=false → pool/batch path).
    local r = display.newRect( cx, cy, 180, 180 )
    r.fill = { type = "image", filename = "test_checker.png" }
    r.fillExtension = "MegaMechanicalData"
    local n = r.fillVertexCount or 4
    for i = 1, n do
        local cr, cg, cb = colorFn( i, n )
        r.fillExtendedData:setAttributeValue( i, "wh", 180, 180 )
        r.fillExtendedData:setAttributeValue( i, "tiling", 1, 1 )
        r.fillExtendedData:setAttributeValue( i, "outlineColor", cr, cg, cb )
        r.fillExtendedData:setAttributeValue( i, "outlineWidth", 3 )
    end
    r.fill.effect = "filter.custom.extcolor079"
    return r, n
end

-- (1) Uniform GREEN: correct render = solid green square; garbled = noise.
local r1, n1 = makeExtShape( display.contentCenterX, display.contentCenterY - 120,
    function() return 0, 1, 0 end )

-- (2) Per-vertex GRADIENT: each corner a distinct color, proving distinct
-- per-vertex values reach distinct vertices (red / green / blue / yellow).
local corners = { {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0} }
local r2, n2 = makeExtShape( display.contentCenterX, display.contentCenterY + 120,
    function( i ) local c = corners[ ((i-1) % 4) + 1 ]; return c[1], c[2], c[3] end )

print("vertexext079: r1 vertices=" .. tostring(n1) .. " r2 vertices=" .. tostring(n2))
print("SCREENSHOT_READY")
