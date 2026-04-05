display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320

print("=== Fallback Path Test (" .. backend .. ") ===")

local WARMUP = 20
local MEASURE = 150
local pass, fail = 0, 0

local function check(name, condition)
    if condition then pass = pass + 1; print("[PASS] " .. name)
    else fail = fail + 1; print("[FAIL] " .. name) end
end

local function measureFPS(label, setupFn, callback)
    local group = setupFn()
    local frameCount, frameTimes, lastTime, phase = 0, {}, 0, "warmup"
    local function onFrame()
        frameCount = frameCount + 1
        if phase == "warmup" then
            if frameCount >= WARMUP then phase = "measure"; frameCount = 0; lastTime = system.getTimer() end
        elseif phase == "measure" then
            local now = system.getTimer()
            local dt = now - lastTime; lastTime = now
            if dt > 0 then table.insert(frameTimes, 1000/dt) end
            if frameCount >= MEASURE then
                Runtime:removeEventListener("enterFrame", onFrame)
                local sum = 0
                for _, fps in ipairs(frameTimes) do sum = sum + fps end
                local avg = sum / #frameTimes
                print(string.format("[Perf] %s: avg=%.1f FPS", label, avg))
                display.remove(group)
                if callback then callback(avg) end
            end
        end
    end
    Runtime:addEventListener("enterFrame", onFrame)
end

local tests = {}
local testIndex = 0
local function runNext()
    testIndex = testIndex + 1
    if testIndex > #tests then
        print(string.format("\n=== FALLBACK TEST RESULTS (%s) === Pass: %d | Fail: %d === END ===", backend, pass, fail))
        return
    end
    tests[testIndex]()
end

-- Test 1: SDF ON vs OFF - 500 circles should render identically
table.insert(tests, function()
    print("\n--- SDF Fallback Test ---")
    if graphics.setSDF then
        graphics.setSDF(true)
        measureFPS("SDF ON (500 circles)", function()
            local g = display.newGroup()
            for i = 1, 500 do
                local c = display.newCircle(g, math.random(20,W-20), math.random(20,H-80), 15*S)
                c:setFillColor(0.8, 0.4, 0.2)
            end
            return g
        end, function(fpsOn)
            graphics.setSDF(false)
            measureFPS("SDF OFF (500 circles)", function()
                local g = display.newGroup()
                for i = 1, 500 do
                    local c = display.newCircle(g, math.random(20,W-20), math.random(20,H-80), 15*S)
                    c:setFillColor(0.8, 0.4, 0.2)
                end
                return g
            end, function(fpsOff)
                check("SDF fallback renders without crash", true)
                check("SDF fallback FPS reasonable", fpsOff > 10)
                graphics.setSDF(true)  -- restore
                timer.performWithDelay(200, runNext)
            end)
        end)
    else
        print("[SKIP] graphics.setSDF not available")
        timer.performWithDelay(200, runNext)
    end
end)

-- Test 2: Instancing ON vs OFF - batch with 1000 slots
table.insert(tests, function()
    print("\n--- Instancing Fallback Test ---")
    if graphics.setInstancing then
        -- Need atlas for batch
        -- Create test images first
        local imageFiles = {}
        for i = 1, 3 do
            local filename = "fb_test_"..i..".png"
            table.insert(imageFiles, filename)
            local snap = display.newSnapshot(32*S, 32*S)
            local r = display.newRect(snap.group, 0, 0, 30*S, 30*S)
            r:setFillColor(0.2*i, 0.8-0.2*i, 0.5)
            snap:invalidate()
            display.save(snap, { filename=filename, baseDir=system.DocumentsDirectory, captureOffscreenArea=true })
            snap:removeSelf()
        end
        timer.performWithDelay(500, function()
            local atlas = graphics.newAtlas(imageFiles, { baseDir = system.DocumentsDirectory })
            if not atlas then
                print("[SKIP] Atlas creation failed")
                timer.performWithDelay(200, runNext)
                return
            end

            graphics.setInstancing(true)
            measureFPS("Instancing ON (1000 batch)", function()
                local batch = display.newBatch(atlas, 1000)
                local list = atlas:list()
                for i = 1, 1000 do
                    batch:add(list[((i-1)%#list)+1], math.random(20,W-20), math.random(20,H-80))
                end
                return batch
            end, function(fpsOn)
                graphics.setInstancing(false)
                measureFPS("Instancing OFF (1000 batch)", function()
                    local batch = display.newBatch(atlas, 1000)
                    local list = atlas:list()
                    for i = 1, 1000 do
                        batch:add(list[((i-1)%#list)+1], math.random(20,W-20), math.random(20,H-80))
                    end
                    return batch
                end, function(fpsOff)
                    check("Instancing fallback renders without crash", true)
                    check("Instancing fallback FPS reasonable", fpsOff > 10)
                    graphics.setInstancing(true)  -- restore
                    atlas:removeSelf()
                    timer.performWithDelay(200, runNext)
                end)
            end)
        end)
    else
        print("[SKIP] graphics.setInstancing not available")
        timer.performWithDelay(200, runNext)
    end
end)

-- Test 3: Memory leak check - toggle on/off 20 times
table.insert(tests, function()
    print("\n--- Memory Leak Toggle Test ---")
    local memBefore = collectgarbage("count")
    for i = 1, 20 do
        if graphics.setSDF then graphics.setSDF(false); graphics.setSDF(true) end
        if graphics.setInstancing then graphics.setInstancing(false); graphics.setInstancing(true) end
    end
    collectgarbage("collect")
    local memAfter = collectgarbage("count")
    local diff = memAfter - memBefore
    print(string.format("  Memory before: %.1f KB, after: %.1f KB, diff: %.1f KB", memBefore, memAfter, diff))
    check("Toggle memory leak < 10 KB", diff < 10)
    timer.performWithDelay(200, runNext)
end)

timer.performWithDelay(500, runNext)
