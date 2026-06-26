-- Repro: does kernel.graph composite shader work in bgfx/Metal?
-- Two shapes: one single-pass SDF, one composite (graph) of two SDF passes.
-- Expected: both show colored shapes.
-- Fail: one or both show as white rectangle.

local W, H = display.contentWidth, display.contentHeight
local CX, CY = display.contentCenterX, display.contentCenterY

-- Step 1: Register two simple SDF filter shaders
local kernel1 = {
    language = "glsl", category = "filter", name = "sdf_pass1",
    fragment = [[
P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    P_UV vec2 p = (uv - 0.5) * 2.0;
    P_UV float d = length(p) - 0.8;
    P_UV float a = 1.0 - smoothstep(-0.02, 0.02, d);
    return CoronaColorScale(vec4(1.0, 0.3, 0.0, a));
}
]],
}
graphics.defineEffect(kernel1)

local kernel2 = {
    language = "glsl", category = "filter", name = "sdf_pass2",
    fragment = [[
P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    P_COLOR vec4 src = texture2D(CoronaSampler0, uv);
    P_COLOR vec4 tint = vec4(0.0, 0.5, 1.0, 1.0);
    return CoronaColorScale(mix(src, tint, 0.4));
}
]],
}
graphics.defineEffect(kernel2)

-- Step 2: Register composite shader (graph)
local compositeKernel = {
    language = "glsl", category = "filter", name = "sdf_composite",
    graph = {
        nodes = {
            sdf_pass1 = { effect = "filter.custom.sdf_pass1", input1 = "paint1" },
            sdf_pass2 = { effect = "filter.custom.sdf_pass2", input1 = "sdf_pass1" },
        },
        output = "sdf_pass2",
    },
}
graphics.defineEffect(compositeKernel)

-- Labels
local function label(txt, x, y)
    local t = display.newText(txt, x, y, native.systemFont, 14)
    t:setFillColor(0, 0, 0)
    return t
end

label("Single-pass SDF (orange circle)", CX - 100, CY - 80)
label("Composite graph SDF (tinted)", CX + 100, CY - 80)

-- Shape A: single-pass SDF
local shapeA = display.newRect(CX - 100, CY, 120, 120)
shapeA.fill.effect = "filter.custom.sdf_pass1"

-- Shape B: composite graph SDF
local shapeB = display.newRect(CX + 100, CY, 120, 120)
shapeB.fill.effect = "filter.custom.sdf_composite"

-- Status
local status = display.newText("", CX, CY + 120, native.systemFont, 16)
status:setFillColor(0, 0, 0)
status.text = string.format(
    "A effect=%s | B effect=%s",
    tostring(shapeA.fill.effect), tostring(shapeB.fill.effect)
)

print("[REPRO] composite-shader-repro started")
print("[REPRO] shapeA fill.effect =", tostring(shapeA.fill.effect))
print("[REPRO] shapeB fill.effect =", tostring(shapeB.fill.effect))
