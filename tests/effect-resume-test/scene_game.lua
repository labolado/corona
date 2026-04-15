local composer = require("composer")
local scene = composer.newScene()

local items = {}

function scene:create(event)
    local sceneGroup = self.view
    local cx, cy = display.contentCenterX, display.contentCenterY

    -- Background
    local bg = display.newRect(sceneGroup, cx, cy, display.actualContentWidth, display.actualContentHeight)
    bg:setFillColor(0.12, 0.12, 0.18)

    local label = display.newText(sceneGroup, "Touch rects for outline. Home+back to test.", cx, cy - 130, native.systemFont, 11)
    label:setFillColor(0.9, 0.9, 0.3)

    -- KEY CONDITION: Use snapshot (tank game components are inside snapshots)
    local snap = display.newSnapshot(sceneGroup, 300, 200)
    snap.x, snap.y = cx, cy - 20

    -- Add objects INSIDE the snapshot (like tank assembly parts)
    local colors = {
        {0.8, 0.2, 0.2},
        {0.2, 0.8, 0.2},
        {0.2, 0.2, 0.8},
        {0.8, 0.8, 0.2},
    }
    local offsets = {
        {-60, -40}, {60, -40},
        {-60, 40},  {60, 40},
    }

    for i, off in ipairs(offsets) do
        local r = display.newRoundedRect(snap.group, off[1], off[2], 90, 55, 10)
        r:setFillColor(unpack(colors[i]))
        r.id = "item_" .. i
        items[#items + 1] = r

        -- Touch: apply effect dynamically (like tank tutorial)
        r:addEventListener("touch", function(e)
            if e.phase == "began" then
                r.fill.effect = "filter.custom.blinked_outline"
                r.fill.effect.outlineWidth = 3
                r.fill.effect.color1 = {0, 0.8, 1, 0.5}
                r.fill.effect.color2 = {1, 1, 0, 0.5}
                snap:invalidate()
                print("[TEST] Effect ON: " .. r.id .. " (in snapshot)")
            elseif e.phase == "ended" or e.phase == "cancelled" then
                r.fill.effect = nil
                snap:invalidate()
                print("[TEST] Effect OFF: " .. r.id)
            end
            return true
        end)
    end
    snap:invalidate()

    -- Persistent test_blink in snapshot
    local blinkRect = display.newRoundedRect(snap.group, -60, 80, 100, 40, 8)
    blinkRect:setFillColor(0.8, 0.8, 0.8)
    blinkRect.fill.effect = "filter.custom.test_blink"
    blinkRect.id = "snap_blink"
    items[#items + 1] = blinkRect

    -- Persistent blinked_outline in snapshot
    local outRect = display.newRoundedRect(snap.group, 60, 80, 100, 40, 8)
    outRect:setFillColor(0.6, 0.3, 0.8)
    outRect.fill.effect = "filter.custom.blinked_outline"
    outRect.fill.effect.outlineWidth = 4
    outRect.fill.effect.color1 = {1, 0, 0, 0.6}
    outRect.fill.effect.color2 = {0, 1, 0, 0.6}
    outRect.id = "snap_outline"
    items[#items + 1] = outRect
    snap:invalidate()

    -- Also test effects OUTSIDE snapshot for comparison
    local outsideBlinkRect = display.newRoundedRect(sceneGroup, cx - 60, cy + 110, 100, 40, 8)
    outsideBlinkRect:setFillColor(0.8, 0.8, 0.8)
    outsideBlinkRect.fill.effect = "filter.custom.test_blink"
    outsideBlinkRect.id = "outside_blink"

    local outsideOutlineRect = display.newRoundedRect(sceneGroup, cx + 60, cy + 110, 100, 40, 8)
    outsideOutlineRect:setFillColor(0.6, 0.3, 0.8)
    outsideOutlineRect.fill.effect = "filter.custom.blinked_outline"
    outsideOutlineRect.fill.effect.outlineWidth = 4
    outsideOutlineRect.fill.effect.color1 = {1, 0, 0, 0.6}
    outsideOutlineRect.fill.effect.color2 = {0, 1, 0, 0.6}
    outsideOutlineRect.id = "outside_outline"

    local snapLabel = display.newText(sceneGroup, "IN snapshot ^  |  Outside v", cx, cy + 75, native.systemFont, 9)
    snapLabel:setFillColor(0.7, 0.7, 0.7)
    local outsideLabel = display.newText(sceneGroup, "Left: test_blink  Right: blinked_outline", cx, cy + 135, native.systemFont, 9)
    outsideLabel:setFillColor(0.7, 0.7, 0.7)

    print("[TEST] Scene created with snapshot + effects inside/outside")
end

function scene:show(event)
    if event.phase == "did" then print("[TEST] Scene shown") end
end

function scene:hide(event)
    if event.phase == "did" then print("[TEST] Scene hidden") end
end

function scene:destroy(event)
    print("[TEST] Scene destroyed")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
