-- SDF vs Mesh performance benchmark
-- Usage: SOLAR2D_TEST=sdf SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

local W = display.contentWidth
local H = display.contentHeight

print("=== SDF PERFORMANCE BENCHMARK ===")
print("Backend: " .. (os.getenv("SOLAR2D_BACKEND") or "gl"))
print("Screen: " .. W .. "x" .. H)
print("")

local results = {}

-- Utility: measure FPS over N frames
local function measureFPS(label, setupFn, frames, callback)
    local objects = setupFn()
    local frameCount = 0
    local startTime = system.getTimer()

    local function onFrame()
        frameCount = frameCount + 1
        if frameCount >= frames then
            Runtime:removeEventListener("enterFrame", onFrame)
            local elapsed = system.getTimer() - startTime
            local fps = frameCount / (elapsed / 1000)

            -- Cleanup
            for i = #objects, 1, -1 do
                if objects[i].removeSelf then
                    objects[i]:removeSelf()
                end
                objects[i] = nil
            end

            callback(fps, elapsed)
        end
    end

    Runtime:addEventListener("enterFrame", onFrame)
end

-- Test configs
local NUM_SHAPES = 1000
local NUM_FRAMES = 200

-- Test 1: Circles
local function createCircles()
    local objs = {}
    for i = 1, NUM_SHAPES do
        local c = display.newCircle(
            math.random(20, W - 20),
            math.random(20, H - 20),
            math.random(5, 20)
        )
        c:setFillColor(math.random(), math.random(), math.random())
        objs[#objs + 1] = c
    end
    return objs
end

-- Test 2: Rects
local function createRects()
    local objs = {}
    for i = 1, NUM_SHAPES do
        local r = display.newRect(
            math.random(20, W - 20),
            math.random(20, H - 20),
            math.random(10, 40),
            math.random(10, 40)
        )
        r:setFillColor(math.random(), math.random(), math.random())
        objs[#objs + 1] = r
    end
    return objs
end

-- Test 3: RoundedRects
local function createRoundedRects()
    local objs = {}
    for i = 1, NUM_SHAPES do
        local r = display.newRoundedRect(
            math.random(20, W - 20),
            math.random(20, H - 20),
            math.random(15, 40),
            math.random(15, 40),
            math.random(3, 8)
        )
        r:setFillColor(math.random(), math.random(), math.random())
        objs[#objs + 1] = r
    end
    return objs
end

-- Test 4: Mixed (500 circle + 500 rect)
local function createMixed()
    local objs = {}
    for i = 1, 500 do
        local c = display.newCircle(
            math.random(20, W - 20),
            math.random(20, H - 20),
            math.random(5, 20)
        )
        c:setFillColor(math.random(), math.random(), math.random())
        objs[#objs + 1] = c
    end
    for i = 1, 500 do
        local r = display.newRect(
            math.random(20, W - 20),
            math.random(20, H - 20),
            math.random(10, 40),
            math.random(10, 40)
        )
        r:setFillColor(math.random(), math.random(), math.random())
        objs[#objs + 1] = r
    end
    return objs
end

-- Test 5: Polygons (triangles)
local function createPolygons()
    local objs = {}
    for i = 1, NUM_SHAPES do
        local s = math.random(10, 25)
        local p = display.newPolygon(
            math.random(30, W - 30),
            math.random(30, H - 30),
            { 0, -s, s, s, -s, s }  -- triangle
        )
        p:setFillColor(math.random(), math.random(), math.random())
        objs[#objs + 1] = p
    end
    return objs
end

-- Run tests sequentially
local tests = {
    { name = "circles",      fn = createCircles },
    { name = "rects",        fn = createRects },
    { name = "roundedRects", fn = createRoundedRects },
    { name = "mixed",        fn = createMixed },
    { name = "polygons",     fn = createPolygons },
}

local testIndex = 0

local function runNextTest()
    testIndex = testIndex + 1
    if testIndex > #tests then
        -- All tests done, print summary
        print("")
        print("=== SDF BENCHMARK RESULTS ===")
        print(string.format("%-15s  %8s  %8s  %8s", "Test", "SDF FPS", "Mesh FPS", "Delta"))
        for _, r in ipairs(results) do
            local delta = r.sdfFps - r.meshFps
            local pct = (delta / r.meshFps) * 100
            print(string.format("%-15s  %8.1f  %8.1f  %+7.1f (%+.1f%%)",
                r.name, r.sdfFps, r.meshFps, delta, pct))
        end
        print("=== END ===")
        timer.performWithDelay(500, function() os.exit(0) end)
        return
    end

    local t = tests[testIndex]
    print(string.format("[Bench %d/%d] %s (%d shapes, %d frames each)",
        testIndex, #tests, t.name, NUM_SHAPES, NUM_FRAMES))

    -- Run with SDF ON
    math.randomseed(42) -- consistent seed
    graphics.setSDF(true)
    measureFPS(t.name .. " SDF", t.fn, NUM_FRAMES, function(sdfFps, sdfMs)
        print(string.format("  SDF ON:  %.1f FPS (%.0f ms)", sdfFps, sdfMs))

        -- Run with SDF OFF
        math.randomseed(42) -- same seed for fair comparison
        graphics.setSDF(false)
        measureFPS(t.name .. " Mesh", t.fn, NUM_FRAMES, function(meshFps, meshMs)
            print(string.format("  SDF OFF: %.1f FPS (%.0f ms)", meshFps, meshMs))

            results[#results + 1] = {
                name = t.name,
                sdfFps = sdfFps,
                meshFps = meshFps,
            }

            -- Re-enable SDF for next test
            graphics.setSDF(true)

            -- Next test
            timer.performWithDelay(100, runNextTest)
        end)
    end)
end

-- Start tests
timer.performWithDelay(500, runNextTest)
