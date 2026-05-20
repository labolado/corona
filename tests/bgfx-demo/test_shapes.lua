--[[
    test_shapes.lua - 矢量图形渲染性能（Scenario 4）

    Tests rendering of rects, circles, rounded rects at increasing counts.
    Auto-exits after completion.
--]]

display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local LEVELS = { 100, 300, 600, 1000, 2000 }
local WARMUP = 60
local MEASURE = 240

local currentLevel = 0
local shapes = {}
local frameCount = 0
local phase = "idle"
local fpsSamples = {}
local results = {}

-- UI
local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.05, 0.05, 0.08)

local statusText = display.newText({
    text = "Shapes Benchmark", x = W/2, y = 30,
    font = native.systemFontBold, fontSize = 15
})
statusText:setFillColor(0.9, 0.9, 0.9)

local logText = display.newText({
    text = "", x = 16, y = 60,
    font = native.systemFont, fontSize = 11,
    width = W - 32, align = "left"
})
logText.anchorX = 0
logText:setFillColor(0.3, 1, 0.3)

local function clearShapes()
    for _, s in ipairs(shapes) do
        if s.parent then s.parent:remove(s) else s:removeSelf() end
    end
    shapes = {}
end

local function randColor()
    return math.random(), math.random() * 0.6 + 0.3, math.random() * 0.4 + 0.3
end

local function createShapes(count)
    clearShapes()
    local types = { "rect", "circle", "roundedRect" }
    for i = 1, count do
        local t = types[(i % 3) + 1]
        local x = math.random(20, W - 20)
        local y = math.random(60, H - 30)
        local s
        if t == "rect" then
            s = display.newRect(x, y, math.random(8, 30), math.random(8, 30))
        elseif t == "circle" then
            s = display.newCircle(x, y, math.random(6, 20))
        else
            s = display.newRoundedRect(x, y, math.random(10, 35), math.random(10, 35), 5)
        end
        s:setFillColor(randColor())
        s.rotation = math.random() * 360
        s.vx, s.vy = (math.random() - 0.5) * 3, (math.random() - 0.5) * 3
        s.rotSpeed = (math.random() - 0.5) * 8
        table.insert(shapes, s)
    end
end

local function updateShapes()
    for _, s in ipairs(shapes) do
        s.x, s.y = s.x + s.vx, s.y + s.vy
        s.rotation = s.rotation + s.rotSpeed
        if s.x < 10 or s.x > W - 10 then s.vx = -s.vx end
        if s.y < 50 or s.y > H - 20 then s.vy = -s.vy end
    end
end

local function startLevel()
    currentLevel = currentLevel + 1
    if currentLevel > #LEVELS then
        phase = "done"
        statusText.text = "Complete!"
        local out = "=== SHAPES BENCHMARK ===\n"
        out = out .. string.format("%-8s %8s %8s %8s\n", "Shapes", "Avg", "Min", "Max")
        for _, r in ipairs(results) do
            out = out .. string.format("%-8d %8.1f %8.1f %8.1f\n", r.count, r.avg, r.min, r.max)
        end
        print(out)
        return
    end
    local count = LEVELS[currentLevel]
    statusText.text = "Testing " .. count .. " shapes..."
    print("[Shapes] Level: " .. count .. " shapes")
    createShapes(count)
    frameCount = 0
    fpsSamples = {}
    phase = "warmup"
end

local lastTime = 0
local function onEnterFrame()
    if phase == "idle" or phase == "done" then return end
    updateShapes()
    if phase == "warmup" then
        frameCount = frameCount + 1
        if frameCount >= WARMUP then
            phase = "measure"
            frameCount = 0
            lastTime = system.getTimer()
        end
    elseif phase == "measure" then
        local now = system.getTimer()
        local dt = now - lastTime
        lastTime = now
        if dt > 0 then table.insert(fpsSamples, 1000 / dt) end
        frameCount = frameCount + 1
        if frameCount >= MEASURE then
            local sum, mn, mx = 0, 999, 0
            for _, v in ipairs(fpsSamples) do
                sum = sum + v
                if v < mn then mn = v end
                if v > mx then mx = v end
            end
            local avg = sum / #fpsSamples
            local count = LEVELS[currentLevel]
            table.insert(results, { count = count, avg = avg, min = mn, max = mx })
            print(string.format("[Shapes] %d: avg=%.1f min=%.1f max=%.1f", count, avg, mn, mx))
            local txt = "Shapes:\n"
            for _, r in ipairs(results) do
                txt = txt .. string.format("%4d: %5.1f %5.1f %5.1f\n", r.count, r.avg, r.min, r.max)
            end
            logText.text = txt
            timer.performWithDelay(100, startLevel)
            phase = "idle"
        end
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)
timer.performWithDelay(500, startLevel)
