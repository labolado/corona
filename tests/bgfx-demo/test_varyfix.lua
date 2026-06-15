-- #59 isolation test: a vec2 vertex->fragment custom varying on an IMAGE rect (no snapshot).
-- VS writes a constant vec2 varying; FS outputs it directly as color.
-- Fixed bgfx (and GL): solid orange (0.9, 0.3, 0). Buggy bgfx: black (varying reads 0).
print("=== varyfix test (#59 vec2 varying isolation, image rect) ===")

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0, 0, 0.15)

local kernel = {
    language = "glsl",
    category = "filter",
    name = "varytest",
    vertex = [[
        varying P_UV vec2 vGrad;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            vGrad = vec2(0.9, 0.3);
            return position;
        }
    ]],
    fragment = [[
        varying P_UV vec2 vGrad;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            return CoronaColorScale(vec4(vGrad.x, vGrad.y, 0.0, 1.0));
        }
    ]],
}
graphics.defineEffect(kernel)

local r = display.newImageRect("test_icon.png", 200, 200)
r.x = display.contentCenterX
r.y = display.contentCenterY
r.fill.effect = "filter.custom.varytest"

print("SCREENSHOT_READY")
