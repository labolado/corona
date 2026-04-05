--[[
    test_batch.lua - Batch functionality tests
--]]

display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Batch Test (" .. backend .. ") ===")

local pass, fail = 0, 0
local function check(name, condition)
    if condition then
        pass = pass + 1
        print("[PASS] " .. name)
    else
        fail = fail + 1
        print("[FAIL] " .. name)
    end
end

-- Create test images
local lfs = require("lfs")
local docsPath = system.pathForFile(nil, system.DocumentsDirectory)
print("Documents path: " .. tostring(docsPath))

local imageFiles = {}

for i = 1, 3 do
    local filename = "batch_test_" .. i .. ".png"
    table.insert(imageFiles, filename)
    
    local snapshot = display.newSnapshot(64, 64)
    local bg = display.newRect(snapshot.group, 0, 0, 64, 64)
    bg:setFillColor(i * 0.3, 0.5, 1 - i * 0.3)
    local circle = display.newCircle(snapshot.group, 0, 0, 20)
    circle:setFillColor(1, 1, 1, 0.5)
    snapshot:invalidate()
    
    display.save(snapshot, {
        filename = filename,
        baseDir = system.DocumentsDirectory,
        captureOffscreenArea = true
    })
    snapshot:removeSelf()
    print("Created " .. filename)
end

-- Short delay
timer.performWithDelay(200, function()
    print("\n--- Test 1: Create Atlas ---")
    local atlas = graphics.newAtlas(imageFiles, { baseDir = system.DocumentsDirectory })
    check("Atlas created", atlas ~= nil)
    
    if not atlas then
        print(string.format("\n=== RESULTS: Pass %d | Fail %d ===", pass, fail))
        os.exit(1)
        return
    end
    
    print("\n--- Test 2: Create Batch ---")
    local batch = display.newBatch(atlas, 100)
    check("Batch created", batch ~= nil)
    
    if not batch then
        print(string.format("\n=== RESULTS: Pass %d | Fail %d ===", pass, fail))
        os.exit(1)
        return
    end
    
    print("\n--- Test 3: Batch Operations ---")
    
    -- Test add()
    print("Testing batch:add()...")
    local slot1 = batch:add(imageFiles[1], 50, 60)
    print("batch:add() returned " .. tostring(slot1))
    check("add() returns slot", slot1 ~= nil)
    
    -- Test numSlots (property, not method)
    print("Testing batch.numSlots...")
    local count = batch.numSlots
    print("batch.numSlots = " .. tostring(count))
    check("numSlots returns number", type(count) == "number")
    check("numSlots is 1 after one add", count == 1)
    
    print("\n--- Test 4: Multiple Add Operations ---")
    local addOk = true
    for i = 1, 10 do
        local slot = batch:add(imageFiles[(i % 3) + 1], i * 10, i * 10)
        if not slot then
            addOk = false
            print("add() returned nil at iteration " .. i)
            break
        end
    end
    check("Multiple add() operations", addOk)
    check("numSlots is 11 after 11 adds", batch.numSlots == 11)
    
    print("\n--- Test 5: batch:removeSelf() ---")
    
    -- Test removeSelf does not crash
    local removeOk = true
    local removeErr = nil
    local ok, err = pcall(function()
        batch:removeSelf()
    end)
    if not ok then
        removeOk = false
        removeErr = err
        print("batch:removeSelf() crashed: " .. tostring(err))
    end
    check("removeSelf() does not crash", removeOk)
    
    -- Test batch is unusable after removeSelf
    local unusableOk, unusableErr = pcall(function()
        return batch.numSlots
    end)
    check("batch unusable after removeSelf", not unusableOk)
    
    -- Test atlas:removeSelf()
    print("\n--- Test 6: atlas:removeSelf() ---")
    local atlasRemoveOk = true
    local atlasRemoveErr = nil
    local aok, aerr = pcall(function()
        atlas:removeSelf()
    end)
    if not aok then
        atlasRemoveOk = false
        atlasRemoveErr = aerr
        print("atlas:removeSelf() crashed: " .. tostring(aerr))
    end
    check("atlas removeSelf() does not crash", atlasRemoveOk)
    
    -- Test atlas is unusable after removeSelf
    local atlasUnusableOk, atlasUnusableErr = pcall(function()
        return atlas:has(imageFiles[1])
    end)
    check("atlas unusable after removeSelf", not atlasUnusableOk)
    
    -- Final summary
    print(string.format("\n=== BATCH TEST RESULTS (%s) ===", backend))
    print(string.format("Pass: %d | Fail: %d", pass, fail))
    print("=== END ===")
    
    timer.performWithDelay(500, function()
        os.exit(fail > 0 and 1 or 0)
    end)
end)
