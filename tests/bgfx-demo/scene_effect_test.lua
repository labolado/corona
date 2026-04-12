local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sceneGroup = self.view
    local W = display.contentWidth
    local H = display.contentHeight

    local bg = display.newRect(sceneGroup, W/2, H/2, W, H)
    bg:setFillColor(0.15, 0.15, 0.2)

    -- Define blinked_outline effect (custom VS + varying)
    local kernel = {}
    kernel.language = "glsl"
    kernel.category = "filter"
    kernel.name = "blinked_outline2"
    kernel.isTimeDependent = true
    kernel.uniformData = {
        { name = "outlineWidth", default = 3, min = 0, max = 100, type = "float", index = 0 },
        { name = "color1", default = { 1, 0, 0, 0.8 }, min = {0,0,0,0}, max = {1,1,1,1}, type = "vec4", index = 1 },
        { name = "color2", default = { 0, 1, 0, 0.8 }, min = {0,0,0,0}, max = {1,1,1,1}, type = "vec4", index = 2 },
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

    -- Apply effect to a rect
    local rect = display.newRoundedRect(sceneGroup, W/2, H/2 - 40, 200, 150, 12)
    rect:setFillColor(0.3, 0.7, 0.3)
    rect.fill.effect = "filter.custom.blinked_outline2"
    rect.fill.effect.outlineWidth = 4
    rect.fill.effect.color1 = { 1, 0, 0, 0.8 }
    rect.fill.effect.color2 = { 0, 1, 0, 0.8 }

    local title = display.newText(sceneGroup, "Effect Test (VS+varying)", W/2, 20, native.systemFont, 14)
    title:setFillColor(1,1,1)

    local info = display.newText(sceneGroup, "Press Home, then return.\nOutline should still blink.", W/2, H - 40, native.systemFont, 11)
    info:setFillColor(0.8, 0.8, 0.8)

    local resumeCount = 0
    local status = display.newText(sceneGroup, "Resumes: 0", W/2, H - 70, native.systemFont, 12)
    status:setFillColor(1, 1, 0)

    Runtime:addEventListener("system", function(event)
        if event.type == "applicationResume" then
            resumeCount = resumeCount + 1
            status.text = "Resumes: " .. resumeCount
            print("[EFFECT_TEST] Resume #" .. resumeCount .. " effect=" .. tostring(rect.fill.effect))
        end
    end)

    print("[Scene EffectTest] Created with blinked_outline2 effect")
end

function scene:show(event) end
function scene:hide(event) end
function scene:destroy(event) end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
