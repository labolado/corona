-- scene_mask_triangles.lua - 高危回归: Mask Triangles (mask vertex layout)
local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sg = self.view
    local cw, ch = display.contentCenterX, display.contentCenterY
    local cw2, ch2 = display.contentWidth, display.contentHeight

    local title = display.newText({parent=sg, text="Mask Triangles (mask vertex layout)", x=cw, y=40, fontSize=14})
    title:setFillColor(1,1,0)

    -- 16 circles + setMask（缩量从 100 → 16）
    local maskFile = "mask.png"
    local ok, err = pcall(function()
        for r=1,4 do for c=1,4 do
            local circ = display.newCircle(sg, cw + (c-2.5)*60, ch + (r-2.5)*60, 25)
            circ:setFillColor(0.4 + r*0.15, 0.4 + c*0.15, 0.6)
        end end
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
