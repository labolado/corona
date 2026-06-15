-- #59 rigorous: a vec2 vertex->fragment varying that GRADIENTS across the quad.
-- VS writes vGrad = normalized position (0..1, 0..1); FS outputs (vGrad.x, vGrad.y, 0).
-- Correct: a red(left)->yellow(right) x green(bottom) gradient.
-- If the varying width were wrong (#59 swizzle bug), one channel would be flat/zero.
print("=== vary59 test (gradient vec2 varying) ===")
display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0, 0, 0.15)

local kernel = {
    language = "glsl", category = "filter", name = "grad59",
    vertex = [[
        varying P_UV vec2 vGrad;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            vGrad = CoronaTexCoord;   // 0..1 across the quad
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

local r = display.newImageRect("test_icon.png", 400, 400)
r.x = display.contentCenterX
r.y = display.contentCenterY
r.fill.effect = "filter.custom.grad59"

print("SCREENSHOT_READY")
