-- scene_physics_uaf.lua - 高危回归: Physics body lifecycle
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Physics body lifecycle", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    local physics = require("physics")
    physics.start()
    local objs = {}
    for i=1,5 do
        local b = display.newCircle(sg, cw + (i-3)*40, ch, 15)
        b:setFillColor(0.8, 0.3, 0.3)
        physics.addBody(b, "dynamic", {radius=15})
        objs[i] = b
    end
    -- 1.5s 后销毁所有 body 验证 lifecycle 不崩
    timer.performWithDelay(1500, function()
        for _, b in ipairs(objs) do
            if b and b.removeSelf then b:removeSelf() end
        end
        physics.stop()
    end)
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
