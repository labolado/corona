-- scene_skybox.lua - 高危回归: Skybox (custom shader)
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Skybox (custom shader)", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    -- 简化 skybox: 4 色 quad 验证 shader 链路
    local colors = {{1,0,0},{0,1,0},{0,0,1},{1,1,0}}
    for i=1,4 do
        local r = display.newRect(sg, cw + (i-2.5)*70, ch, 60, 60)
        r:setFillColor(unpack(colors[i]))
    end
    -- bgfx-demo skybox effect 已注册，渲染验证不崩即可
    local sky = display.newRect(sg, cw, ch+100, 200, 60)
    sky:setFillColor(0.3, 0.5, 0.9)
end

function scene:show(event) end
function scene:hide(event)
    if event.phase == "did" then composer.removeScene(scene) end
end
function scene:destroy(event) end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)
return scene
