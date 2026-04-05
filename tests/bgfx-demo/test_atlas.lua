--[[
    test_atlas.lua - Atlas functionality tests
--]]

display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Atlas Test (" .. backend .. ") ===")

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

for i = 1, 4 do
    local filename = "atlas_test_" .. i .. ".png"
    table.insert(imageFiles, filename)
    
    local snapshot = display.newSnapshot(64, 64)
    local bg = display.newRect(snapshot.group, 0, 0, 64, 64)
    bg:setFillColor(i * 0.2, 0.5, 1 - i * 0.2)
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

-- Create large image
local largeSnapshot = display.newSnapshot(128, 128)
local largeBg = display.newRect(largeSnapshot.group, 0, 0, 128, 128)
largeBg:setFillColor(0.5, 0.5, 0.5)
local largeCircle = display.newCircle(largeSnapshot.group, 0, 0, 40)
largeCircle:setFillColor(1, 0, 0, 0.5)
largeSnapshot:invalidate()
display.save(largeSnapshot, {
    filename = "atlas_test_large.png",
    baseDir = system.DocumentsDirectory,
    captureOffscreenArea = true
})
largeSnapshot:removeSelf()
table.insert(imageFiles, "atlas_test_large.png")
print("Created atlas_test_large.png")

-- Short delay
timer.performWithDelay(200, function()
    print("\n--- Test 1: Atlas Creation ---")
    local atlas = graphics.newAtlas(imageFiles, { baseDir = system.DocumentsDirectory })
    check("Atlas created", atlas ~= nil)
    check("Atlas is userdata", type(atlas) == "userdata")
    
    if not atlas then
        print(string.format("\n=== RESULTS: Pass %d | Fail %d ===", pass, fail))
        os.exit(1)
        return
    end
    
    print("\n--- Test 2: atlas:list() ---")
    local listResult = atlas:list()
    print("atlas:list() returned " .. type(listResult) .. " with " .. (listResult and #listResult or 0) .. " items")
    check("list() returns table", type(listResult) == "table")
    check("list() has correct count", #listResult == #imageFiles)
    
    print("\n--- Test 3: atlas:has() ---")
    local hasResult = atlas:has("atlas_test_1.png")
    check("has() returns true for existing file", hasResult == true)
    local hasMissing = atlas:has("nonexistent.png")
    check("has() returns false for missing file", hasMissing == false)
    
    print("\n--- Test 4: atlas:getFrame() ---")
    local frame = atlas:getFrame("atlas_test_1.png")
    check("getFrame() returns value", frame ~= nil)
    
    print("\n--- Test 5: Atlas Properties ---")
    local fc = atlas.frameCount
    check("frameCount is number", type(fc) == "number")
    local w = atlas.width
    check("width is number", type(w) == "number")
    local h = atlas.height
    check("height is number", type(h) == "number")
    
    print("\n--- Test 6: display.newImage with Atlas ---")
    local imgResult = display.newImage(atlas, "atlas_test_1.png", 100, 100)
    print("display.newImage returned " .. tostring(imgResult))
    check("newImage creates object", imgResult ~= nil)
    if imgResult then
        imgResult:removeSelf()
    end
    
    print("\n--- Test 7: atlas:removeSelf() ---")
    local removeOk = true
    local removeErr = nil
    -- Use pcall only to catch crash, but we assert it succeeds
    local ok, err = pcall(function()
        atlas:removeSelf()
    end)
    if not ok then
        removeOk = false
        removeErr = err
        print("removeSelf() crashed: " .. tostring(err))
    end
    check("removeSelf() does not crash", removeOk)
    
    -- Verify atlas is no longer usable after removeSelf
    local unusableOk, unusableErr = pcall(function()
        return atlas:has("atlas_test_1.png")
    end)
    check("atlas unusable after removeSelf", not unusableOk)
    
    print("\n--- Test 8: Multiple Atlas Creation ---")
    local multiSuccess = true
    for i = 1, 5 do
        local testAtlas = graphics.newAtlas(imageFiles, { baseDir = system.DocumentsDirectory })
        if not testAtlas then
            multiSuccess = false
            break
        end
        testAtlas:removeSelf()
    end
    check("Multiple atlas creation", multiSuccess)
    
    -- Final summary
    print(string.format("\n=== ATLAS TEST RESULTS (%s) ===", backend))
    print(string.format("Pass: %d | Fail: %d", pass, fail))
    print("=== END ===")
    
    timer.performWithDelay(500, function()
        os.exit(fail > 0 and 1 or 0)
    end)
end)
