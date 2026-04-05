--[[
    test_culling.lua - Viewport Culling (Frustum Culling) Test
    
    Tests Solar2D's existing culling mechanism effectiveness.
    Solar2D already implements viewport culling in:
    - GroupObject::UpdateTransform() - calls CullOffscreen(screenBounds)
    - GroupObject::Draw() - skips objects with IsOffScreen() = true
    
    This test verifies the culling is working and measures performance gains.
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Viewport Culling Test ===")
print("Backend: " .. backend)
print("Display: " .. display.contentWidth .. "x" .. display.contentHeight)

-- Test configuration
local TOTAL_OBJECTS = 3000
local WARMUP_FRAMES = 60
local MEASURE_FRAMES = 180
local SCROLL_SPEED = 2

-- Scene dimensions (3x screen for scrolling test)
local SCENE_WIDTH = display.contentWidth * 3
local SCENE_HEIGHT = display.contentHeight * 3

-- UI
local bg = display.newRect(display.contentCenterX, display.contentCenterY,
    display.contentWidth, display.contentHeight)
bg:setFillColor(0.05, 0.05, 0.08)

local titleText = display.newText({
    text = "Viewport Culling Test: " .. backend,
    x = display.contentCenterX, y = 25,
    font = native.systemFontBold, fontSize = 16
})
titleText:setFillColor(0.9, 0.9, 0.9)

local statusText = display.newText({
    text = "Initializing...",
    x = display.contentCenterX, y = 50,
    font = native.systemFont, fontSize = 12
})
statusText:setFillColor(0.7, 0.7, 0.7)

local statsText = display.newText({
    text = "",
    x = 20, y = 80,
    font = native.systemFont, fontSize = 10,
    width = display.contentWidth - 40,
    align = "left"
})
statsText.anchorX = 0
statsText.anchorY = 0
statsText:setFillColor(0.3, 1, 0.3)

-- Test state
local objects = {}
local objectsGroup = nil
local frameCount = 0
local phase = "idle"  -- idle, warmup, measure
local currentTest = 0
local results = {}
local scrollX = 0
local scrollDirection = 1

-- Create objects for test
-- mode: "scattered" = 3x area (most offscreen), "onscreen" = all on screen
local function createObjects(count, mode)
    if objectsGroup then
        objectsGroup:removeSelf()
    end
    objects = {}
    objectsGroup = display.newGroup()

    local rangeX, rangeY, offsetX, offsetY
    if mode == "scattered" then
        -- 3x screen area - only ~1/3 will be visible at a time
        rangeX = SCENE_WIDTH
        rangeY = SCENE_HEIGHT
        offsetX = -display.contentWidth
        offsetY = -display.contentHeight
    else
        -- All on screen
        rangeX = display.contentWidth - 20
        rangeY = display.contentHeight - 100
        offsetX = 10
        offsetY = 80
    end

    for i = 1, count do
        local x = offsetX + math.random() * rangeX
        local y = offsetY + math.random() * rangeY
        local size = 8 + math.random() * 16
        local rect = display.newRect(objectsGroup, x, y, size, size)
        
        -- Visual variety
        local hue = (i / count) * 0.8 + 0.1
        rect:setFillColor(hue, 0.6 + math.random() * 0.4, 0.8)
        rect:setStrokeColor(hue, 0.9, 1)
        rect.strokeWidth = 1
        
        -- Animation data
        rect.vx = (math.random() - 0.5) * 2
        rect.vy = (math.random() - 0.5) * 2
        rect.rotSpeed = (math.random() - 0.5) * 5
        
        table.insert(objects, rect)
    end
    
    -- Apply initial scroll position
    objectsGroup.x = scrollX
end

-- Update objects (simulate active scene)
local function updateObjects()
    for i = 1, #objects do
        local obj = objects[i]
        obj.rotation = obj.rotation + obj.rotSpeed
        
        -- Subtle movement to keep transforms updating
        obj.x = obj.x + obj.vx * 0.1
        obj.y = obj.y + obj.vy * 0.1
    end
end

-- Scroll the scene (for scattered test)
local function updateScroll()
    scrollX = scrollX + SCROLL_SPEED * scrollDirection
    
    -- Bounce between edges
    local minX = -display.contentWidth * 2
    local maxX = 0
    if scrollX <= minX or scrollX >= maxX then
        scrollDirection = -scrollDirection
    end
    
    objectsGroup.x = scrollX
end

-- Start next test
local function startNextTest()
    currentTest = currentTest + 1
    
    if currentTest > 2 then
        phase = "done"
        statusText.text = "Test Complete!"
        return
    end
    
    local testName = (currentTest == 1) and "scattered" or "onscreen"
    local testLabel = (currentTest == 1) 
        and "Test 1: 3x Area (Culling Active)" 
        or "Test 2: On Screen (No Culling)"
    
    statusText.text = testLabel
    print("[CullingTest] Starting: " .. testLabel)
    
    scrollX = 0
    scrollDirection = 1
    createObjects(TOTAL_OBJECTS, testName)
    frameCount = 0
    phase = "warmup"
end

-- Frame timing
local frameTimes = {}
local lastFrameTime = 0
local cullStats = { culled = 0, visible = 0 }

-- Per-frame handler
local function onEnterFrame()
    if phase == "idle" or phase == "done" then return end
    
    -- Update scene
    updateObjects()
    if currentTest == 1 then
        updateScroll()  -- Scroll for scattered test
    end
    
    if phase == "warmup" then
        frameCount = frameCount + 1
        if frameCount >= WARMUP_FRAMES then
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
            
            local testName = (currentTest == 1) and "3x Area (Culling)" or "On Screen (No Culling)"
            table.insert(results, {
                name = testName,
                avg = avg,
                min = min,
                max = max
            })
            
            print(string.format("[CullingTest] %s: avg=%.1f min=%.1f max=%.1f FPS",
                testName, avg, min, max))
            
            startNextTest()
        end
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)

-- Results display timer
local function updateStats()
    if #results > 0 then
        local txt = "RESULTS:\n"
        for _, r in ipairs(results) do
            txt = txt .. string.format("%-25s: %.1f FPS (%.1f-%.1f)\n", 
                r.name, r.avg, r.min, r.max)
        end
        
        if #results == 2 then
            local speedup = results[1].avg / results[2].avg
            txt = txt .. string.format("\nSpeedup from culling: %.2fx\n", speedup)
            txt = txt .. string.format("(Higher is better - culling should keep FPS similar)")
        end
        
        statsText.text = txt
    end
    
    -- Show current object count
    if phase ~= "done" then
        local info = string.format("Objects: %d | Phase: %s | Scroll: %.0f", 
            #objects, phase, scrollX)
        if phase == "measure" then
            info = info .. string.format(" | Frame: %d/%d", frameCount, MEASURE_FRAMES)
        end
        -- Append to stats
        local currentText = statsText.text
        if currentText and currentText ~= "" then
            statsText.text = currentText .. "\n\n" .. info
        else
            statsText.text = info
        end
    end
end

timer.performWithDelay(100, updateStats, -1)

-- Auto-exit after test completes (for automated runs)
local function checkComplete()
    if phase == "done" then
        print("\n=== CULLING TEST RESULTS ===")
        for _, r in ipairs(results) do
            print(string.format("%-25s: %.1f FPS", r.name, r.avg))
        end
        if #results == 2 then
            print(string.format("Speedup: %.2fx", results[1].avg / results[2].avg))
        end
        print("============================\n")
        
        -- Auto exit after 3 seconds
        timer.performWithDelay(3000, function()
            os.exit(0)
        end)
    end
end
timer.performWithDelay(500, checkComplete, -1)

-- Start test
timer.performWithDelay(500, function()
    startNextTest()
end)
