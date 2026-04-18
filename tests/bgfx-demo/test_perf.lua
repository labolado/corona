--[[
    test_perf.lua - Official SDK vs bgfx Performance Benchmark

    Usage: SOLAR2D_TEST=perf SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    7 scenarios:
      A: Sprite Rain                - same-texture sprites, random pos/size/rotation
      B: Mixed                      - different textures + text + rounded rects + physics
      C: Effects                    - fill.effect + mask + snapshot
      D: Physics + Render           - dynamic physics bodies + rendering
      E: Startup Time               - object creation timing (ms)
      F: Memory                     - memory usage before/peak/after GC + texture
      G: Create/Destroy Throughput  - object creation/destruction speed

    Output format (grep-friendly):
      === PERF RESULTS ===
      Scene A: Sprite Rain
        100: avg=60.0 min=59.2
        ...
      === END RESULTS ===
--]]

display.setStatusBar(display.HiddenStatusBar)
system.setIdleTimer(false)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== PERF BENCHMARK ===")
print("Backend: " .. backend)
print("Display: " .. display.contentWidth .. "x" .. display.contentHeight)

local W = display.contentWidth
local H = display.contentHeight

-- Scale factor: all sizes relative to screen width (base = 320)
local S = W / 320

local WARMUP_FRAMES = 30
local MEASURE_FRAMES = 300

-- UI
local uiGroup = display.newGroup()
local bg = display.newRect(uiGroup, W / 2, H / 2, W, H)
bg:setFillColor(0.03, 0.03, 0.06)

local titleText = display.newText({
    parent = uiGroup,
    text = string.format("PERF BENCHMARK [%s]", backend),
    x = W / 2, y = 15 * S,
    font = native.systemFontBold, fontSize = 12 * S
})
titleText:setFillColor(0.9, 0.9, 0.9)

local statusText = display.newText({
    parent = uiGroup,
    text = "Initializing...",
    x = W / 2, y = 30 * S,
    font = native.systemFont, fontSize = 11 * S
})
statusText:setFillColor(0.7, 0.7, 0.7)

local resultsText = display.newText({
    parent = uiGroup,
    text = "",
    x = 10 * S, y = 55 * S,
    font = native.systemFont, fontSize = 9 * S,
    width = W - 20 * S, align = "left"
})
resultsText.anchorX = 0
resultsText.anchorY = 0
resultsText:setFillColor(0.3, 1, 0.3)

-- State
local sceneGroup = nil
local objects = {}
local phase = "idle"
local frameCount = 0
local results = {}
local frameTimes = {}
local lastFrameTime = 0
local pendingAdvance = false
local currentScenarioIdx = 0
local currentResultTable = nil

-- Physics
local physics = nil
local physicsStarted = false
local success, result = pcall(function() return require("physics") end)
if success then
    physics = result
    physics.start()
    physics.setGravity(0, 9.8)
    physics.setScale(30)
    physicsStarted = true
end

local function cleanup()
    if sceneGroup then
        sceneGroup:removeSelf()
        sceneGroup = nil
    end
    objects = {}
    collectgarbage("collect")
end

local function updateResultsDisplay()
    local txt = ""
    for sceneName, sceneResults in pairs(results) do
        txt = txt .. sceneName .. ":\n"
        for _, r in ipairs(sceneResults) do
            if r.customText then
                txt = txt .. "  " .. r.customText .. "\n"
            else
                txt = txt .. string.format("  %d: avg=%.1f min=%.1f\n", r.count, r.avg, r.min)
            end
        end
    end
    resultsText.text = txt
end

------------------------------------------------------------------------
-- Scene A: Sprite Rain
------------------------------------------------------------------------
local LEVELS_A = { 100, 500, 1000, 2000, 5000, 10000, 20000 }
local currentALevel = 0

local function setupSceneA(count)
    cleanup()
    sceneGroup = display.newGroup()
    for i = 1, count do
        -- Use colored rects for visibility; mix with images for texture testing
        local obj
        if i % 3 == 0 then
            obj = display.newImageRect(sceneGroup, "test_star_alpha.png", 20*S, 20*S)
        else
            obj = display.newRect(sceneGroup, 0, 0, 16*S, 16*S)
            obj:setFillColor(0.2 + math.random() * 0.8, 0.2 + math.random() * 0.8, 0.2 + math.random() * 0.8, 0.8)
        end
        obj.x = math.random(10, W - 10)
        obj.y = math.random(10, H - 10)
        obj.rotation = math.random(0, 360)
        local s = 0.5 + math.random() * 1.0
        obj.xScale = s
        obj.yScale = s
        obj.vx = (math.random() - 0.5) * 4 * S
        obj.vy = (math.random() - 0.5) * 4 * S
        obj.rotSpeed = (math.random() - 0.5) * 10
        table.insert(objects, obj)
    end
    uiGroup:toFront()
end

local function animateSceneA()
    for i = 1, #objects do
        local obj = objects[i]
        obj.x = obj.x + obj.vx
        obj.y = obj.y + obj.vy
        obj.rotation = obj.rotation + obj.rotSpeed
        if obj.x < 0 then obj.x = W elseif obj.x > W then obj.x = 0 end
        if obj.y < 50 then obj.y = H elseif obj.y > H then obj.y = 50 end
    end
end

------------------------------------------------------------------------
-- Scene B: Mixed (different textures + text + rounded rects + physics)
------------------------------------------------------------------------
local LEVELS_B = { 100, 500, 1000, 2000, 3000 }
local currentBLevel = 0
local textureFiles = {
    "grass1.png",
    "desert-track-3.png",
    "spring-track.png",
    "tank_shape-1.png",
}

local function setupSceneB(count)
    cleanup()
    sceneGroup = display.newGroup()
    local texCount = #textureFiles
    local textCount = math.min(math.floor(count * 0.1), 50)
    local uiCount = math.min(math.floor(count * 0.1), 50)
    local physCount = math.min(math.floor(count * 0.1), 50)
    local spriteCount = count - textCount - uiCount - physCount

    -- Sprites with different textures
    for i = 1, spriteCount do
        local tex = textureFiles[((i - 1) % texCount) + 1]
        local obj = display.newImageRect(sceneGroup, tex, 20*S, 20*S)
        obj.x = math.random(10, W - 10)
        obj.y = math.random(60, H - 10)
        obj.vx = (math.random() - 0.5) * 3
        obj.vy = (math.random() - 0.5) * 3
        table.insert(objects, obj)
    end

    -- Text objects
    for i = 1, textCount do
        local obj = display.newText({
            parent = sceneGroup,
            text = "T" .. i,
            x = math.random(20, W - 20),
            y = math.random(60, H - 20),
            font = native.systemFont,
            fontSize = 10*S
        })
        obj:setFillColor(math.random(), math.random(), math.random())
        obj.vx = (math.random() - 0.5) * 2
        obj.vy = (math.random() - 0.5) * 2
        table.insert(objects, obj)
    end

    -- UI rounded rects
    for i = 1, uiCount do
        local obj = display.newRoundedRect(sceneGroup,
            math.random(20, W - 20), math.random(60, H - 20), 24*S, 16*S, 4)
        obj:setFillColor(math.random(), math.random(), math.random(), 0.8)
        obj.vx = (math.random() - 0.5) * 2
        obj.vy = (math.random() - 0.5) * 2
        table.insert(objects, obj)
    end

    -- Physics bodies
    if physics then
        for i = 1, physCount do
            local obj = display.newRect(sceneGroup,
                math.random(20, W - 20), math.random(60, H / 2), 12, 12)
            obj:setFillColor(0.8, 0.4, 0.2)
            physics.addBody(obj, "dynamic", { density = 1, friction = 0.3, bounce = 0.3 })
            table.insert(objects, obj)
        end
    end

    uiGroup:toFront()
end

local function animateSceneB()
    for i = 1, #objects do
        local obj = objects[i]
        if obj.vx then
            obj.x = obj.x + obj.vx
            obj.y = obj.y + obj.vy
            if obj.x < 0 or obj.x > W then obj.vx = -obj.vx end
            if obj.y < 50 or obj.y > H then obj.vy = -obj.vy end
        end
    end
end

------------------------------------------------------------------------
-- Scene C: Effects (effect + mask + snapshot)
------------------------------------------------------------------------
local LEVELS_C = { 50, 200, 500, 1000, 3000 }
local currentCLevel = 0
local effects = {
    "filter.blur",
    "filter.brightness",
    "filter.contrast",
    "filter.saturate",
}

local function setupSceneC(count)
    cleanup()
    sceneGroup = display.newGroup()
    local effCount = math.floor(count * 0.4)
    local maskCount = math.floor(count * 0.3)
    local snapCount = count - effCount - maskCount

    -- Effect objects
    for i = 1, effCount do
        local eff = effects[((i - 1) % #effects) + 1]
        local obj = display.newRect(sceneGroup,
            math.random(20, W - 20), math.random(60, H - 20), 24, 24)
        obj:setFillColor(math.random(), math.random(), math.random())
        obj.fill.effect = eff
        obj.vx = (math.random() - 0.5) * 2
        obj.vy = (math.random() - 0.5) * 2
        table.insert(objects, obj)
    end

    -- Mask objects
    for i = 1, maskCount do
        local obj = display.newCircle(sceneGroup,
            math.random(20, W - 20), math.random(60, H - 20), 12)
        obj:setFillColor(math.random(), math.random(), math.random())
        local mask = graphics.newMask("test_mask_circle.png")
        obj:setMask(mask)
        obj.maskX = 0
        obj.maskY = 0
        obj.maskScaleX = 0.8
        obj.maskScaleY = 0.8
        obj.vx = (math.random() - 0.5) * 2
        obj.vy = (math.random() - 0.5) * 2
        table.insert(objects, obj)
    end

    -- Snapshot group objects
    for i = 1, snapCount do
        local snap = display.newSnapshot(sceneGroup, 32, 32)
        snap.x = math.random(20, W - 20)
        snap.y = math.random(60, H - 20)
        local r = display.newRect(snap.group, 0, 0, 16*S, 16*S)
        r:setFillColor(math.random(), math.random(), math.random())
        snap:invalidate()
        snap.vx = (math.random() - 0.5) * 2
        snap.vy = (math.random() - 0.5) * 2
        table.insert(objects, snap)
    end

    uiGroup:toFront()
end

local function animateSceneC()
    for i = 1, #objects do
        local obj = objects[i]
        if obj.vx then
            obj.x = obj.x + obj.vx
            obj.y = obj.y + obj.vy
            if obj.x < 0 or obj.x > W then obj.vx = -obj.vx end
            if obj.y < 50 or obj.y > H then obj.vy = -obj.vy end
        end
    end
end

------------------------------------------------------------------------
-- Scene D: Physics + Render
------------------------------------------------------------------------
local LEVELS_D = { 100, 300, 500, 800, 1000, 1500, 3000 }

local function setupSceneD(count)
    cleanup()
    sceneGroup = display.newGroup()
    for i = 1, count do
        local obj = display.newRect(sceneGroup, 0, 0, 10*S, 10*S)
        obj.x = math.random(20, W - 20)
        obj.y = math.random(60, H / 2)
        obj:setFillColor(math.random(), math.random(), math.random())
        if physics then
            physics.addBody(obj, "dynamic", { density = 1, friction = 0.3, bounce = 0.5 })
            obj:setLinearVelocity((math.random() - 0.5) * 100, (math.random() - 0.5) * 100)
            obj.angularVelocity = (math.random() - 0.5) * 200
        end
        table.insert(objects, obj)
    end
    uiGroup:toFront()
end

local function animateSceneD()
    -- Physics engine handles position updates automatically
end

------------------------------------------------------------------------
-- Scene E: Startup Time
------------------------------------------------------------------------
local LEVELS_E = { 1 }

local function setupSceneE(count)
    local times = {}
    for run = 1, 10 do
        collectgarbage("collect")
        local t0 = system.getTimer()
        local g = display.newGroup()
        -- 50 images
        for i = 1, 50 do
            local obj = display.newImageRect(g, "test_star_alpha.png", 16*S, 16*S)
            obj.x = math.random(10, W - 10)
            obj.y = math.random(60, H - 10)
        end
        -- 20 texts
        for i = 1, 20 do
            display.newText({
                parent = g, text = "Test" .. i,
                x = math.random(20, W - 20), y = math.random(60, H - 20),
                font = native.systemFont, fontSize = 10*S
            })
        end
        -- 10 rounded rects
        for i = 1, 10 do
            display.newRoundedRect(g,
                math.random(20, W - 20), math.random(60, H - 20), 24*S, 16*S, 4)
        end
        -- 5 groups
        for i = 1, 5 do
            local sg = display.newGroup()
            g:insert(sg)
            local r = display.newRect(sg, 0, 0, 20*S, 20*S)
            r:setFillColor(math.random(), math.random(), math.random())
        end
        local t1 = system.getTimer()
        table.insert(times, t1 - t0)
        g:removeSelf()
    end

    local sum, minT, maxT = 0, 9999, 0
    for _, t in ipairs(times) do
        sum = sum + t
        if t < minT then minT = t end
        if t > maxT then maxT = t end
    end
    local avgT = sum / #times

    local rt = currentResultTable
    if not rt then return end
    table.insert(rt, {
        count = 1,
        customText = string.format("startup: avg=%.1fms min=%.1fms max=%.1fms", avgT, minT, maxT)
    })
    print(string.format("[Perf] Result E startup: avg=%.1fms min=%.1fms max=%.1fms", avgT, minT, maxT))
    updateResultsDisplay()
    pendingAdvance = true
end

------------------------------------------------------------------------
-- Scene F: Memory
------------------------------------------------------------------------
local LEVELS_F = { 500, 1000, 2000 }

local function setupSceneF(count)
    collectgarbage("collect")
    local initMem = collectgarbage("count") / 1024
    local initTex = system.getInfo("textureMemoryUsed") / (1024 * 1024)

    local g = display.newGroup()
    for i = 1, count do
        local obj = display.newImageRect(g, "test_star_alpha.png", 16*S, 16*S)
        obj.x = math.random(10, W - 10)
        obj.y = math.random(60, H - 10)
    end

    local peakMem = collectgarbage("count") / 1024
    local peakTex = system.getInfo("textureMemoryUsed") / (1024 * 1024)

    g:removeSelf()
    g = nil
    collectgarbage("collect")

    local afterMem = collectgarbage("count") / 1024
    local afterTex = system.getInfo("textureMemoryUsed") / (1024 * 1024)

    local rt = currentResultTable
    if not rt then return end
    table.insert(rt, {
        count = count,
        customText = string.format("%d: init=%.1fMB peak=%.1fMB after_gc=%.1fMB tex=%.1fMB",
            count, initMem, peakMem, afterMem, peakTex)
    })
    print(string.format("[Perf] Result F %d: init=%.1fMB peak=%.1fMB after_gc=%.1fMB tex=%.1fMB",
        count, initMem, peakMem, afterMem, peakTex))
    updateResultsDisplay()
    pendingAdvance = true
end

------------------------------------------------------------------------
-- Scene G: Create/Destroy Throughput
------------------------------------------------------------------------
local LEVELS_G = { 1 }

local function setupSceneG(count)
    local ROUNDS = 50
    local OBJS_PER_ROUND = 100
    local totalTime = 0

    for round = 1, ROUNDS do
        collectgarbage("collect")
        local t0 = system.getTimer()
        local objs = {}
        for i = 1, OBJS_PER_ROUND do
            local obj = display.newImageRect("test_star_alpha.png", 16*S, 16*S)
            obj.x = math.random(10, W - 10)
            obj.y = math.random(60, H - 10)
            table.insert(objs, obj)
        end
        for i = 1, #objs do
            objs[i]:removeSelf()
        end
        local t1 = system.getTimer()
        totalTime = totalTime + (t1 - t0)
    end

    local avgRound = totalTime / ROUNDS
    local throughput = (ROUNDS * OBJS_PER_ROUND) / (totalTime / 1000)

    local rt = currentResultTable
    if not rt then return end
    table.insert(rt, {
        count = 1,
        customText = string.format("throughput: %.0f obj/sec avg_round=%.1fms", throughput, avgRound)
    })
    print(string.format("[Perf] Result G throughput: %.0f obj/sec avg_round=%.1fms", throughput, avgRound))
    updateResultsDisplay()
    pendingAdvance = true
end

------------------------------------------------------------------------
-- Benchmark engine
------------------------------------------------------------------------
local scenarios = {
    {
        name = "A",
        label = "Sprite Rain",
        levels = LEVELS_A,
        setup = setupSceneA,
        animate = animateSceneA,
        current = 0,
    },
    {
        name = "B",
        label = "Mixed",
        levels = LEVELS_B,
        setup = setupSceneB,
        animate = animateSceneB,
        current = 0,
    },
    {
        name = "C",
        label = "Effects",
        levels = LEVELS_C,
        setup = setupSceneC,
        animate = animateSceneC,
        current = 0,
    },
    {
        name = "D",
        label = "Physics + Render",
        levels = LEVELS_D,
        setup = setupSceneD,
        animate = animateSceneD,
        current = 0,
    },
    {
        name = "E",
        label = "Startup Time",
        levels = LEVELS_E,
        setup = setupSceneE,
        isCustom = true,
        current = 0,
    },
    {
        name = "F",
        label = "Memory",
        levels = LEVELS_F,
        setup = setupSceneF,
        isCustom = true,
        current = 0,
    },
    {
        name = "G",
        label = "Create/Destroy Throughput",
        levels = LEVELS_G,
        setup = setupSceneG,
        isCustom = true,
        current = 0,
    },
}

local function startNextLevel()
    local sc = scenarios[currentScenarioIdx]
    sc.current = sc.current + 1
    if sc.current > #sc.levels then
        -- Scenario done, move to next
        return false
    end

    local count = sc.levels[sc.current]
    statusText.text = string.format("Scene %s (%s): %d objects...", sc.name, sc.label, count)
    print(string.format("[Perf] Scene %s (%s) level %d", sc.name, sc.label, count))

    sc.setup(count)
    if sc.isCustom then
        phase = "custom"
    else
        frameCount = 0
        frameTimes = {}
        phase = "warmup"
    end
    return true
end

local function startNextScenario()
    currentScenarioIdx = currentScenarioIdx + 1
    if currentScenarioIdx > #scenarios then
        phase = "done"
        cleanup()
        statusText.text = "BENCHMARK COMPLETE"

        -- Print final report
        print("\n=== PERF RESULTS ===")
        for _, sc in ipairs(scenarios) do
            print(string.format("Scene %s: %s", sc.name, sc.label))
            local sceneResults = results[sc.label] or {}
            for _, r in ipairs(sceneResults) do
                if r.customText then
                    print("  " .. r.customText)
                else
                    print(string.format("  %d: avg=%.1f min=%.1f", r.count, r.avg, r.min))
                end
            end
        end
        print("=== END RESULTS ===")

        -- Upload results to local server (for WiFi devices where syslog is unavailable)
        local reportLines = {}
        for _, sc in ipairs(scenarios) do
            local sceneResults = results[sc.label] or {}
            for _, r in ipairs(sceneResults) do
                if r.customText then
                    table.insert(reportLines, string.format("[Perf] Result %s %s", sc.name, r.customText))
                else
                    table.insert(reportLines, string.format("[Perf] Result %s %d: avg=%.1f min=%.1f", sc.name, r.count, r.avg, r.min))
                end
            end
        end
        local reportText = table.concat(reportLines, "\n")

        -- Build structured metadata for data management
        local json = require("json")
        local metadata = {
            device = system.getInfo("model"),
            arch = system.getInfo("architectureInfo"),
            platform = system.getInfo("platformName"),
            platformVersion = system.getInfo("platformVersion") or "",
            backend = backend,
            contentWidth = display.contentWidth,
            contentHeight = display.contentHeight,
            pixelWidth = display.pixelWidth,
            pixelHeight = display.pixelHeight,
            date = os.date("%Y-%m-%d %H:%M:%S"),
            results = reportText,
        }
        local body = json.encode(metadata)
        local function onUpload(event) end
        local headers = { ["Content-Type"] = "application/json" }
        pcall(function()
            local network = require("network")
            network.request("http://192.168.2.89:9876/perf", "POST", onUpload,
                { body = body, headers = headers })
        end)

        -- Auto-exit
        timer.performWithDelay(2000, function()
            os.exit(0)
        end)
        return
    end

    local sc = scenarios[currentScenarioIdx]
    results[sc.label] = {}
    currentResultTable = results[sc.label]
    startNextLevel()
end

local function onEnterFrame()
    if phase == "idle" or phase == "done" then return end

    local sc = scenarios[currentScenarioIdx]

    if phase == "custom" then
        if pendingAdvance then
            pendingAdvance = false
            if not startNextLevel() then
                startNextScenario()
            end
        end
        return
    end

    if sc and sc.animate then
        sc.animate()
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
            local sum, minFps = 0, 9999
            for _, fps in ipairs(frameTimes) do
                sum = sum + fps
                if fps < minFps then minFps = fps end
            end
            local avgFps = sum / #frameTimes
            local count = sc.levels[sc.current]
            table.insert(currentResultTable, { count = count, avg = avgFps, min = minFps })
            print(string.format("[Perf] Result %s %d: avg=%.1f min=%.1f", sc.name, count, avgFps, minFps))
            updateResultsDisplay()

            if not startNextLevel() then
                startNextScenario()
            end
        end
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)

-- Start
timer.performWithDelay(300, function()
    startNextScenario()
end)
