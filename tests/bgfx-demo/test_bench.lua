--[[
    test_bench.lua - Automated Performance Benchmark

    Usage: SOLAR2D_TEST=bench SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Runs multiple stress levels and outputs FPS data.
    Auto-exits after completion for scripted benchmarks.
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Performance Benchmark ===")
print("Backend: " .. backend)
print("Display: " .. display.contentWidth .. "x" .. display.contentHeight)

-- Benchmark configuration
local LEVELS = { 500, 1000, 2000, 3000, 5000 }
local WARMUP_FRAMES = 30       -- skip initial frames
local MEASURE_FRAMES = 300     -- measure over 300 frames (~5 seconds)

local currentLevel = 0
local objects = {}
local objectsGroup = nil
local frameCount = 0
local measureStart = 0
local results = {}
local phase = "idle"  -- idle, warmup, measure

-- UI
local bg = display.newRect(display.contentCenterX, display.contentCenterY,
    display.contentWidth, display.contentHeight)
bg:setFillColor(0.05, 0.05, 0.08)

local titleText = display.newText({
    text = "Performance Benchmark: " .. backend,
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
    x = 20, y = 90,
    font = native.systemFont, fontSize = 11,
    width = display.contentWidth - 40,
    align = "left"
})
resultsText.anchorX = 0
resultsText.anchorY = 0
resultsText:setFillColor(0.3, 1, 0.3)

-- Create objects for a level
local function createObjects(count)
    if objectsGroup then
        objectsGroup:removeSelf()
    end
    objects = {}
    objectsGroup = display.newGroup()

    for i = 1, count do
        local x = math.random(10, display.contentWidth - 10)
        local y = math.random(80, display.contentHeight - 30)
        local rect = display.newRect(objectsGroup, x, y, 6, 6)
        local hue = (i / count) * 0.8 + 0.1
        rect:setFillColor(hue, 0.6 + math.random() * 0.4, 0.8)
        rect.vx = (math.random() - 0.5) * 4
        rect.vy = (math.random() - 0.5) * 4
        rect.rotationSpeed = (math.random() - 0.5) * 10
        table.insert(objects, rect)
    end
end

-- Update all objects
local function updateObjects()
    local left, right = 5, display.contentWidth - 5
    local top, bottom = 80, display.contentHeight - 30
    for i = 1, #objects do
        local obj = objects[i]
        obj.x = obj.x + obj.vx
        obj.y = obj.y + obj.vy
        obj.rotation = obj.rotation + obj.rotationSpeed
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

-- Start next benchmark level
local function startNextLevel()
    currentLevel = currentLevel + 1
    if currentLevel > #LEVELS then
        -- All done
        phase = "done"
        statusText.text = "Benchmark Complete!"

        -- Print summary
        local summary = string.format("\n=== BENCHMARK RESULTS (%s) ===\n", backend)
        summary = summary .. string.format("%-10s %8s %8s %8s\n", "Objects", "Avg FPS", "Min FPS", "Max FPS")
        summary = summary .. string.rep("-", 40) .. "\n"
        for _, r in ipairs(results) do
            summary = summary .. string.format("%-10d %8.1f %8.1f %8.1f\n", r.count, r.avg, r.min, r.max)
        end
        summary = summary .. "=== END ===\n"
        print(summary)
        return
    end

    local count = LEVELS[currentLevel]
    statusText.text = "Testing " .. count .. " objects..."
    print("[Bench] Starting level: " .. count .. " objects")

    createObjects(count)
    frameCount = 0
    phase = "warmup"
end

-- Per-frame handler
local frameTimes = {}
local lastFrameTime = 0

local function onEnterFrame()
    if phase == "idle" or phase == "done" then return end

    updateObjects()

    if phase == "warmup" then
        frameCount = frameCount + 1
        if frameCount >= WARMUP_FRAMES then
            -- Start measuring
            phase = "measure"
            frameCount = 0
            frameTimes = {}
            lastFrameTime = system.getTimer()
        end
    elseif phase == "measure" then
        local now = system.getTimer()
        local dt = now - lastFrameTime
        lastFrameTime = now

        if dt > 0 then
            table.insert(frameTimes, 1000 / dt)
        end

        frameCount = frameCount + 1
        if frameCount >= MEASURE_FRAMES then
            -- Calculate results
            local sum, min, max = 0, 999, 0
            for _, fps in ipairs(frameTimes) do
                sum = sum + fps
                if fps < min then min = fps end
                if fps > max then max = fps end
            end
            local avg = sum / #frameTimes

            local count = LEVELS[currentLevel]
            table.insert(results, { count = count, avg = avg, min = min, max = max })
            print(string.format("[Bench] %d objects: avg=%.1f min=%.1f max=%.1f FPS",
                count, avg, min, max))

            -- Update results display
            local txt = string.format("%-10s %8s %8s %8s\n", "Objects", "Avg", "Min", "Max")
            txt = txt .. string.rep("-", 38) .. "\n"
            for _, r in ipairs(results) do
                txt = txt .. string.format("%-10d %8.1f %8.1f %8.1f\n", r.count, r.avg, r.min, r.max)
            end
            resultsText.text = txt

            -- Next level
            startNextLevel()
        end
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)

-- Start benchmark
timer.performWithDelay(500, function()
    phase = "idle"
    startNextLevel()
end)
