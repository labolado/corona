-- scene_batching.lua - 高危回归: Batching (50 same-texture rects)
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Batching (50 same-texture rects)", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    -- 50 个相同纹理 rect 触发 batch 路径
    for i=1,50 do
        local x = cw + ((i-1) % 10 - 4.5) * 32
        local y = ch + (math.floor((i-1)/10) - 2) * 32
        local r = display.newRect(sg, x, y, 28, 28)
        r:setFillColor(0.5 + (i%5)*0.1, 0.4, 0.8)
    end
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
