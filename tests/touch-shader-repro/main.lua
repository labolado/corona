-- Minimal repro: rect with custom GLSL shader turns white on touch
-- Tests if fill.effect is lost/broken when display object is touched

local kernel = {}
kernel.language = "glsl"
kernel.category = "filter"
kernel.name = "sdf_rect"
kernel.uniformData = {
    {
        name = "wh",
        default = { 100, 100, 0, 0 },
        type = "vec4",
        index = 0,
    },
    {
        name = "outlineColor",
        default = { 0.2, 0.2, 0.2, 1.0 },
        type = "vec4",
        index = 1,
    },
}
kernel.fragment = [[
uniform P_COLOR vec4 u_UserData0; // wh
uniform P_COLOR vec4 u_UserData1; // outlineColor

P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
{
    P_UV vec2 p = (uv - 0.5) * 2.0;
    P_UV vec2 wh = u_UserData0.xy;
    P_UV float r = 0.15;
    P_UV vec2 d = abs(p) - (1.0 - r);
    P_UV float sdf = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;

    P_UV float border = 0.015;
    P_COLOR vec4 fillColor = vec4(0.5, 0.5, 0.55, 1.0);
    P_COLOR vec4 outlineColor = u_UserData1;
    P_UV float alpha = 1.0 - smoothstep(-border, border, sdf);
    P_UV float outline = smoothstep(-border*3.0, -border, sdf) * (1.0 - smoothstep(-border, 0.0, sdf));
    P_COLOR vec4 color = mix(fillColor, outlineColor, outline);
    color.a = alpha;
    return CoronaColorScale(color);
}
]]
graphics.defineEffect(kernel)

local W, H = display.contentWidth, display.contentHeight
local CX, CY = display.contentCenterX, display.contentCenterY

local status = display.newText("Touch the shape", CX, 30, native.systemFont, 18)
status:setFillColor(0, 0, 0)

local shape = display.newRect(CX, CY, 200, 150)
shape.fill.effect = "filter.custom.sdf_rect"
shape.fill.effect.wh = { 200, 150, 0, 0 }
shape.fill.effect.outlineColor = { 0.2, 0.2, 0.2, 1.0 }

local info = display.newText("Effect: sdf_rect | fill.effect = " .. tostring(shape.fill.effect), CX, CY + 120, native.systemFont, 14)
info:setFillColor(0, 0, 0)
info.width = W - 20

local tapCount = 0

shape:addEventListener("touch", function(e)
    if e.phase == "began" then
        tapCount = tapCount + 1
        -- Read back effect state
        local eff = shape.fill.effect
        local effStr = tostring(eff)
        status.text = string.format("Touch #%d phase=began | fill.effect=%s", tapCount, effStr)
        info.text = string.format("outlineColor=%s | setFillColor called=NO", effStr)
        print("[REPRO] touch began, fill.effect=" .. tostring(eff))
    elseif e.phase == "ended" then
        local eff = shape.fill.effect
        status.text = string.format("Touch #%d phase=ended | fill.effect=%s", tapCount, tostring(eff))
        print("[REPRO] touch ended, fill.effect=" .. tostring(eff))
    end
    return true
end)

-- Also test: setFillColor on touch (simulates what mech might do)
local shape2 = display.newRect(CX, CY + 200, 200, 150)
shape2.fill.effect = "filter.custom.sdf_rect"
shape2.fill.effect.wh = { 200, 150, 0, 0 }
shape2.fill.effect.outlineColor = { 0.8, 0.2, 0.2, 1.0 }

local label2 = display.newText("Shape2: setFillColor on touch", CX, CY + 160, native.systemFont, 14)
label2:setFillColor(0, 0, 0)

shape2:addEventListener("touch", function(e)
    if e.phase == "began" then
        -- Simulate what mech does: setFillColor
        shape2:setFillColor(1, 1, 1, 1)
        print("[REPRO] shape2 touch began, setFillColor(1,1,1,1) called")
    end
    return true
end)

print("[REPRO] test started, shapes created with sdf_rect shader")
