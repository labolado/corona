-- test_effect_resume.lua — 最小复现：custom VS+varying effect 在 resume 后是否失效
-- 启动: SOLAR2D_TEST=effect_resume

-- 定义带 custom vertex shader + varying 的 filter
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "test_varying",
    isTimeDependent = true,
    vertex = [[
        varying P_COLOR vec4 v_tint;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position) {
            P_DEFAULT float t = mod(CoronaTotalTime * 2.0, 2.0);
            v_tint = (t < 1.0) ? vec4(1,0,0,1) : vec4(0,1,0,1);
            return position;
        }
    ]],
    fragment = [[
        varying P_COLOR vec4 v_tint;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            P_COLOR vec4 c = texture2D(CoronaSampler0, uv);
            return CoronaColorScale(c * v_tint);
        }
    ]],
})

display.newRect(display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight):setFillColor(0.15, 0.15, 0.2)

-- 带 custom VS+varying effect 的测试对象
local r = display.newRoundedRect(display.contentCenterX, display.contentCenterY - 60, 300, 200, 20)
r:setFillColor(0.5, 0.5, 0.5)
r.fill.effect = "filter.custom.test_varying"

-- 对照组：普通 fill，无 effect
local r2 = display.newRoundedRect(display.contentCenterX, display.contentCenterY + 160, 300, 100, 12)
r2:setFillColor(0.2, 0.6, 1.0)

local txt = display.newText({
    text = "Top: custom VS+varying (should blink red/green)\nBottom: no effect (blue, control)\n\nSwitch to BG and back. Does top still blink?",
    x = display.contentCenterX, y = 40, width = display.contentWidth - 40,
    font = native.systemFont, fontSize = 14, align = "center",
})

local resumeCount = 0
Runtime:addEventListener("system", function(e)
    if e.type == "applicationResume" then
        resumeCount = resumeCount + 1
        print("[EFFECT_RESUME] resume #" .. resumeCount .. " effect=" .. tostring(r.fill.effect))
    end
end)

print("[EFFECT_RESUME] test started, effect=" .. tostring(r.fill.effect))
