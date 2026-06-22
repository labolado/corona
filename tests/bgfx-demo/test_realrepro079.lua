-- #079 faithful repro: mirror ball_hero2 TestSpriteShader:testEllipse() structure
-- as closely as possible to trigger the real green-band + black-zigzag bug.
--   * gray #4d4d4d fullscreen bg (real uses addFullScreenColorBackground)
--   * nested subgroups: layer -> workGroup -> group (real uses display.newSubGroup)
--   * meta079.png paletted white texture (real uses meta.png)
--   * sd_circle_cake_ext kernel verbatim from the project
--   * optional group scale via EB_SCALE (gallery is zoomable; default 1)
print("=== realrepro079 ===")
display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local _L, _T, _R, _B = 0, 0, W, H

-- fullscreen gray bg
local bkg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bkg:setFillColor(0.302, 0.302, 0.302)

graphics.defineVertexExtension({
    name = "MegaMechanicalData",
    { name = "wh",           type = "float", componentCount = 2 },
    { name = "tiling",       type = "float", componentCount = 2 },
    { name = "outlineColor", type = "float", componentCount = 3 },
    { name = "outlineWidth", type = "float", componentCount = 1 },
})
graphics.defineEffect({
    language = "glsl", category = "filter", name = "sd_circle_cake_ext",
    vertexExtension = "MegaMechanicalData",
    vertex = [[
        varying P_POSITION vec2 v_wh;
        varying P_UV vec2 v_tiling;
        varying P_COLOR vec4 v_outlineColor;
        varying P_POSITION float v_outlineWidth;
        P_POSITION vec2 VertexKernel (P_POSITION vec2 position) {
            v_wh = CoronaWh;
            v_tiling = CoronaTiling;
            v_outlineColor = vec4(CoronaOutlineColor, 1.0);
            v_outlineWidth = CoronaOutlineWidth;
            return position;
        }
    ]],
    fragment = [[
        varying P_POSITION vec2 v_wh;
        varying P_UV vec2 v_tiling;
        varying P_COLOR vec4 v_outlineColor;
        varying P_POSITION float v_outlineWidth;
        P_UV float sdPie(P_UV vec2 p, P_UV vec2 c, P_UV float r) {
            p.x = abs(p.x);
            P_UV float l = length(p) - r;
            P_UV float mm = length(p - c*clamp(dot(p,c),0.0,r) );
            return max(l,mm*sign(c.y*p.x-c.x*p.y));
        }
        P_UV float sdCircle(P_UV vec2 p, P_UV float r ) { return length(p) - r; }
        P_UV vec2 rotateCW(P_UV vec2 p, P_UV float a) {
            P_UV mat2 m = mat2(cos(a), -sin(a), sin(a), cos(a));
            return p * m;
        }
        P_COLOR vec4 FragmentKernel (P_UV vec2 uv) {
            P_UV vec2 uv2 = fract( uv * v_tiling );
            P_COLOR vec4 col2 = CoronaColorScale(texture2D( CoronaSampler0, uv2 ));
            P_UV float px = 1. / v_wh.x;
            P_UV vec2 p = (2.0 * uv * v_wh - v_wh) * px;
            P_UV float outline = v_outlineWidth * px;
            P_UV float s = 2.0 * px;
            P_UV float offset = outline + 4.0 * px;
            P_UV float fill = sdCircle(p, 1.0 - offset);
            P_UV float angle = radians(10.0);
            P_UV float fill2 = sdPie(rotateCW(p, radians(90.0)), vec2(sin(angle), cos(angle)), 1.0 - offset);
            P_UV float stroke = abs(fill) - outline;
            P_COLOR vec4 col = vec4(0.0);
            P_COLOR float a1 = 1.0-smoothstep( -s, s, fill );
            P_COLOR float ratio = v_outlineWidth / 3.0;
            P_COLOR float a2 = 1.0-smoothstep( -s*ratio, s*ratio, stroke );
            col = mix( col, col2, a1 );
            col = mix(col, vec4(col.rgb * 0.9, col.a), 1.0-smoothstep( -s, s, fill2 ));
            P_COLOR float m = step(v_outlineColor.a, 0.01);
            col2 = mix(v_outlineColor * v_ColorScale.a, col2, m);
            col = mix( col, col2, a2 );
            return col;
        }
    ]],
})

-- nested subgroups like the real scene (layer -> workGroup -> group)
local layer = display.newGroup()
local workGroup = display.newGroup(); layer:insert(workGroup)
local group = display.newGroup(); workGroup:insert(group)

local scale = tonumber(os.getenv("EB_SCALE")) or 1
if scale ~= 1 then workGroup:scale(scale, scale) end

local d = tonumber(os.getenv("EB_D")) or 64
local sx = 1
local row = tonumber(os.getenv("EB_ROW")) or 50
local column = tonumber(os.getenv("EB_COL")) or 100
local xStart, yStart = _L, _T
local xMax, yMax = _R - d*0.5, _B - d*0.5
local mn = math.min
local count = 0
-- EB_MIX (default ON): interleave a plain non-extension image rect immediately
-- BEFORE each extension marble, in the SAME group, so both stride classes land
-- in one shared bgfx geometry pool — the exact #079 trigger (non-ext flat 68B
-- fills the pool, the marble's interleaved 136B unit appends, but the marble's
-- Draw offset doesn't account for the mixed stride and reads the non-ext bytes →
-- wh garbles → full-screen smear). Without the Renderer fix this renders garbage;
-- with it each stride class gets its own pool. Set EB_MIX=0 for the plain grid.
local mix = os.getenv("EB_MIX") ~= "0"
for i = 1, row do
    local y = mn(yStart + (i - 0.5)*d, yMax)
    for j = 1, column do
        local x = mn(xStart + (j - 0.5)*d, xMax)
        if mix then
            -- non-extension sibling sharing the pool (mirrors the gallery's
            -- custom.tiling background drawn alongside the marbles)
            local bg = display.newRect(group, x, y, d, d)
            bg.fill = { type = "image", filename = (os.getenv("EB_TEX") or "meta079.png") }
            bg:setFillColor(0.3, 0.3, 0.3)
        end
        local rect = display.newRect(group, x, y, d, d)
        rect.fill = { type = "image", filename = (os.getenv("EB_TEX") or "meta079.png") }
        rect.fillExtension = "MegaMechanicalData"
        for ii = 1, 4 do
            rect.fillExtendedData:setAttributeValue(ii, "wh", d*sx, d*sx)
            rect.fillExtendedData:setAttributeValue(ii, "tiling", 1, 1)
            rect.fillExtendedData:setAttributeValue(ii, "outlineColor", 0, 1, 0)
            rect.fillExtendedData:setAttributeValue(ii, "outlineWidth", 3)
        end
        rect.fill.effect = "filter.custom.sd_circle_cake_ext"
        count = count + 1
    end
end
print("realrepro079: created " .. count .. " marbles, scale=" .. scale .. " mix=" .. tostring(mix))
print("SCREENSHOT_READY")
