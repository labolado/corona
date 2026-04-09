--[[
    test_batching.lua - Draw Call Batching Test

    Usage: SOLAR2D_TEST=batching SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Tests draw call batching by creating image-based objects (GPU-stored geometry,
    indexed triangles) that bypass higher-level CPU batching.

    Test modes:
      "same"  = all objects share one texture -> maximum batching potential
      "mixed" = objects use 4 different textures -> breaks batches at texture changes

    Environment variables:
      SOLAR2D_BATCH=0  - disable batching (for A/B comparison)
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local batchEnv = os.getenv("SOLAR2D_BATCH")
print("=== Draw Call Batching Test ===")
print("Backend: " .. backend)
print("Batching: " .. (batchEnv == "0" and "DISABLED" or "ENABLED"))

-- Test configuration
local LEVELS = { 100, 500, 1000, 2000 }
local WARMUP_FRAMES = 30
local MEASURE_FRAMES = 300

local currentLevel = 0
local objects = {}
local objectsGroup = nil
local frameCount = 0
local measureStart = 0
local results = {}
local phase = "idle"
local testMode = "same"

-- Available textures for multi-texture modes
local textures = { "test_cyan.png", "test_red.png", "test_green.png", "test_blue.png" }

-- Test modes:
-- "same"    = all objects use one texture -> all batch into 1 draw
-- "grouped" = objects grouped by texture (250 cyan, 250 red, 250 green, 250 blue)
--             -> consecutive same-texture draws batch within each group
-- "shuffled"= textures alternate (cyan, red, green, blue, cyan, ...)
--             -> no consecutive matching draws, worst case
local testModes = { "same", "grouped", "shuffled" }
local testModeIdx = 0

-- UI (created first, will be behind test objects - use toFront later)
local uiGroup = display.newGroup()

local bg = display.newRect(uiGroup, display.contentCenterX, display.contentCenterY,
    display.contentWidth, display.contentHeight)
bg:setFillColor(0.05, 0.05, 0.08)

local titleText = display.newText({
    parent = uiGroup,
    text = "Batching Test: " .. backend .. " [" .. (batchEnv == "0" and "OFF" or "ON") .. "]",
    x = display.contentCenterX, y = 20,
    font = native.systemFontBold, fontSize = 13
})
titleText:setFillColor(0.9, 0.9, 0.9)

local statusText = display.newText({
    parent = uiGroup,
    text = "Initializing...",
    x = display.contentCenterX, y = 40,
    font = native.systemFont, fontSize = 11
})
statusText:setFillColor(0.7, 0.7, 0.7)

local statsText = display.newText({
    parent = uiGroup,
    text = "",
    x = display.contentCenterX, y = 58,
    font = native.systemFont, fontSize = 9
})
statsText:setFillColor(0.3, 0.8, 1)

local resultsText = display.newText({
    parent = uiGroup,
    text = "",
    x = 10, y = 78,
    font = native.systemFont, fontSize = 9,
    width = display.contentWidth - 20,
    align = "left"
})
resultsText.anchorX = 0
resultsText.anchorY = 0
resultsText:setFillColor(0.3, 1, 0.3)

-- Create objects for a level
-- Uses display.newImageRect which creates GPU-stored indexed geometry
-- that the higher-level Renderer can't batch (forces FlushBatch per object)
local function createObjects(count, mode)
    if objectsGroup then
        objectsGroup:removeSelf()
    end
    objects = {}
    objectsGroup = display.newGroup()

    for i = 1, count do
        local x = math.random(10, display.contentWidth - 10)
        local y = math.random(70, display.contentHeight - 20)
        local obj

        if mode == "same" then
            -- All share the same texture
            obj = display.newImageRect(objectsGroup, "test_cyan.png", 12, 12)
        elseif mode == "grouped" then
            -- Group by texture: first 1/4 uses tex1, next 1/4 uses tex2, etc.
            local groupSize = math.ceil(count / #textures)
            local texIdx = math.floor((i - 1) / groupSize) + 1
            if texIdx > #textures then texIdx = #textures end
            obj = display.newImageRect(objectsGroup, textures[texIdx], 12, 12)
        else
            -- Shuffled: cycle textures so no two consecutive match
            local texIdx = ((i - 1) % #textures) + 1
            obj = display.newImageRect(objectsGroup, textures[texIdx], 12, 12)
        end

        obj.x = x
        obj.y = y
        obj.vx = (math.random() - 0.5) * 3
        obj.vy = (math.random() - 0.5) * 3
        table.insert(objects, obj)
    end

    -- Bring UI above test objects
    uiGroup:toFront()
end

-- Batch stats accumulator
local batchStatsAccum = {
    totalDraws = 0,
    actualSubmits = 0,
    batchCount = 0,
    maxBatch = 0,
    samples = 0,
}

local function resetBatchAccum()
    batchStatsAccum.totalDraws = 0
    batchStatsAccum.actualSubmits = 0
    batchStatsAccum.batchCount = 0
    batchStatsAccum.maxBatch = 0
    batchStatsAccum.samples = 0
end

local function sampleBatchStats()
    local s = graphics.getDirtyStats()
    if s and s.batchTotalDraws then
        batchStatsAccum.totalDraws = batchStatsAccum.totalDraws + s.batchTotalDraws
        batchStatsAccum.actualSubmits = batchStatsAccum.actualSubmits + s.batchActualSubmits
        batchStatsAccum.batchCount = batchStatsAccum.batchCount + s.batchCount
        if s.batchMaxSize > batchStatsAccum.maxBatch then
            batchStatsAccum.maxBatch = s.batchMaxSize
        end
        batchStatsAccum.samples = batchStatsAccum.samples + 1
    end
end

-- Start next level
local function startNextLevel()
    currentLevel = currentLevel + 1
    if currentLevel > #LEVELS then
        testModeIdx = testModeIdx + 1
        if testModeIdx < #testModes then
            testMode = testModes[testModeIdx + 1]
            currentLevel = 1
        else
            -- All done
            phase = "done"
            statusText.text = "COMPLETE"
            local report = "=== BATCHING TEST RESULTS ===\n"
            report = report .. string.format("%-6s %-6s %-8s %-8s %-8s %-6s\n",
                "Mode", "Count", "Draws", "Submits", "Saved%", "FPS")
            for _, r in ipairs(results) do
                local saved = 0
                if r.avgDraws > 0 then
                    saved = (1 - r.avgSubmits / r.avgDraws) * 100
                end
                local line = string.format("%-6s %-6d %-8.0f %-8.0f %-8.1f %-6.1f",
                    r.mode, r.count, r.avgDraws, r.avgSubmits, saved, r.fps)
                report = report .. line .. "\n"
            end
            resultsText.text = report
            print(report)
            print("=== BATCHING TEST DONE ===")

            timer.performWithDelay(3000, function()
                os.exit(0)
            end)
            return
        end
    end

    local count = LEVELS[currentLevel]
    statusText.text = testMode .. " texture, " .. count .. " objects - warming up..."
    createObjects(count, testMode)
    resetBatchAccum()
    frameCount = 0
    phase = "warmup"
end

-- Frame listener
local function onFrame(event)
    if phase == "done" or phase == "idle" then return end

    -- Animate objects
    for _, obj in ipairs(objects) do
        obj.x = obj.x + obj.vx
        obj.y = obj.y + obj.vy
        if obj.x < 5 or obj.x > display.contentWidth - 5 then obj.vx = -obj.vx end
        if obj.y < 70 or obj.y > display.contentHeight - 5 then obj.vy = -obj.vy end
    end

    frameCount = frameCount + 1

    -- Update live stats display
    local s = graphics.getDirtyStats()
    if s and s.batchTotalDraws then
        statsText.text = string.format("D:%d->S:%d (B:%d max:%d) [draw:%d idx:%d]",
            s.batchTotalDraws, s.batchActualSubmits, s.batchCount, s.batchMaxSize,
            s.drawCount or 0, s.drawIndexedCount or 0)
    end

    if phase == "warmup" then
        if frameCount >= WARMUP_FRAMES then
            phase = "measure"
            measureStart = system.getTimer()
            resetBatchAccum()
            frameCount = 0
            statusText.text = testMode .. " texture, " .. LEVELS[currentLevel] .. " objects - measuring..."
        end
    elseif phase == "measure" then
        sampleBatchStats()

        if frameCount >= MEASURE_FRAMES then
            local elapsed = system.getTimer() - measureStart
            local fps = frameCount / (elapsed / 1000)
            local avgDraws = batchStatsAccum.samples > 0 and batchStatsAccum.totalDraws / batchStatsAccum.samples or 0
            local avgSubmits = batchStatsAccum.samples > 0 and batchStatsAccum.actualSubmits / batchStatsAccum.samples or 0

            table.insert(results, {
                mode = testMode,
                count = LEVELS[currentLevel],
                fps = fps,
                avgDraws = avgDraws,
                avgSubmits = avgSubmits,
                maxBatch = batchStatsAccum.maxBatch,
            })

            local saved = avgDraws > 0 and ((1 - avgSubmits / avgDraws) * 100) or 0
            print(string.format("[%s %d] FPS=%.1f draws=%.0f submits=%.0f saved=%.1f%% maxBatch=%d",
                testMode, LEVELS[currentLevel], fps, avgDraws, avgSubmits, saved, batchStatsAccum.maxBatch))

            local report = ""
            for _, r in ipairs(results) do
                local s2 = r.avgDraws > 0 and ((1 - r.avgSubmits / r.avgDraws) * 100) or 0
                report = report .. string.format("%s %d: %.1f FPS, %d->%d draws (%.0f%% saved)\n",
                    r.mode, r.count, r.fps, r.avgDraws, r.avgSubmits, s2)
            end
            resultsText.text = report

            startNextLevel()
        end
    end
end

Runtime:addEventListener("enterFrame", onFrame)

-- Start first level
timer.performWithDelay(100, function()
    graphics.getDirtyStats()
    testModeIdx = 0
    testMode = testModes[1]
    startNextLevel()
end)
