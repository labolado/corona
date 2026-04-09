--[[
    test_stability.lua - Long-running stability test

    Usage: SOLAR2D_TEST=stability SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Runs for 10 minutes, cycling through synthetic scenes every 5 seconds.
    Each scene creates 200-500 objects, applies effects, physics, snapshots.
    Monitors memory and reports results at the end.
--]]

display.setStatusBar(display.HiddenStatusBar)

local physics = require("physics")
local W = display.contentWidth
local H = display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "unknown"

-- Config
local TEST_DURATION = 600000   -- 10 minutes in ms
local SCENE_INTERVAL = 5000    -- 5 seconds per scene
local MEMORY_LOG_INTERVAL = 30000  -- log memory every 30s

-- State
local sceneGroup = nil
local sceneCount = 0
local physicsActive = false
local startTime = system.getTimer()
local peakMemory = 0
local memoryLog = {}
local errorCount = 0

-- Available images for texture load/release testing
local images = {
    "test_red.png", "test_green.png", "test_blue.png",
    "test_cyan.png", "test_magenta.png", "test_yellow.png",
    "test_icon.png", "test_gradient.png", "grass1.png",
    "bg-village2-1.png", "test_star_alpha.png", "test_hull.png"
}

-- Available effects (only well-supported ones)
local effects = {
    "filter.blur", "filter.brightness", "filter.grayscale",
    "filter.invert", "filter.sepia"
}

print("=== STABILITY TEST START ===")
print("Backend: " .. backend)
print("Duration: " .. (TEST_DURATION / 1000) .. "s")
print("Scene interval: " .. (SCENE_INTERVAL / 1000) .. "s")
print("Time: " .. os.date())

-- Status display (persistent, outside scene group)
local statusText = display.newText({
    text = "Stability: starting...",
    x = W / 2, y = 12,
    font = native.systemFontBold, fontSize = 10
})
statusText:setFillColor(1, 1, 0)

local memText = display.newText({
    text = "",
    x = W / 2, y = H - 10,
    font = native.systemFont, fontSize = 9
})
memText:setFillColor(0.7, 0.7, 0.7)

local function getMemoryKB()
    collectgarbage("collect")
    return collectgarbage("count")
end

local function logMemory(tag)
    local luaMem = getMemoryKB()
    if luaMem > peakMemory then peakMemory = luaMem end
    local elapsed = math.floor((system.getTimer() - startTime) / 1000)
    local entry = {time = elapsed, mem = luaMem, scenes = sceneCount}
    table.insert(memoryLog, entry)
    print(string.format("MEMORY [%ds] lua=%.1fKB peak=%.1fKB scenes=%d %s",
        elapsed, luaMem, peakMemory, sceneCount, tag or ""))
    memText.text = string.format("Lua: %.0fKB  Peak: %.0fKB  Scenes: %d", luaMem, peakMemory, sceneCount)
end

-- Scene types - each creates different object patterns
local sceneBuilders = {}

-- Scene 1: Rectangles + circles
function sceneBuilders.shapes(group)
    local count = math.random(200, 400)
    for i = 1, count do
        local obj
        if math.random() > 0.5 then
            obj = display.newRect(group, math.random(0, W), math.random(20, H - 20),
                math.random(5, 40), math.random(5, 40))
        else
            obj = display.newCircle(group, math.random(0, W), math.random(20, H - 20),
                math.random(3, 20))
        end
        obj:setFillColor(math.random(), math.random(), math.random(), math.random(50, 100) / 100)
    end
    return count
end

-- Scene 2: Text objects
function sceneBuilders.text(group)
    local count = math.random(100, 200)
    local fonts = {native.systemFont, native.systemFontBold}
    for i = 1, count do
        local t = display.newText({
            parent = group,
            text = "Obj" .. i,
            x = math.random(0, W), y = math.random(20, H - 20),
            font = fonts[math.random(1, 2)],
            fontSize = math.random(8, 24)
        })
        t:setFillColor(math.random(), math.random(), math.random())
    end
    return count
end

-- Scene 3: Images (texture load/release)
function sceneBuilders.images(group)
    local count = math.random(50, 150)
    for i = 1, count do
        local imgFile = images[math.random(1, #images)]
        local ok, img = pcall(display.newImage, group, imgFile)
        if ok and img then
            img.x = math.random(0, W)
            img.y = math.random(20, H - 20)
            img.xScale = math.random(20, 80) / 100
            img.yScale = img.xScale
            img.alpha = math.random(50, 100) / 100
        end
    end
    return count
end

-- Scene 4: Groups with nested objects
function sceneBuilders.groups(group)
    local total = 0
    for g = 1, math.random(10, 20) do
        local subGroup = display.newGroup()
        group:insert(subGroup)
        subGroup.x = math.random(0, W)
        subGroup.y = math.random(20, H - 20)
        local n = math.random(10, 30)
        for i = 1, n do
            local r = display.newRect(subGroup, math.random(-30, 30), math.random(-30, 30),
                math.random(3, 15), math.random(3, 15))
            r:setFillColor(math.random(), math.random(), math.random())
        end
        total = total + n
    end
    return total
end

-- Scene 5: Physics objects
function sceneBuilders.physics(group)
    physics.start()
    physicsActive = true
    physics.setGravity(0, 9.8)
    local count = math.random(100, 200)

    -- Ground
    local ground = display.newRect(group, W / 2, H - 30, W, 20)
    ground:setFillColor(0.4, 0.4, 0.4)
    physics.addBody(ground, "static")

    for i = 1, count do
        local obj
        if math.random() > 0.5 then
            obj = display.newRect(group, math.random(10, W - 10), math.random(-200, 100),
                math.random(5, 20), math.random(5, 20))
            physics.addBody(obj, "dynamic", {density = 1, friction = 0.3, bounce = 0.2})
        else
            obj = display.newCircle(group, math.random(10, W - 10), math.random(-200, 100),
                math.random(3, 12))
            physics.addBody(obj, "dynamic", {density = 1, friction = 0.3, bounce = 0.4, radius = obj.path.radius})
        end
        obj:setFillColor(math.random(), math.random(), math.random())
    end
    return count
end

-- Scene 6: Snapshots
function sceneBuilders.snapshots(group)
    local count = math.random(5, 15)
    for i = 1, count do
        local snap = display.newSnapshot(group, math.random(40, 100), math.random(40, 100))
        snap.x = math.random(0, W)
        snap.y = math.random(20, H - 20)
        -- Add objects into snapshot
        for j = 1, math.random(5, 20) do
            local r = display.newRect(0, 0, math.random(5, 20), math.random(5, 20))
            r:setFillColor(math.random(), math.random(), math.random())
            r.x = math.random(-30, 30)
            r.y = math.random(-30, 30)
            snap.group:insert(r)
        end
        snap:invalidate()
    end
    return count
end

-- Scene 7: Effects/shaders on objects
function sceneBuilders.effects(group)
    local count = math.random(50, 150)
    for i = 1, count do
        local obj = display.newRect(group, math.random(0, W), math.random(20, H - 20),
            math.random(10, 50), math.random(10, 50))
        obj:setFillColor(math.random(), math.random(), math.random())
        -- Apply random effect (some may not exist, pcall protects)
        pcall(function()
            obj.fill.effect = effects[math.random(1, #effects)]
        end)
    end
    return count
end

-- Scene 8: Canvas textures
function sceneBuilders.canvas(group)
    local count = math.random(5, 15)
    for i = 1, count do
        local tex = graphics.newTexture({type = "canvas", width = 64, height = 64})
        if tex then
            local r = display.newRect(0, 0, 60, 60)
            r:setFillColor(math.random(), math.random(), math.random())
            tex:draw(r)
            tex:invalidate()
            r:removeSelf()

            local img = display.newImage(group, tex.filename, tex.baseDir)
            if img then
                img.x = math.random(0, W)
                img.y = math.random(20, H - 20)
            end
            tex:releaseSelf()
        end
    end
    return count
end

-- Scene 9: Mixed (all types)
function sceneBuilders.mixed(group)
    local total = 0
    -- Some shapes
    for i = 1, math.random(50, 100) do
        local r = display.newRect(group, math.random(0, W), math.random(20, H - 20),
            math.random(5, 30), math.random(5, 30))
        r:setFillColor(math.random(), math.random(), math.random())
        total = total + 1
    end
    -- Some text
    for i = 1, math.random(20, 50) do
        local t = display.newText(group, "T" .. i, math.random(0, W), math.random(20, H - 20),
            native.systemFont, math.random(8, 18))
        t:setFillColor(math.random(), math.random(), math.random())
        total = total + 1
    end
    -- Some images
    for i = 1, math.random(20, 50) do
        local imgFile = images[math.random(1, #images)]
        local ok, img = pcall(display.newImage, group, imgFile)
        if ok and img then
            img.x = math.random(0, W)
            img.y = math.random(20, H - 20)
            img.xScale = 0.5
            img.yScale = 0.5
            total = total + 1
        end
    end
    -- A snapshot
    pcall(function()
        local snap = display.newSnapshot(group, 80, 80)
        snap.x = W / 2
        snap.y = H / 2
        for j = 1, 10 do
            local c = display.newCircle(0, 0, math.random(3, 15))
            c:setFillColor(math.random(), math.random(), math.random())
            c.x = math.random(-30, 30)
            c.y = math.random(-30, 30)
            snap.group:insert(c)
        end
        snap:invalidate()
        total = total + 11
    end)
    return total
end

local sceneNames = {"shapes", "text", "images", "groups", "physics", "snapshots", "effects", "canvas", "mixed"}

local function destroyScene()
    if sceneGroup then
        -- Stop physics only if it was started
        if physicsActive then
            pcall(function() physics.stop() end)
            physicsActive = false
        end
        sceneGroup:removeSelf()
        sceneGroup = nil
        collectgarbage("collect")
        collectgarbage("collect")
    end
end

local function buildScene()
    destroyScene()

    sceneGroup = display.newGroup()
    sceneCount = sceneCount + 1

    local sceneType = sceneNames[((sceneCount - 1) % #sceneNames) + 1]
    local builder = sceneBuilders[sceneType]

    local ok, result = pcall(builder, sceneGroup)
    local objCount = ok and (result or 0) or 0

    if not ok then
        errorCount = errorCount + 1
        print(string.format("ERROR scene %d (%s): %s", sceneCount, sceneType, tostring(result)))
    end

    local elapsed = math.floor((system.getTimer() - startTime) / 1000)
    statusText.text = string.format("#%d %s (%d objs) %ds", sceneCount, sceneType, objCount, elapsed)
    statusText:toFront()
    memText:toFront()
end

local function finishTest()
    destroyScene()
    collectgarbage("collect")

    local totalTime = math.floor((system.getTimer() - startTime) / 1000)
    local finalMem = getMemoryKB()

    local firstMem = memoryLog[1] and memoryLog[1].mem or finalMem
    local memGrowth = ((finalMem - firstMem) / firstMem) * 100

    print("\n=== STABILITY TEST RESULTS ===")
    print(string.format("Backend: %s", backend))
    print(string.format("Duration: %ds", totalTime))
    print(string.format("Scenes cycled: %d", sceneCount))
    print(string.format("Errors: %d", errorCount))
    print(string.format("Memory start: %.1fKB", firstMem))
    print(string.format("Memory end: %.1fKB", finalMem))
    print(string.format("Memory peak: %.1fKB", peakMemory))
    print(string.format("Memory growth: %.1f%%", memGrowth))
    print(string.format("Verdict: %s", (errorCount == 0 and math.abs(memGrowth) < 10) and "PASS" or "FAIL"))
    print("=== END ===")

    -- Write results to file
    local f = io.open("/tmp/stability_" .. backend .. "_results.txt", "w")
    if f then
        f:write("=== STABILITY TEST RESULTS ===\n")
        f:write(string.format("Backend: %s\n", backend))
        f:write(string.format("Duration: %ds\n", totalTime))
        f:write(string.format("Scenes: %d\n", sceneCount))
        f:write(string.format("Errors: %d\n", errorCount))
        f:write(string.format("Mem start: %.1fKB\n", firstMem))
        f:write(string.format("Mem end: %.1fKB\n", finalMem))
        f:write(string.format("Mem peak: %.1fKB\n", peakMemory))
        f:write(string.format("Mem growth: %.1f%%\n", memGrowth))
        f:write(string.format("Verdict: %s\n", (errorCount == 0 and math.abs(memGrowth) < 10) and "PASS" or "FAIL"))
        f:write("\nMemory log:\n")
        f:write("Time(s)\tLuaMem(KB)\tScenes\n")
        for _, entry in ipairs(memoryLog) do
            f:write(string.format("%d\t%.1f\t%d\n", entry.time, entry.mem, entry.scenes))
        end
        f:close()
    end

    statusText.text = string.format("DONE: %ds %d scenes mem:%.0f%%",
        totalTime, sceneCount, memGrowth)
    statusText:setFillColor(errorCount == 0 and 0 or 1, errorCount == 0 and 1 or 0, 0)
end

-- Scene cycling timer
local sceneTimer = timer.performWithDelay(SCENE_INTERVAL, function()
    local elapsed = system.getTimer() - startTime
    if elapsed >= TEST_DURATION then
        finishTest()
        return
    end
    buildScene()
end, 0)

-- Memory logging timer
local memTimer = timer.performWithDelay(MEMORY_LOG_INTERVAL, function()
    local elapsed = system.getTimer() - startTime
    if elapsed >= TEST_DURATION then return end
    logMemory("periodic")
end, 0)

-- Initial scene + memory log
buildScene()
logMemory("initial")
