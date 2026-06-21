-- #079 v2 regression: FAITHFUL ball_hero2 TestSpriteShader reproduction.
-- Uses the REAL sd_circle_cake_ext effect (samples image, SDF circle math with
-- v_wh/v_tiling/v_outlineColor/v_outlineWidth) on a dense grid of image-fill
-- rects, mirroring sprite_shader.lua testEllipse(shaderMode==2).

print("=== vertexext079 RECT test (FAITHFUL sd_circle_cake_ext repro) ===")
display.setStatusBar(display.HiddenStatusBar)

graphics.defineVertexExtension({
    name = "MegaMechanicalData",
    { name = "wh",           type = "float", componentCount = 2 },
    { name = "tiling",       type = "float", componentCount = 2 },
    { name = "outlineColor", type = "float", componentCount = 3 },
    { name = "outlineWidth", type = "float", componentCount = 1 },
})

-- Exact copy of ball_hero2 lib/graphics/shaders/filter/sd_circle_cake_ext.lua
graphics.defineEffect({
    category = "filter",
    name = "sd_circle_cake_ext",
    vertexExtension = "MegaMechanicalData",
    vertex = [[
varying P_POSITION vec2 v_wh;
varying P_UV vec2 v_tiling;
varying P_COLOR vec4 v_outlineColor;
varying P_POSITION float v_outlineWidth;

P_POSITION vec2 VertexKernel (P_POSITION vec2 position)
{
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

P_UV float sdPie(P_UV vec2 p, P_UV vec2 c, P_UV float r)
{
    p.x = abs(p.x);
    P_UV float l = length(p) - r;
    P_UV float m = length(p - c*clamp(dot(p,c),0.0,r) );
    return max(l,m*sign(c.y*p.x-c.x*p.y));
}
P_UV float sdCircle(P_UV vec2 p, P_UV float r ) { return length(p) - r; }
P_UV vec2 rotateCW(P_UV vec2 p, P_UV float a)
{
    P_UV mat2 m = mat2(cos(a), -sin(a), sin(a), cos(a));
    return p * m;
}
P_COLOR vec4 FragmentKernel (P_UV vec2 uv)
{
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

local W2, H2 = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W2, H2)
bg:setFillColor(0.3, 0.3, 0.3)

-- Faithful to testEllipse: 50x100 grid of 64px image-fill rects with the
-- sd_circle_cake_ext effect + per-vertex MegaMechanicalData.
local d = 64
local group = display.newGroup()
local row, column = 50, 100
local count = 0
local minF = math.min
local xMax = W2 - d * 0.5
local yMax = H2 - d * 0.5
for i = 1, row do
    local y = minF((i - 0.5) * d, yMax)
    for j = 1, column do
        local x = minF((j - 0.5) * d, xMax)
        local rect = display.newRect(group, x, y, d, d)
        rect.fill = {type = "image", filename = "meta079.png"}
        rect.fillExtension = "MegaMechanicalData"
        for ii = 1, 4 do
            rect.fillExtendedData:setAttributeValue(ii, "wh", d, d)
            rect.fillExtendedData:setAttributeValue(ii, "tiling", 1, 1)
            rect.fillExtendedData:setAttributeValue(ii, "outlineColor", 0, 1, 0) -- green
            rect.fillExtendedData:setAttributeValue(ii, "outlineWidth", 3)
        end
        rect.fill.effect = "filter.custom.sd_circle_cake_ext"
        count = count + 1
    end
end
print("vertexext079rect: created " .. count .. " sd_circle_cake_ext image rects")
print("EXPECT: grid of meta.png-textured circles each with a GREEN ring outline")

local frameN = 0
Runtime:addEventListener("enterFrame", function()
    frameN = frameN + 1
    if frameN == 30 then
        display.save( display.getCurrentStage(), {
            filename = "rect079_capture.png",
            baseDir = system.DocumentsDirectory,
            captureOffscreenArea = false,
        } )
        print("=== captured rect079_capture.png ===")
    end
end)
