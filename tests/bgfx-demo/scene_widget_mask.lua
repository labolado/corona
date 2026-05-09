-- scene_widget_mask.lua - 高危回归: Widget Mask (UI scrollView mask)
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Widget Mask (UI scrollView mask)", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    -- 简化 widget_mask: scrollview-like rect with mask
    local container = display.newContainer(sg, 200, 150)
    container.x, container.y = cw, ch
    for i=1,5 do
        local item = display.newRect(container, 0, (i-3)*40, 180, 30)
        item:setFillColor(0.3 + i*0.1, 0.5, 0.7)
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
