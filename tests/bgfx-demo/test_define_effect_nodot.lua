-- test_define_effect_nodot.lua
-- Regression for Lua shader kernels whose public names do not contain dots.
-- Covers both graphics.defineEffect({ name = "mytint" }) and the built-in
-- Lua filter "filter.color" path that previously skipped runtime bgfx compile.

local W, H = display.contentWidth, display.contentHeight
display.setDefault("background", 0.08, 0.09, 0.11)

graphics.defineEffect({
    category = "filter",
    name = "mytint",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
            color.rgb = vec3(color.r * 0.15, color.g * 1.0, color.b * 0.15);
            return CoronaColorScale(color);
        }
    ]]
})

local title = display.newText({
    text = "defineEffect no-dot + filter.color",
    x = W * 0.5,
    y = 24,
    font = native.systemFontBold,
    fontSize = 16
})
title:setFillColor(1, 1, 1)

local function makeSwatch(x, effectName, labelText)
    local group = display.newGroup()

    local bg = display.newRect(group, x, H * 0.5, W * 0.34, H * 0.42)
    bg:setFillColor(0.25, 0.25, 0.28)

    for i = 0, 5 do
        local stripe = display.newRect(group, x - W * 0.17 + (i + 0.5) * W * 0.34 / 6, H * 0.5, W * 0.34 / 6, H * 0.42)
        if i % 2 == 0 then
            stripe:setFillColor(1, 1, 1)
        else
            stripe:setFillColor(0.1, 0.2, 1)
        end
    end

    bg:toFront()
    bg.fill.effect = effectName
    bg:setFillColor(1, 0.25, 0.25)

    local label = display.newText({
        text = labelText,
        x = x,
        y = H * 0.76,
        font = native.systemFont,
        fontSize = 13
    })
    label:setFillColor(0.95, 0.95, 0.95)
end

makeSwatch(W * 0.3, "filter.custom.mytint", "custom mytint")
makeSwatch(W * 0.7, "filter.color", "built-in filter.color")

timer.performWithDelay(1200, function()
    print("[PASS] define_effect_nodot rendered")
end)
