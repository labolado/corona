-- test_sceneswitch.lua
-- Regression for a2d1ada1's reset pre-scan (bgfx::reset before FBO draws on a
-- viewport change). Rapidly cycles composer through FBO/effect-heavy scenes with
-- slide transitions, which is where the pre-scan's cross-frame control flow is
-- exercised. The harness checks the log for crashes and confirms the final frame
-- is not black (a stuck/blacked-out frame after switching = regression).

local composer = require("composer")

-- Mix of FBO/effect scenes (masks, custom_shader, effect_chain) and plain ones,
-- so every transition crosses a viewport/FBO boundary in both directions.
local list = { "masks", "custom_shader", "effect_chain", "blend", "images", "shapes", "animation" }

local i, rounds = 0, 0

local function nextScene()
    i = i + 1
    if i > #list then i = 1; rounds = rounds + 1 end
    if rounds >= 2 then
        print("=== SCENESWITCH DONE ===")
        return
    end
    local name = list[i]
    local ok, err = pcall(function()
        composer.gotoScene("scene_" .. name, { effect = "slideLeft", time = 200 })
    end)
    if ok then
        print("SCENESWITCH loaded=" .. name .. " round=" .. rounds)
    else
        print("SCENESWITCH ERROR scene=" .. name .. " : " .. tostring(err))
    end
    timer.performWithDelay(500, nextScene)
end

print("=== SCENESWITCH READY ===")
timer.performWithDelay(400, nextScene)
