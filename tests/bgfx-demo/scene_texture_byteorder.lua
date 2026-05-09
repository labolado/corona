-- scene_texture_byteorder.lua - 高危回归: Texture byte order (RGBA channels)
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Texture byte order (RGBA channels)", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    -- 静态 R/G/B/A 四色 rect 验证字节序不互换
    local channels = {{1,0,0,1, "R"}, {0,1,0,1, "G"}, {0,0,1,1, "B"}, {1,1,0,1, "Y"}}
    for i, c in ipairs(channels) do
        local r = display.newRect(sg, cw + (i-2.5)*70, ch, 60, 60)
        r:setFillColor(c[1], c[2], c[3], c[4])
        local t = display.newText({parent=sg, text=c[5], x=cw + (i-2.5)*70, y=ch+50, fontSize=14})
        t:setFillColor(1,1,1)
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
