--[[
    test_scene_switch.lua - Minimal repro for bgfx scene-switch render bug
    Usage: SOLAR2D_TEST=scene_switch SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Env vars:
      REPRO_LEVEL=1..7  complexity level (default 1)
      REPRO_AUTO=1       auto-cycle A->B->A for hands-free testing
      REPRO_CYCLES=N     number of round trips (default 1)
--]]

display.setStatusBar(display.HiddenStatusBar)

local LEVEL = tonumber(os.getenv("REPRO_LEVEL") or _G.REPRO_LEVEL or "1")
local CYCLES = tonumber(os.getenv("REPRO_CYCLES") or _G.REPRO_CYCLES or "1")
print("=== scene_switch test, LEVEL=" .. LEVEL .. " CYCLES=" .. CYCLES .. " ===")

_G.REPRO_LEVEL = LEVEL

local composer = require("composer")
composer.recycleOnSceneChange = true  -- match mech_test_copy behavior

local AUTO = os.getenv("REPRO_AUTO") or _G.REPRO_AUTO
if AUTO then
    print("[AUTO] Will auto-cycle A->B->A x" .. CYCLES)
    local delay = 0
    for c = 1, CYCLES do
        delay = delay + 1500
        local d1 = delay
        timer.performWithDelay(d1, function()
            print("[AUTO] cycle " .. c .. " -> scene_switch_b")
            composer.gotoScene("scene_switch_b", {effect = "slideLeft", time = 300})
        end)
        delay = delay + 2000
        local d2 = delay
        timer.performWithDelay(d2, function()
            print("[AUTO] cycle " .. c .. " -> scene_switch_a (return)")
            composer.gotoScene("scene_switch_a", {effect = "slideRight", time = 300})
        end)
    end
    delay = delay + 1500
    timer.performWithDelay(delay, function()
        print("[AUTO] All " .. CYCLES .. " cycles complete. Saving screenshot...")
        -- Use display.save to capture actual rendered content (works on any display)
        local stage = display.currentStage
        local cap = display.capture(stage, {saveToPhotoLibrary = false})
        if cap then
            local backend = os.getenv("SOLAR2D_BACKEND") or "unknown"
            local filename = "scene_switch_" .. backend .. ".png"
            display.save(cap, {filename = filename, baseDir = system.TemporaryDirectory})
            print("[AUTO] Saved: " .. filename .. " to TemporaryDirectory")
            display.remove(cap)
        else
            print("[AUTO] WARNING: display.capture failed")
        end
    end)
end

composer.gotoScene("scene_switch_a")
