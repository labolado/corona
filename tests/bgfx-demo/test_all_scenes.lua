--[[
    test_all_scenes.lua - 全场景 FPS 录制（Scenario 3）

    Walks through all 11 nav scenes, records FPS per scene.
    Auto-exits after completion.
--]]

display.setStatusBar(display.HiddenStatusBar)

local composer = require("composer")
local W, H = display.contentWidth, display.contentHeight

local scenes = {
    { name = "shapes", label = "Shapes" },
    { name = "images", label = "Images" },
    { name = "text", label = "Text" },
    { name = "transforms", label = "Transform" },
    { name = "blend", label = "Blend" },
    { name = "animation", label = "Animate" },
    { name = "groups", label = "Group" },
    { name = "physics", label = "Physics" },
    { name = "masks", label = "Mask" },
    { name = "stress", label = "Stress" },
    { name = "custom_shader", label = "Shader" },
}

local MEASURE_SECONDS = 4
local currentIdx = 0
local frameCount = 0
local measureStart = 0
local results = {}
local phase = "idle"  -- idle | warmup | measure | done

-- UI overlay. bg must sit BEHIND the composer scenes (composer.stage), otherwise
-- this opaque rect hides every scene's visuals and only the text overlay shows —
-- which makes the recorded video useless for visual regression.
local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.05, 0.05, 0.08)
bg:toBack()

local statusText = display.newText({
    text = "Initializing...", x = W/2, y = 40,
    font = native.systemFontBold, fontSize = 16
})
statusText:setFillColor(1, 1, 0)

local resultsText = display.newText({
    text = "", x = 16, y = 70,
    font = native.systemFont, fontSize = 12,
    width = W - 32, align = "left"
})
resultsText.anchorX = 0

-- Navigation button for scenes that need it
local nextBtn = display.newRect(W - 50, H - 50, 80, 40)
nextBtn:setFillColor(0.2, 0.6, 0.2)
local nextLabel = display.newText("Next>", W - 50, H - 50, native.systemFontBold, 16)
nextLabel:setFillColor(1, 1, 1)

local function gotoNextScene()
    currentIdx = currentIdx + 1
    if currentIdx > #scenes then
        phase = "done"
        statusText.text = "All Scenes Complete!"
        local out = "=== ALL SCENES FPS ===\n"
        out = out .. string.format("%-12s %8s %8s\n", "Scene", "AvgFPS", "Frames")
        local total = 0
        for _, r in ipairs(results) do
            out = out .. string.format("%-12s %8.1f %8d\n", r.name, r.fps, r.frames)
            total = total + r.fps
        end
        out = out .. string.format("%-12s %8.1f\n", "AVERAGE", total / #results)
        print(out)
        print("[PASS] all_scenes complete")
        timer.performWithDelay(500, function() os.exit(0) end)
        return
    end
    local info = scenes[currentIdx]
    statusText.text = "Loading: " .. info.label .. " (" .. currentIdx .. "/" .. #scenes .. ")"
    phase = "warmup"
    frameCount = 0
    composer.gotoScene("scene_" .. info.name, { effect = "fade", time = 200 })
    timer.performWithDelay(800, function()
        if phase == "done" then return end
        phase = "measure"
        measureStart = system.getTimer()
        frameCount = 0
        statusText.text = "Measuring: " .. info.label
    end)
end

local function onEnterFrame()
    if phase ~= "measure" then return end
    frameCount = frameCount + 1
    local elapsed = (system.getTimer() - measureStart) / 1000
    if elapsed >= MEASURE_SECONDS then
        local info = scenes[currentIdx]
        local fps = frameCount / MEASURE_SECONDS
        table.insert(results, { name = info.label, fps = fps, frames = frameCount })
        print(string.format("[Scene] %s: avg=%.1f FPS over %d frames (%.1f sec)", info.label, fps, frameCount, elapsed))

        local txt = "Scene FPS:\n"
        for _, r in ipairs(results) do
            txt = txt .. string.format("%s: %.1f\n", r.name, r.fps)
        end
        resultsText.text = txt

        timer.performWithDelay(200, gotoNextScene)
        phase = "idle"
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)

-- Start
timer.performWithDelay(500, gotoNextScene)
