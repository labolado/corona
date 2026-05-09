-- scene_effect_chain.lua - 高危回归: Effect chain (filter composite + colorMatrix)
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Effect chain (filter composite + colorMatrix)", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    -- 加一个 rect 应用 filter（不一定每个 effect 都可用，pcall 包）
    local r = display.newRect(sg, cw, ch, 200, 100)
    r:setFillColor(0.5, 0.7, 0.9)
    pcall(function()
        r.fill.effect = "filter.colorMatrix"
    end)
    -- 第 2 个 rect 不带 effect，对照
    local r2 = display.newRect(sg, cw, ch+120, 200, 60)
    r2:setFillColor(0.9, 0.5, 0.5)
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
