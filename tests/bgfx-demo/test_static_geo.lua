-- test_static_geo.lua
-- Static vs Dynamic geometry performance test
-- Creates static objects (never move) and dynamic objects (move every frame)
-- Shows FPS to measure static buffer optimization impact

local W, H = display.contentWidth, display.contentHeight
local staticGroup = display.newGroup()
local dynamicGroup = display.newGroup()

local NUM_STATIC = 500
local NUM_DYNAMIC = 500
local FPS_INTERVAL = 60
local frameCount = 0
local totalTime = 0

-- FPS display
local fpsText = display.newText({
    text = "FPS: --",
    x = W / 2,
    y = 30,
    fontSize = 20,
})
fpsText:setFillColor(1, 1, 0)

local infoText = display.newText({
    text = string.format("Static: %d  Dynamic: %d", NUM_STATIC, NUM_DYNAMIC),
    x = W / 2,
    y = 55,
    fontSize = 14,
})
infoText:setFillColor(1, 1, 1)

-- Create static objects (placed once, never moved)
for i = 1, NUM_STATIC do
    local size = math.random(5, 15)
    local obj = display.newRect(staticGroup,
        math.random(size, W - size),
        math.random(80, H - size),
        size, size)
    obj:setFillColor(0.2, 0.4, 0.8, 0.6)
end

-- Create dynamic objects (move every frame)
local dynObjs = {}
for i = 1, NUM_DYNAMIC do
    local size = math.random(5, 15)
    local obj = display.newRect(dynamicGroup,
        math.random(size, W - size),
        math.random(80, H - size),
        size, size)
    obj:setFillColor(0.8, 0.2, 0.2, 0.6)
    dynObjs[i] = {
        obj = obj,
        vx = math.random() * 2 - 1,
        vy = math.random() * 2 - 1,
    }
end

-- Results tracking
local fpsResults = {}
local resultCount = 0
local maxResults = 10

local function onFrame(event)
    frameCount = frameCount + 1
    totalTime = totalTime + (1 / display.fps)

    -- Move dynamic objects
    for i = 1, #dynObjs do
        local d = dynObjs[i]
        local obj = d.obj
        obj.x = obj.x + d.vx
        obj.y = obj.y + d.vy

        -- Bounce off edges
        if obj.x < 10 or obj.x > W - 10 then d.vx = -d.vx end
        if obj.y < 80 or obj.y > H - 10 then d.vy = -d.vy end
    end

    -- Update FPS display
    if frameCount % FPS_INTERVAL == 0 then
        local fps = FPS_INTERVAL / totalTime
        fpsText.text = string.format("FPS: %.1f", fps)
        totalTime = 0

        resultCount = resultCount + 1
        fpsResults[resultCount] = fps
        print(string.format("[static_geo] Sample %d: FPS=%.1f (static=%d dynamic=%d)",
            resultCount, fps, NUM_STATIC, NUM_DYNAMIC))

        if resultCount >= maxResults then
            -- Calculate average
            local sum = 0
            for j = 1, #fpsResults do
                sum = sum + fpsResults[j]
            end
            local avg = sum / #fpsResults
            print(string.format("[static_geo] RESULT: avg_fps=%.1f samples=%d static=%d dynamic=%d",
                avg, #fpsResults, NUM_STATIC, NUM_DYNAMIC))
            Runtime:removeEventListener("enterFrame", onFrame)
        end
    end
end

Runtime:addEventListener("enterFrame", onFrame)
