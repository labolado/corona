--[[
    test_dirty.lua - Static Geometry Cache Effectiveness Test

    Usage: SOLAR2D_TEST=dirty SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Measures whether the existing static geometry cache already provides
    "dirty rect" optimization by comparing:
      Test 1: 1000 fully static rects (should have 0 geometry uploads after init)
      Test 2: 1000 every-frame moving rects (geometry re-uploaded each frame)
      Test 3: 900 static + 100 dynamic (mixed scenario)

    Key metrics: FPS, geometry uploads per frame, cache hit rate.
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Dirty Rect / Static Cache Test ===")
print("Backend: " .. backend)
print("Display: " .. display.contentWidth .. "x" .. display.contentHeight)

-- Test configuration
local OBJECT_COUNT = 1000
local WARMUP_FRAMES = 60
local MEASURE_FRAMES = 300

local tests = {
    { name = "Static (1000 rects, no movement)", dynamicCount = 0 },
    { name = "Dynamic (1000 rects, all moving)", dynamicCount = 1000 },
    { name = "Mixed (900 static + 100 dynamic)", dynamicCount = 100 },
}

local currentTest = 0
local objects = {}
local dynamicObjects = {}
local objectsGroup = nil
local frameCount = 0
local frameTimes = {}
local lastFrameTime = 0
local phase = "idle"
local results = {}
local statsSamples = {}

-- UI
local bg = display.newRect(display.contentCenterX, display.contentCenterY,
    display.contentWidth, display.contentHeight)
bg:setFillColor(0.05, 0.05, 0.08)

local titleText = display.newText({
    text = "Static Cache Test: " .. backend,
    x = display.contentCenterX, y = 30,
    font = native.systemFontBold, fontSize = 14
})
titleText:setFillColor(0.9, 0.9, 0.9)

local statusText = display.newText({
    text = "Initializing...",
    x = display.contentCenterX, y = 55,
    font = native.systemFont, fontSize = 12
})
statusText:setFillColor(0.7, 0.7, 0.7)

local resultsText = display.newText({
    text = "",
    x = 20, y = 80,
    font = native.systemFont, fontSize = 10,
    width = display.contentWidth - 40,
    align = "left"
})
resultsText.anchorX = 0
resultsText.anchorY = 0
resultsText:setFillColor(0.3, 1, 0.3)

-- Create test objects
local function createObjects(dynamicCount)
    if objectsGroup then
        objectsGroup:removeSelf()
    end
    objects = {}
    dynamicObjects = {}
    objectsGroup = display.newGroup()

    for i = 1, OBJECT_COUNT do
        local x = math.random(10, display.contentWidth - 10)
        local y = math.random(80, display.contentHeight - 30)
        local rect = display.newRect(objectsGroup, x, y, 8, 8)

        if i <= dynamicCount then
            rect:setFillColor(1, 0.3, 0.3)  -- red = dynamic
            rect.vx = (math.random() - 0.5) * 3
            rect.vy = (math.random() - 0.5) * 3
            table.insert(dynamicObjects, rect)
        else
            rect:setFillColor(0.3, 0.6, 1)  -- blue = static
        end
        table.insert(objects, rect)
    end
end

-- Update dynamic objects only
local function updateDynamic()
    local left, right = 5, display.contentWidth - 5
    local top, bottom = 80, display.contentHeight - 30
    for i = 1, #dynamicObjects do
        local obj = dynamicObjects[i]
        obj.x = obj.x + obj.vx
        obj.y = obj.y + obj.vy
        if obj.x < left or obj.x > right then
            obj.vx = -obj.vx
            obj.x = math.max(left, math.min(right, obj.x))
        end
        if obj.y < top or obj.y > bottom then
            obj.vy = -obj.vy
            obj.y = math.max(top, math.min(bottom, obj.y))
        end
    end
end

local function startNextTest()
    currentTest = currentTest + 1
    if currentTest > #tests then
        phase = "done"
        statusText.text = "Test Complete!"

        -- Print summary
        local summary = string.format("\n=== STATIC CACHE TEST RESULTS (%s) ===\n", backend)
        summary = summary .. string.format("%-35s %8s %8s %10s %10s %8s\n",
            "Test", "Avg FPS", "Min FPS", "Uploads/f", "CacheHit%", "Draws/f")
        summary = summary .. string.rep("-", 90) .. "\n"
        for _, r in ipairs(results) do
            summary = summary .. string.format("%-35s %8.1f %8.1f %10.1f %9.1f%% %8.1f\n",
                r.name, r.avgFps, r.minFps, r.avgUploads, r.avgHitRate, r.avgDraws)
        end
        summary = summary .. "\nConclusion: "
        if #results >= 2 then
            local staticUploads = results[1].avgUploads
            local dynamicUploads = results[2].avgUploads
            if staticUploads < 5 then
                summary = summary .. "Static geometry cache IS effective. "
                summary = summary .. string.format("Static: %.0f uploads/frame vs Dynamic: %.0f uploads/frame. ", staticUploads, dynamicUploads)
                summary = summary .. "No additional dirty-rect implementation needed."
            else
                summary = summary .. "Static geometry cache NOT effective. Additional dirty-rect optimization may be needed."
            end
        end
        summary = summary .. "\n=== END ===\n"
        print(summary)

        -- Write to file for automation
        local outPath = "/tmp/dirty_test_" .. backend .. "_results.txt"
        local f = io.open(outPath, "w")
        if f then
            f:write(summary)
            f:close()
            print("[DirtyTest] Results written to " .. outPath)
        end
        return
    end

    local test = tests[currentTest]
    statusText.text = string.format("Test %d/%d: %s", currentTest, #tests, test.name)
    print(string.format("[DirtyTest] Starting: %s", test.name))

    createObjects(test.dynamicCount)
    frameCount = 0
    frameTimes = {}
    statsSamples = {}
    phase = "warmup"
end

local function onEnterFrame()
    if phase == "idle" or phase == "done" then return end

    updateDynamic()

    if phase == "warmup" then
        -- Enable stats collection early so data is valid by measure phase
        if frameCount == 0 and graphics.getDirtyStats then
            graphics.getDirtyStats()
        end
        frameCount = frameCount + 1
        if frameCount >= WARMUP_FRAMES then
            phase = "measure"
            frameCount = 0
            frameTimes = {}
            statsSamples = {}
            lastFrameTime = system.getTimer()
        end
    elseif phase == "measure" then
        local now = system.getTimer()
        local dt = now - lastFrameTime
        lastFrameTime = now

        if dt > 0 then
            table.insert(frameTimes, 1000 / dt)
        end

        -- Collect dirty stats if available
        if graphics.getDirtyStats then
            local stats = graphics.getDirtyStats()
            if stats then
                table.insert(statsSamples, stats)
            end
        end

        frameCount = frameCount + 1
        if frameCount >= MEASURE_FRAMES then
            -- Calculate FPS results
            local sum, min, max = 0, 9999, 0
            for _, fps in ipairs(frameTimes) do
                sum = sum + fps
                if fps < min then min = fps end
                if fps > max then max = fps end
            end
            local avgFps = sum / math.max(#frameTimes, 1)

            -- Calculate stats averages
            local avgUploads, avgHitRate, avgDraws = 0, 0, 0
            if #statsSamples > 0 then
                local sumU, sumH, sumD = 0, 0, 0
                for _, s in ipairs(statsSamples) do
                    sumU = sumU + (s.geometryUploads or 0)
                    sumH = sumH + (s.cacheHitRate or 0)
                    sumD = sumD + (s.drawCalls or 0)
                end
                avgUploads = sumU / #statsSamples
                avgHitRate = sumH / #statsSamples
                avgDraws = sumD / #statsSamples
            end

            local test = tests[currentTest]
            local result = {
                name = test.name,
                avgFps = avgFps,
                minFps = min,
                maxFps = max,
                avgUploads = avgUploads,
                avgHitRate = avgHitRate,
                avgDraws = avgDraws,
            }
            table.insert(results, result)

            print(string.format("[DirtyTest] %s: avg=%.1f min=%.1f FPS, uploads=%.1f/f, cacheHit=%.1f%%, draws=%.1f/f",
                test.name, avgFps, min, avgUploads, avgHitRate, avgDraws))

            -- Update display
            local txt = ""
            for _, r in ipairs(results) do
                txt = txt .. string.format("%s\n  FPS: %.1f avg / %.1f min | Uploads: %.1f/f | Cache: %.1f%% | Draws: %.1f/f\n\n",
                    r.name, r.avgFps, r.minFps, r.avgUploads, r.avgHitRate, r.avgDraws)
            end
            resultsText.text = txt

            startNextTest()
        end
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)

-- Start first test after a brief delay
timer.performWithDelay(500, function()
    startNextTest()
end)
