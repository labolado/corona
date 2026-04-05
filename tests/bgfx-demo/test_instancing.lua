--[[
    test_instancing.lua - GPU Instancing performance test for BatchObject

    Compares:
    1. Batch with GPU instancing (5000 objects via display.newBatch)
    2. Individual display objects (5000 x display.newRect)
    3. Reports FPS and draw calls
--]]

display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== INSTANCING PERF TEST (" .. backend .. ") ===")

local W = display.contentWidth
local H = display.contentHeight

-- Step 1: Create a simple atlas with colored tiles
local imageFiles = {}
local lfs = require("lfs")

for i = 1, 4 do
    local filename = "inst_tile_" .. i .. ".png"
    table.insert(imageFiles, filename)

    local snapshot = display.newSnapshot(32, 32)
    local bg = display.newRect(snapshot.group, 0, 0, 32, 32)
    bg:setFillColor(i * 0.25, 0.3 + i * 0.15, 1 - i * 0.2)
    snapshot:invalidate()

    display.save(snapshot, {
        filename = filename,
        baseDir = system.DocumentsDirectory,
        captureOffscreenArea = true
    })
    snapshot:removeSelf()
end

-- FPS tracker
local fps = 0
local frameCount = 0
local startTime = system.getTimer()
local fpsText = display.newText("FPS: --", W / 2, 20, native.systemFont, 14)
fpsText:setFillColor(1, 1, 0)

local function updateFPS()
    frameCount = frameCount + 1
    local elapsed = (system.getTimer() - startTime) / 1000
    if elapsed >= 1 then
        fps = frameCount / elapsed
        fpsText.text = string.format("FPS: %.1f", fps)
        frameCount = 0
        startTime = system.getTimer()
    end
end
Runtime:addEventListener("enterFrame", updateFPS)

local NUM_OBJECTS = 5000
local results = {}

-- Delay chain
timer.performWithDelay(500, function()
    print("\n--- Phase 1: Batch + GPU Instancing (" .. NUM_OBJECTS .. " objects) ---")

    local atlas = graphics.newAtlas(imageFiles, { baseDir = system.DocumentsDirectory })
    if not atlas then
        print("[FAIL] Could not create atlas")
        return
    end

    local batch = display.newBatch(atlas, NUM_OBJECTS)

    local t0 = system.getTimer()
    for i = 1, NUM_OBJECTS do
        local filename = imageFiles[((i - 1) % #imageFiles) + 1]
        local x = math.random(10, W - 10)
        local y = math.random(40, H - 10)
        batch:add(filename, x, y, { rotation = math.random(0, 360), alpha = 0.5 + math.random() * 0.5 })
    end
    local addTime = system.getTimer() - t0
    print(string.format("  Add time: %.1f ms", addTime))
    print(string.format("  Batch slot count: %d", batch:count()))

    -- Measure FPS for 5 seconds
    frameCount = 0
    startTime = system.getTimer()

    timer.performWithDelay(3000, function()
        local elapsed = (system.getTimer() - startTime) / 1000
        local batchFps = frameCount / elapsed
        results.batchFPS = batchFps
        print(string.format("  Batch FPS: %.1f (over %.1fs)", batchFps, elapsed))

        -- Cleanup
        batch:removeSelf()
        atlas = nil

        -- Phase 2: Individual objects
        print("\n--- Phase 2: Individual display.newRect (" .. NUM_OBJECTS .. " objects) ---")
        local rects = {}
        t0 = system.getTimer()
        for i = 1, NUM_OBJECTS do
            local r = display.newRect(
                math.random(10, W - 10),
                math.random(40, H - 10),
                32, 32)
            r:setFillColor(math.random() * 0.8 + 0.2, 0.5, math.random() * 0.8 + 0.2)
            r.rotation = math.random(0, 360)
            r.alpha = 0.5 + math.random() * 0.5
            table.insert(rects, r)
        end
        addTime = system.getTimer() - t0
        print(string.format("  Create time: %.1f ms", addTime))

        frameCount = 0
        startTime = system.getTimer()

        timer.performWithDelay(3000, function()
            elapsed = (system.getTimer() - startTime) / 1000
            local rectFps = frameCount / elapsed
            results.rectFPS = rectFps
            print(string.format("  Individual FPS: %.1f (over %.1fs)", rectFps, elapsed))

            -- Cleanup
            for _, r in ipairs(rects) do
                r:removeSelf()
            end
            rects = nil

            -- Print summary
            print("\n=== INSTANCING PERF RESULTS (" .. backend .. ") ===")
            print(string.format("  Batch (instanced): %.1f FPS", results.batchFPS))
            print(string.format("  Individual rects:  %.1f FPS", results.rectFPS))
            if results.batchFPS > 0 and results.rectFPS > 0 then
                print(string.format("  Speedup: %.1fx", results.batchFPS / results.rectFPS))
            end
            print("=== END ===")
        end)
    end)
end)
