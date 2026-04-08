--[[
    test_instancing.lua - GPU Instancing test

    Tests:
    1. BatchObject GPU instancing vs CPU fallback
    2. Auto-batching of regular display objects (command-buffer level)
    3. Verifies SOLAR2D_INSTANCE=0 fallback path

    Usage:
      SOLAR2D_TEST=instancing SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...
    Disable instancing:
      SOLAR2D_INSTANCE=0  (BatchObject uses CPU vertices, auto-instancing disabled)
    Disable all batching:
      SOLAR2D_BATCH=0
--]]

display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local instancing = os.getenv("SOLAR2D_INSTANCE") or "1"
print("=== INSTANCING TEST (" .. backend .. ", instance=" .. instancing .. ") ===")

local W = display.contentWidth
local H = display.contentHeight

-- FPS tracker
local frameCount = 0
local startTime = system.getTimer()
local fpsText = display.newText("FPS: --", W / 2, 20, native.systemFont, 14)
fpsText:setFillColor(1, 1, 0)
local titleText = display.newText("", W / 2, H - 20, native.systemFont, 12)
titleText:setFillColor(0, 1, 0)

local function updateFPS()
    frameCount = frameCount + 1
    local elapsed = (system.getTimer() - startTime) / 1000
    if elapsed >= 1 then
        local fps = frameCount / elapsed
        fpsText.text = string.format("FPS: %.1f", fps)
        frameCount = 0
        startTime = system.getTimer()
    end
end
Runtime:addEventListener("enterFrame", updateFPS)

local results = {}

-- Scene 1: BatchObject instancing (atlas-based, GPU instanced draw)
local function testBatchInstancing(count, callback)
    titleText.text = "BatchObject: " .. count .. " sprites"
    print(string.format("\n--- BatchObject instancing: %d sprites ---", count))

    local imageFiles = {}
    for i = 1, 4 do
        local filename = "inst_tile_" .. i .. ".png"
        table.insert(imageFiles, filename)
        local snap = display.newSnapshot(20, 20)
        local bg = display.newRect(snap.group, 0, 0, 20, 20)
        bg:setFillColor(0.2 + 0.2 * i, 0.3, 1 - 0.15 * i)
        snap:invalidate()
        display.save(snap, { filename = filename, baseDir = system.DocumentsDirectory, captureOffscreenArea = true })
        snap:removeSelf()
    end

    local atlas = graphics.newAtlas(imageFiles, { baseDir = system.DocumentsDirectory })
    if not atlas then
        print("[FAIL] Could not create atlas")
        callback(0)
        return
    end

    local batch = display.newBatch(atlas, count)
    for i = 1, count do
        local fname = imageFiles[((i - 1) % #imageFiles) + 1]
        batch:add(fname,
            math.random(10, W - 10),
            math.random(40, H - 40),
            { rotation = math.random(0, 360), alpha = 0.5 + math.random() * 0.5 })
    end
    print(string.format("  Batch slots: %d", batch:count()))

    frameCount = 0
    startTime = system.getTimer()

    timer.performWithDelay(3000, function()
        local elapsed = (system.getTimer() - startTime) / 1000
        local fps = frameCount / elapsed
        print(string.format("  FPS: %.1f (over %.1fs)", fps, elapsed))
        batch:removeSelf()
        callback(fps)
    end)
end

-- Scene 2: Regular display objects (baseline comparison)
local function testRegularObjects(count, callback)
    titleText.text = "Regular objects: " .. count .. " rects"
    print(string.format("\n--- Regular display objects: %d rects ---", count))

    local rects = {}
    for i = 1, count do
        local r = display.newRect(
            math.random(10, W - 10),
            math.random(40, H - 40),
            20, 20)
        r:setFillColor(0.2 + math.random() * 0.6, 0.3, 0.8)
        r.rotation = math.random(0, 360)
        r.alpha = 0.5 + math.random() * 0.5
        table.insert(rects, r)
    end

    frameCount = 0
    startTime = system.getTimer()

    timer.performWithDelay(3000, function()
        local elapsed = (system.getTimer() - startTime) / 1000
        local fps = frameCount / elapsed
        print(string.format("  FPS: %.1f (over %.1fs)", fps, elapsed))
        for _, r in ipairs(rects) do r:removeSelf() end
        callback(fps)
    end)
end

-- Scene 3: Interleaved textures (forces many draw calls, tests cmd-buffer batching)
local function testInterleaved(count, callback)
    titleText.text = "Interleaved: " .. count .. " mixed objects"
    print(string.format("\n--- Interleaved objects: %d ---", count))

    local objects = {}
    -- Create colored rect images to use as fill
    local images = {}
    for i = 1, 4 do
        local fname = "inst_color_" .. i .. ".png"
        local snap = display.newSnapshot(20, 20)
        local bg = display.newRect(snap.group, 0, 0, 20, 20)
        bg:setFillColor(i * 0.25, 0.5, 1 - i * 0.2)
        snap:invalidate()
        display.save(snap, { filename = fname, baseDir = system.DocumentsDirectory, captureOffscreenArea = true })
        snap:removeSelf()
        table.insert(images, fname)
    end

    -- Interleave: groups of 10 same-texture objects, then switch texture
    -- This creates many draw calls with same-texture runs of 10
    for i = 1, count do
        local texIdx = math.floor((i - 1) / 10) % #images + 1
        local r = display.newImageRect(images[texIdx], system.DocumentsDirectory, 20, 20)
        r.x = math.random(10, W - 10)
        r.y = math.random(40, H - 40)
        r.rotation = math.random(0, 360)
        table.insert(objects, r)
    end

    frameCount = 0
    startTime = system.getTimer()

    timer.performWithDelay(3000, function()
        local elapsed = (system.getTimer() - startTime) / 1000
        local fps = frameCount / elapsed
        print(string.format("  FPS: %.1f (over %.1fs)", fps, elapsed))
        for _, o in ipairs(objects) do o:removeSelf() end
        callback(fps)
    end)
end

-- Run all tests
local LEVELS = { 1000, 2000, 5000 }

local function runTests(levelIdx)
    if levelIdx > #LEVELS then
        print("\n=== INSTANCING RESULTS (" .. backend .. ", instance=" .. instancing .. ") ===")
        for _, n in ipairs(LEVELS) do
            local r = results[n]
            if r then
                print(string.format("  %d objects:", n))
                print(string.format("    BatchObject: %.1f FPS", r.batch or 0))
                print(string.format("    Regular:     %.1f FPS", r.regular or 0))
                print(string.format("    Interleaved: %.1f FPS", r.interleaved or 0))
            end
        end
        print("=== END ===")
        titleText.text = "All tests complete"
        return
    end

    local count = LEVELS[levelIdx]
    results[count] = {}

    testBatchInstancing(count, function(fps)
        results[count].batch = fps
        testRegularObjects(count, function(fps2)
            results[count].regular = fps2
            testInterleaved(count, function(fps3)
                results[count].interleaved = fps3
                runTests(levelIdx + 1)
            end)
        end)
    end)
end

timer.performWithDelay(500, function()
    runTests(1)
end)
