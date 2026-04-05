display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320

print("=== Draw Call & SDF Performance Test ===")
print("Backend: " .. backend)
print("Display: " .. W .. "x" .. H)

local WARMUP = 30
local MEASURE = 200

-- 公共函数：测量 FPS
local function measureFPS(label, setupFn, cleanupFn)
    -- setupFn 创建对象，返回 group
    local group = setupFn()
    
    local frameCount = 0
    local frameTimes = {}
    local lastTime = 0
    local phase = "warmup"
    
    local function onFrame()
        frameCount = frameCount + 1
        if phase == "warmup" then
            if frameCount >= WARMUP then
                phase = "measure"
                frameCount = 0
                lastTime = system.getTimer()
            end
        elseif phase == "measure" then
            local now = system.getTimer()
            local dt = now - lastTime
            lastTime = now
            if dt > 0 then table.insert(frameTimes, 1000/dt) end
            if frameCount >= MEASURE then
                Runtime:removeEventListener("enterFrame", onFrame)
                -- Calculate results
                local sum, min, max = 0, 999, 0
                for _, fps in ipairs(frameTimes) do
                    sum = sum + fps
                    if fps < min then min = fps end
                    if fps > max then max = fps end
                end
                local avg = sum / #frameTimes
                print(string.format("[Perf] %s: avg=%.1f min=%.1f max=%.1f FPS", label, avg, min, max))
                -- Cleanup
                if cleanupFn then cleanupFn(group) end
                display.remove(group)
            end
        end
    end
    Runtime:addEventListener("enterFrame", onFrame)
end

-- 用 timer 串行执行每个测试（每个需要 ~4 秒）
local tests = {}
local testIndex = 0

local function runNextTest()
    testIndex = testIndex + 1
    if testIndex > #tests then
        print("\n=== ALL DRAW CALL TESTS COMPLETE ===")
        return
    end
    tests[testIndex]()
end

-- ========================================
-- Test 1: 1000 同色 rect（最佳合批场景）
-- 预期：bgfx 合批后只需 ~1 draw call，GL 需要 1000
-- ========================================
table.insert(tests, function()
    print("\n--- Test 1: 1000 same-color rects (best case for batching) ---")
    measureFPS("1000 same-color rects", function()
        local g = display.newGroup()
        for i = 1, 1000 do
            local r = display.newRect(g, math.random(20, W-20), math.random(20, H-80), 20*S, 20*S)
            r:setFillColor(0.5, 0.3, 0.8)  -- 全部同色
        end
        return g
    end, nil)
    timer.performWithDelay(5000, runNextTest)
end)

-- ========================================
-- Test 2: 1000 不同色 rect（最差合批场景）
-- 预期：无法合批，对照组
-- ========================================
table.insert(tests, function()
    print("\n--- Test 2: 1000 different-color rects (worst case, no batching) ---")
    measureFPS("1000 diff-color rects", function()
        local g = display.newGroup()
        for i = 1, 1000 do
            local r = display.newRect(g, math.random(20, W-20), math.random(20, H-80), 20*S, 20*S)
            r:setFillColor(math.random(), math.random(), math.random())
        end
        return g
    end, nil)
    timer.performWithDelay(5000, runNextTest)
end)

-- ========================================
-- Test 3: 1000 同色 circle（SDF + 合批）
-- ========================================
table.insert(tests, function()
    print("\n--- Test 3: 1000 same-color circles (SDF + batching) ---")
    measureFPS("1000 same-color circles", function()
        local g = display.newGroup()
        for i = 1, 1000 do
            local c = display.newCircle(g, math.random(20, W-20), math.random(20, H-80), 10*S)
            c:setFillColor(0.8, 0.4, 0.2)
        end
        return g
    end, nil)
    timer.performWithDelay(5000, runNextTest)
end)

-- ========================================
-- Test 4: 500 同色 rect + 500 同色 circle（交替，测合批中断）
-- ========================================
table.insert(tests, function()
    print("\n--- Test 4: 500 rect + 500 circle alternating (batch breaking) ---")
    measureFPS("500r+500c alternating", function()
        local g = display.newGroup()
        for i = 1, 500 do
            local r = display.newRect(g, math.random(20, W-20), math.random(20, H-80), 20*S, 20*S)
            r:setFillColor(0.5, 0.3, 0.8)
            local c = display.newCircle(g, math.random(20, W-20), math.random(20, H-80), 10*S)
            c:setFillColor(0.5, 0.3, 0.8)
        end
        return g
    end, nil)
    timer.performWithDelay(5000, runNextTest)
end)

-- ========================================
-- Test 5: 2000 同色 rect（压力测试）
-- ========================================
table.insert(tests, function()
    print("\n--- Test 5: 2000 same-color rects (stress) ---")
    measureFPS("2000 same-color rects", function()
        local g = display.newGroup()
        for i = 1, 2000 do
            local r = display.newRect(g, math.random(20, W-20), math.random(20, H-80), 15*S, 15*S)
            r:setFillColor(0.3, 0.7, 0.4)
        end
        return g
    end, nil)
    timer.performWithDelay(5000, runNextTest)
end)

-- ========================================
-- Test 6: 1000 同色 roundedRect + 描边（SDF + stroke）
-- ========================================
table.insert(tests, function()
    print("\n--- Test 6: 1000 same-color roundedRects with stroke ---")
    measureFPS("1000 roundedRect+stroke", function()
        local g = display.newGroup()
        for i = 1, 1000 do
            local r = display.newRoundedRect(g, math.random(20, W-20), math.random(20, H-80), 25*S, 25*S, 5*S)
            r:setFillColor(0.2, 0.6, 0.9)
            r.strokeWidth = 2*S
            r:setStrokeColor(1, 1, 1)
        end
        return g
    end, nil)
    timer.performWithDelay(5000, runNextTest)
end)

-- 启动
timer.performWithDelay(500, runNextTest)
