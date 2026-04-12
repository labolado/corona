------------------------------------------------------------------------
-- test_resume_effect.lua - 测试自定义 effect 在 resume 后是否正常
-- Entry: SOLAR2D_TEST=resume_effect
--
-- 复现步骤：
-- 1. 启动后看到绿色方块带闪烁轮廓线
-- 2. 切到后台 (Home)
-- 3. 切回来
-- 4. 观察轮廓线是否还在
------------------------------------------------------------------------

local W, H = display.contentWidth, display.contentHeight

-- 定义 blinked_outline effect (从 tank 游戏复制)
local kernel = {}
kernel.language = "glsl"
kernel.category = "filter"
kernel.name = "blinked_outline"
kernel.isTimeDependent = true

kernel.uniformData = {
    { name = "outlineWidth", default = 3, min = 0, max = 100, type = "float", index = 0 },
    { name = "color1", default = { 1, 0, 0, 0.8 }, min = { 0,0,0,0 }, max = { 1,1,1,1 }, type = "vec4", index = 1 },
    { name = "color2", default = { 0, 1, 0, 0.8 }, min = { 0,0,0,0 }, max = { 1,1,1,1 }, type = "vec4", index = 2 },
}

kernel.vertex = [[
uniform P_COLOR vec4 u_UserData1;
uniform P_COLOR vec4 u_UserData2;
varying P_COLOR vec4 outlineColor;

P_POSITION vec2 VertexKernel( P_POSITION vec2 position )
{
    P_DEFAULT float value = mod(floor(CoronaTotalTime * 1.0), 2.0);
    if (value < 0.001) {
        outlineColor = u_UserData2;
    } else {
        outlineColor = u_UserData1;
    }
    return position;
}
]]

kernel.fragment = [[
varying P_COLOR vec4 outlineColor;
uniform P_UV float u_UserData0;

P_COLOR vec4 FragmentKernel (P_UV vec2 uv)
{
    P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
    P_UV float w = u_UserData0 * CoronaTexelSize.x;
    P_UV float h = u_UserData0 * CoronaTexelSize.y;

    P_COLOR float maxa = color.a;
    P_COLOR float mina = color.a;

    P_COLOR float a;
    a = texture2D(CoronaSampler0, uv + vec2(0, -h)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(0,  h)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2(-w, 0)).a; maxa = max(a, maxa); mina = min(a, mina);
    a = texture2D(CoronaSampler0, uv + vec2( w, 0)).a; maxa = max(a, maxa); mina = min(a, mina);

    color = mix(vec4(0.0), outlineColor, maxa - mina);
    return CoronaColorScale(color);
}
]]

graphics.defineEffect(kernel)

-- 背景
display.newRect(W/2, H/2, W, H):setFillColor(0.2, 0.2, 0.3)

-- 状态文本
local statusText = display.newText({
    text = "Effect active - switch to background and return",
    x = W/2, y = 40,
    font = native.systemFont, fontSize = 14,
})

-- 创建测试对象：绿色方块 + blinked_outline effect
local rect = display.newRoundedRect(W/2, H/2 - 50, 200, 150, 12)
rect:setFillColor(0.3, 0.7, 0.3)
rect.fill.effect = "filter.custom.blinked_outline"
rect.fill.effect.outlineWidth = 4
rect.fill.effect.color1 = { 1, 0, 0, 0.8 }
rect.fill.effect.color2 = { 0, 1, 0, 0.8 }

-- 第二个测试：圆形
local circle = display.newCircle(W/2, H/2 + 120, 50)
circle:setFillColor(0.3, 0.3, 0.7)
circle.fill.effect = "filter.custom.blinked_outline"
circle.fill.effect.outlineWidth = 3
circle.fill.effect.color1 = { 1, 1, 0, 0.8 }
circle.fill.effect.color2 = { 0, 1, 1, 0.8 }

-- 帧计数和 resume 检测
local frameCount = 0
local resumeCount = 0
local effectWorking = true

local frameText = display.newText({
    text = "Frame: 0 | Resumes: 0",
    x = W/2, y = H - 30,
    font = native.systemFont, fontSize = 12,
})

Runtime:addEventListener("enterFrame", function()
    frameCount = frameCount + 1
    frameText.text = string.format("Frame: %d | Resumes: %d | Effect: %s",
        frameCount, resumeCount, rect.fill.effect and "ON" or "OFF")
end)

-- 检测 resume
Runtime:addEventListener("system", function(event)
    if event.type == "applicationResume" then
        resumeCount = resumeCount + 1
        print("[RESUME_EFFECT_TEST] Resume #" .. resumeCount)
        print("[RESUME_EFFECT_TEST] rect.fill.effect = " .. tostring(rect.fill.effect))
        print("[RESUME_EFFECT_TEST] circle.fill.effect = " .. tostring(circle.fill.effect))

        -- 延迟检查 effect 是否还在工作
        timer.performWithDelay(500, function()
            print("[RESUME_EFFECT_TEST] After 500ms: rect.fill.effect = " .. tostring(rect.fill.effect))
            -- 尝试重新设置 effect 看是否能恢复
            -- rect.fill.effect = "filter.custom.blinked_outline"
            -- rect.fill.effect.outlineWidth = 4
        end)
    elseif event.type == "applicationSuspend" then
        print("[RESUME_EFFECT_TEST] Suspend")
    end
end)

print("[RESUME_EFFECT_TEST] Test started. Switch to background and return to test.")
