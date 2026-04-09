--[[
    test_benchmark_all.lua - Comprehensive Performance Benchmark

    Usage: SOLAR2D_TEST=benchmark_all SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Runs 4 scenarios covering different optimization benefits:
      A: Same-texture mass objects (batching + instancing)
      B: Static UI layout (static geometry cache)
      C: Particle/bullet rain (instancing)
      D: Mixed real-world scene (all combined)

    Environment variables:
      SOLAR2D_BACKEND=gl/bgfx   Backend selection
      SOLAR2D_BATCH=0            Disable draw call batching
      SOLAR2D_INSTANCE=0         Disable GPU instancing

    Each scenario runs WARMUP + MEASURE frames, then reports FPS and draw stats.
    Auto-exits after completion.
--]]

display.setStatusBar(display.HiddenStatusBar)
-- Prevent screen lock during benchmark
system.setIdleTimer(false)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local batchEnv = os.getenv("SOLAR2D_BATCH") or "1"
local instanceEnv = os.getenv("SOLAR2D_INSTANCE") or "1"

print("=== COMPREHENSIVE BENCHMARK ===")
print("Backend: " .. backend)
print("Batching: " .. (batchEnv == "0" and "OFF" or "ON"))
print("Instancing: " .. (instanceEnv == "0" and "OFF" or "ON"))
print("Display: " .. display.contentWidth .. "x" .. display.contentHeight)

local W = display.contentWidth
local H = display.contentHeight

-- Timing config
local WARMUP_FRAMES = 30
local MEASURE_FRAMES = 600  -- ~10 seconds at 60fps

-- UI
local uiGroup = display.newGroup()
local bg = display.newRect(uiGroup, W / 2, H / 2, W, H)
bg:setFillColor(0.03, 0.03, 0.06)

local titleText = display.newText({
    parent = uiGroup,
    text = string.format("Benchmark [%s] batch=%s inst=%s", backend, batchEnv, instanceEnv),
    x = W / 2, y = 18,
    font = native.systemFontBold, fontSize = 12
})
titleText:setFillColor(0.9, 0.9, 0.9)

local statusText = display.newText({
    parent = uiGroup,
    text = "Starting...",
    x = W / 2, y = 36,
    font = native.systemFont, fontSize = 11
})
statusText:setFillColor(0.7, 0.7, 0.7)

local liveStats = display.newText({
    parent = uiGroup,
    text = "",
    x = W / 2, y = 52,
    font = native.systemFont, fontSize = 9
})
liveStats:setFillColor(0.3, 0.8, 1)

local resultsText = display.newText({
    parent = uiGroup,
    text = "",
    x = 10, y = 70,
    font = native.systemFont, fontSize = 9,
    width = W - 20, align = "left"
})
resultsText.anchorX = 0
resultsText.anchorY = 0
resultsText:setFillColor(0.3, 1, 0.3)

-- Shared textures (created once via snapshots)
local textureFiles = {}
local textureColors = {
    { 0.2, 0.6, 1.0 },   -- blue
    { 1.0, 0.3, 0.3 },   -- red
    { 0.3, 0.9, 0.3 },   -- green
    { 1.0, 0.8, 0.2 },   -- yellow
}

local function ensureTextures()
    if #textureFiles > 0 then return end
    for i, c in ipairs(textureColors) do
        local fname = "bench_tex_" .. i .. ".png"
        local snap = display.newSnapshot(16, 16)
        local r = display.newRect(snap.group, 0, 0, 16, 16)
        r:setFillColor(c[1], c[2], c[3])
        snap:invalidate()
        display.save(snap, { filename = fname, baseDir = system.DocumentsDirectory, captureOffscreenArea = true })
        snap:removeSelf()
        table.insert(textureFiles, fname)
    end
end

-- State
local sceneGroup = nil
local objects = {}
local phase = "idle"
local frameCount = 0
local measureStart = 0
local allResults = {}

-- Batch stats accumulator
local batchAccum = { draws = 0, submits = 0, batches = 0, maxBatch = 0, samples = 0 }

local function resetAccum()
    batchAccum.draws = 0
    batchAccum.submits = 0
    batchAccum.batches = 0
    batchAccum.maxBatch = 0
    batchAccum.samples = 0
end

local function sampleStats()
    local ok, s = pcall(function() return graphics.getDirtyStats() end)
    s = ok and s or nil
    if s and s.batchTotalDraws then
        batchAccum.draws = batchAccum.draws + s.batchTotalDraws
        batchAccum.submits = batchAccum.submits + s.batchActualSubmits
        batchAccum.batches = batchAccum.batches + s.batchCount
        if s.batchMaxSize > batchAccum.maxBatch then
            batchAccum.maxBatch = s.batchMaxSize
        end
        batchAccum.samples = batchAccum.samples + 1
    end
end

local function cleanup()
    if sceneGroup then
        sceneGroup:removeSelf()
        sceneGroup = nil
    end
    objects = {}
end

---------------------------------------------------------------------------
-- Auto-scaling: probe device speed, then scale object counts accordingly
---------------------------------------------------------------------------
local SCALE = 1

-- Synchronous probe: create 2000 objects, measure one frame time
local function probeDeviceSpeed()
    local probeGroup = display.newGroup()
    for i = 1, 2000 do
        local r = display.newRect(probeGroup, math.random(0, W), math.random(0, H), 6, 6)
        r:setFillColor(math.random(), math.random(), math.random())
    end
    -- Measure how long one frame update takes
    local t0 = system.getTimer()
    -- Force a layout pass
    for i = 1, probeGroup.numChildren do
        local c = probeGroup[i]
        c.x = c.x + 0.001
    end
    local elapsed = system.getTimer() - t0
    probeGroup:removeSelf()
    collectgarbage("collect")

    -- elapsed < 2ms → very fast (A17 Pro etc), scale up aggressively
    -- elapsed < 5ms → fast, scale up moderately
    -- elapsed < 15ms → normal
    -- elapsed > 15ms → slow device
    if elapsed < 2 then
        SCALE = 5      -- 10000/2750/15000/10000 objects
    elseif elapsed < 5 then
        SCALE = 3      -- 6000/1650/9000/6000
    elseif elapsed < 15 then
        SCALE = 2      -- 4000/1100/6000/4000
    else
        SCALE = 1      -- 2000/550/3000/2000 (default)
    end
    print(string.format("Device probe: %.1fms for 2000 objects → scale=%dx", elapsed, SCALE))
end

probeDeviceSpeed()
-- Mobile devices need higher counts to break VSync
local platform = system.getInfo("platform")
local env = system.getInfo("environment")
print("Platform: " .. tostring(platform) .. " env: " .. tostring(env))
-- Solar2D returns "iPhone OS" or "Android" not "ios"/"android"
if platform ~= "Mac OS X" and env ~= "simulator" then
    SCALE = math.max(SCALE, 5)
    print("Mobile override: scale=" .. SCALE)
end

local function sc(base) return math.floor(base * SCALE) end

---------------------------------------------------------------------------
-- Scenario A: Same-texture mass objects
-- Benefits from: batching (same texture = mergeable draws) + instancing
---------------------------------------------------------------------------
local function setupSceneA()
    cleanup()
    ensureTextures()
    sceneGroup = display.newGroup()
    local tex = textureFiles[1]
    local count = sc(2000)
    scenarios[1].count = count
    scenarios[1].label = string.format("Same-texture %d", count)

    for i = 1, count do
        local obj = display.newImageRect(sceneGroup, tex, system.DocumentsDirectory, 10, 10)
        obj.x = math.random(5, W - 5)
        obj.y = math.random(60, H - 5)
        obj.vx = (math.random() - 0.5) * 3
        obj.vy = (math.random() - 0.5) * 3
        table.insert(objects, obj)
    end
    uiGroup:toFront()
end

---------------------------------------------------------------------------
-- Scenario B: Static UI layout (500 static + 50 dynamic)
-- Benefits from: static geometry caching
---------------------------------------------------------------------------
local function setupSceneB()
    cleanup()
    ensureTextures()
    sceneGroup = display.newGroup()

    -- Static UI elements (no movement)
    local staticCount = sc(500)
    scenarios[2].count = staticCount + sc(50)
    scenarios[2].label = string.format("Static UI %d+%d", staticCount, sc(50))
    for i = 1, staticCount do
        local col = math.floor((i - 1) / 25)
        local row = (i - 1) % 25
        local obj = display.newImageRect(sceneGroup,
            textureFiles[((i - 1) % #textureFiles) + 1],
            system.DocumentsDirectory, 14, 14)
        obj.x = 10 + col * 16
        obj.y = 65 + row * 16
        obj.isStatic = true
        table.insert(objects, obj)
    end

    -- Dynamic overlay elements
    for i = 1, sc(50) do
        local obj = display.newRect(sceneGroup,
            math.random(20, W - 20),
            math.random(80, H - 20), 8, 8)
        obj:setFillColor(1, 1, 0, 0.8)
        obj.vx = (math.random() - 0.5) * 4
        obj.vy = (math.random() - 0.5) * 4
        obj.isStatic = false
        table.insert(objects, obj)
    end
    uiGroup:toFront()
end

---------------------------------------------------------------------------
-- Scenario C: Particle/bullet rain (3000 same-shape circles, all moving)
-- Benefits from: instancing (identical geometry, many instances)
---------------------------------------------------------------------------
local function setupSceneC()
    cleanup()
    sceneGroup = display.newGroup()

    local particleCount = sc(3000)
    scenarios[3].count = particleCount
    scenarios[3].label = string.format("Particles %d", particleCount)
    for i = 1, particleCount do
        local obj = display.newCircle(sceneGroup,
            math.random(5, W - 5),
            math.random(60, H - 5), 3)
        local hue = (i / particleCount)
        obj:setFillColor(hue, 0.4 + hue * 0.3, 1 - hue * 0.5, 0.8)
        obj.vx = (math.random() - 0.5) * 5
        obj.vy = math.random() * 3 + 1  -- falling down
        obj.isStatic = false
        table.insert(objects, obj)
    end
    uiGroup:toFront()
end

---------------------------------------------------------------------------
-- Scenario D: Mixed real-world (1000 static + 500 dynamic + 500 particles)
-- Benefits from: all optimizations combined
---------------------------------------------------------------------------
local function setupSceneD()
    cleanup()
    ensureTextures()
    sceneGroup = display.newGroup()

    local sCount, dCount, pCount = sc(1000), sc(500), sc(500)
    scenarios[4].count = sCount + dCount + pCount
    scenarios[4].label = string.format("Mixed %d+%d+%d", sCount, dCount, pCount)

    -- Static background/terrain tiles
    for i = 1, sCount do
        local obj = display.newImageRect(sceneGroup,
            textureFiles[((i - 1) % #textureFiles) + 1],
            system.DocumentsDirectory, 12, 12)
        obj.x = math.random(5, W - 5)
        obj.y = math.random(60, H - 5)
        obj.isStatic = true
        table.insert(objects, obj)
    end

    -- Dynamic "characters"
    for i = 1, dCount do
        local obj = display.newRect(sceneGroup,
            math.random(10, W - 10),
            math.random(60, H - 10), 8, 8)
        obj:setFillColor(0.9, 0.5, 0.2)
        obj.vx = (math.random() - 0.5) * 3
        obj.vy = (math.random() - 0.5) * 3
        obj.isStatic = false
        table.insert(objects, obj)
    end

    -- "Particles" (same shape, instancable)
    for i = 1, pCount do
        local obj = display.newCircle(sceneGroup,
            math.random(5, W - 5),
            math.random(60, H - 5), 2)
        obj:setFillColor(1, 1, 0.5, 0.6)
        obj.vx = (math.random() - 0.5) * 6
        obj.vy = math.random() * 4 + 1
        obj.isStatic = false
        table.insert(objects, obj)
    end
    uiGroup:toFront()
end

-- Animate dynamic objects
local function animateObjects()
    for i = 1, #objects do
        local obj = objects[i]
        if not obj.isStatic and obj.vx then
            obj.x = obj.x + obj.vx
            obj.y = obj.y + obj.vy
            -- Wrap around for particles (scene C/D), bounce for others
            if obj.y > H + 5 then
                obj.y = 55
                obj.x = math.random(5, W - 5)
            end
            if obj.x < 0 then obj.x = W
            elseif obj.x > W then obj.x = 0 end
            if obj.y < 55 then
                obj.vy = math.abs(obj.vy)
            end
        end
    end
end

-- Scenario definitions
local scenarios = {
    { name = "A", label = "Same-texture 2000", setup = setupSceneA, count = 2000 },
    { name = "B", label = "Static UI 500+50",  setup = setupSceneB, count = 550 },
    { name = "C", label = "Particles 3000",    setup = setupSceneC, count = 3000 },
    { name = "D", label = "Mixed 1000+500+500", setup = setupSceneD, count = 2000 },
}

local currentScenario = 0

local function startNextScenario()
    currentScenario = currentScenario + 1
    if currentScenario > #scenarios then
        -- All done - print final report
        phase = "done"
        cleanup()
        statusText.text = "BENCHMARK COMPLETE"

        local header = string.format("=== BENCHMARK RESULTS [%s batch=%s inst=%s] ===",
            backend, batchEnv, instanceEnv)
        print("\n" .. header)
        print(string.format("%-22s %8s %8s %8s %10s %10s",
            "Scenario", "AvgFPS", "MinFPS", "MaxFPS", "AvgDraws", "AvgSubmits"))
        print(string.rep("-", 72))

        local report = ""
        for _, r in ipairs(allResults) do
            local line = string.format("%-22s %8.1f %8.1f %8.1f %10.0f %10.0f",
                r.label, r.avgFps, r.minFps, r.maxFps, r.avgDraws, r.avgSubmits)
            print(line)
            local saved = r.avgDraws > 0 and ((1 - r.avgSubmits / r.avgDraws) * 100) or 0
            report = report .. string.format("%s: %.1f FPS (draws %d->%d, %.0f%% saved)\n",
                r.label, r.avgFps, r.avgDraws, r.avgSubmits, saved)
        end
        print("=== END BENCHMARK ===")
        resultsText.text = report

        -- Save results to Documents directory (pull via idevice tools)
        local header = string.format("SCALE=%d TIME=%s PLATFORM=%s\n",
            SCALE, os.date("%H:%M:%S"), tostring(system.getInfo("platform")))
        local path = system.pathForFile("benchmark_results.txt", system.DocumentsDirectory)
        if path then
            local f = io.open(path, "w")
            if f then
                f:write(header .. report)
                f:close()
                print("Results saved to: " .. path)
            else
                print("ERROR: cannot open file for writing: " .. path)
            end
        else
            print("ERROR: pathForFile returned nil")
        end

        -- Show large on-screen results for screenshot capture
        cleanup()
        if resultsText then resultsText.isVisible = false end
        if liveStats then liveStats.isVisible = false end

        local summaryGroup = display.newGroup()
        local summaryBg = display.newRect(summaryGroup, W / 2, H / 2, W, H)
        summaryBg:setFillColor(0, 0, 0)

        local summaryTitle = display.newText({
            parent = summaryGroup,
            text = string.format("BENCHMARK [%s] batch=%s inst=%s", backend, batchEnv, instanceEnv),
            x = W / 2, y = 40,
            font = native.systemFontBold, fontSize = 16
        })
        summaryTitle:setFillColor(1, 1, 0)

        local yPos = 80
        for _, r in ipairs(allResults) do
            local saved = r.avgDraws > 0 and ((1 - r.avgSubmits / r.avgDraws) * 100) or 0
            local line = string.format("%s: %.1f FPS  (draws %d->%d, %.0f%% saved)",
                r.label, r.avgFps, r.avgDraws, r.avgSubmits, saved)
            local t = display.newText({
                parent = summaryGroup,
                text = line,
                x = W / 2, y = yPos,
                font = native.systemFontBold, fontSize = 14
            })
            if r.avgFps >= 55 then
                t:setFillColor(0.3, 1, 0.3)
            elseif r.avgFps >= 30 then
                t:setFillColor(1, 0.8, 0.2)
            else
                t:setFillColor(1, 0.3, 0.3)
            end
            yPos = yPos + 30
        end

        -- Auto-exit after 10 seconds (enough time for screenshot)
        timer.performWithDelay(10000, function()
            os.exit(0)
        end)
        return
    end

    local sc = scenarios[currentScenario]
    statusText.text = string.format("Scene %s: %s - setting up...", sc.name, sc.label)
    print(string.format("\n--- Scene %s: %s (%d objects) ---", sc.name, sc.label, sc.count))

    sc.setup()
    frameCount = 0
    resetAccum()
    phase = "warmup"
    statusText.text = string.format("Scene %s: %s - warming up...", sc.name, sc.label)
end

-- Frame handler
local frameTimes = {}
local lastFrameTime = 0

local function onEnterFrame()
    if phase == "idle" or phase == "done" then return end

    animateObjects()

    -- Live stats
    local ok, s = pcall(function() return graphics.getDirtyStats() end)
    s = ok and s or nil
    if s and s.batchTotalDraws then
        liveStats.text = string.format("D:%d->S:%d B:%d max:%d",
            s.batchTotalDraws, s.batchActualSubmits, s.batchCount, s.batchMaxSize)
    end

    if phase == "warmup" then
        frameCount = frameCount + 1
        if frameCount >= WARMUP_FRAMES then
            phase = "measure"
            frameCount = 0
            frameTimes = {}
            lastFrameTime = system.getTimer()
            measureStart = lastFrameTime
            resetAccum()
            local sc = scenarios[currentScenario]
            statusText.text = string.format("Scene %s: %s - measuring...", sc.name, sc.label)
        end
    elseif phase == "measure" then
        local now = system.getTimer()
        local dt = now - lastFrameTime
        lastFrameTime = now

        if dt > 0 then
            table.insert(frameTimes, 1000 / dt)
        end
        sampleStats()

        frameCount = frameCount + 1
        if frameCount >= MEASURE_FRAMES then
            -- Compute results
            local sum, minFps, maxFps = 0, 9999, 0
            for _, fps in ipairs(frameTimes) do
                sum = sum + fps
                if fps < minFps then minFps = fps end
                if fps > maxFps then maxFps = fps end
            end
            local avgFps = #frameTimes > 0 and (sum / #frameTimes) or 0
            local avgDraws = batchAccum.samples > 0 and (batchAccum.draws / batchAccum.samples) or 0
            local avgSubmits = batchAccum.samples > 0 and (batchAccum.submits / batchAccum.samples) or 0

            local sc = scenarios[currentScenario]
            local result = {
                name = sc.name,
                label = sc.label,
                count = sc.count,
                avgFps = avgFps,
                minFps = minFps,
                maxFps = maxFps,
                avgDraws = avgDraws,
                avgSubmits = avgSubmits,
                maxBatch = batchAccum.maxBatch,
            }
            table.insert(allResults, result)

            local saved = avgDraws > 0 and ((1 - avgSubmits / avgDraws) * 100) or 0
            print(string.format("  Result: %.1f FPS (min=%.1f max=%.1f) draws=%.0f->%.0f (%.1f%% saved) maxBatch=%d",
                avgFps, minFps, maxFps, avgDraws, avgSubmits, saved, batchAccum.maxBatch))

            -- Update display
            local report = ""
            for _, r in ipairs(allResults) do
                local s2 = r.avgDraws > 0 and ((1 - r.avgSubmits / r.avgDraws) * 100) or 0
                report = report .. string.format("%s: %.1f FPS (draws %d->%d, %.0f%%)\n",
                    r.label, r.avgFps, r.avgDraws, r.avgSubmits, s2)
            end
            resultsText.text = report

            startNextScenario()
        end
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)

-- Start
timer.performWithDelay(500, function()
    startNextScenario()
end)
