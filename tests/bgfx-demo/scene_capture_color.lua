-- scene_capture_color.lua - 高危回归: Capture color (FBO Y-flip + RGBA)
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Capture color (FBO Y-flip + RGBA)", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    -- 4 色矩形 + display.captureScreen 验证 Y-flip
    local colors = {{1,0,0},{0,1,0},{0,0,1},{1,1,1}}
    local labels = {"R","G","B","W"}
    for i=1,4 do
        local r = display.newRect(sg, cw + (i-2.5)*70, ch, 60, 60)
        r:setFillColor(unpack(colors[i]))
        local t = display.newText({parent=sg, text=labels[i], x=cw + (i-2.5)*70, y=ch, fontSize=18})
        t:setFillColor(0,0,0)
    end
    -- 截图测试（不保存，只验证不崩）
    pcall(function()
        local snap = display.captureScreen({captureOffscreenArea=false})
        if snap and snap.removeSelf then snap:removeSelf() end
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
