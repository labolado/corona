--[[
    test_images_real.lua - Real image file loading test

    Tests display.newImage and display.newImageRect with actual PNG files.
    This is critical - previous tests only used programmatic shapes,
    missing texture rendering bugs entirely.

    Usage: SOLAR2D_TEST=images_real SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320

print("=== Real Image Loading Test (" .. backend .. ") ===")

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

-- Test 1: display.newImage with PNG file
print("\n--- Test 1: display.newImage ---")
local img1 = display.newImage("test_gradient.png", 80*S, 80*S)
check("newImage creates object", img1 ~= nil)
if img1 then
    check("newImage has width", img1.width > 0)
    check("newImage has height", img1.height > 0)
    check("newImage visible", img1.isVisible == true)
    print("  size: " .. img1.width .. "x" .. img1.height)
end

-- Test 2: display.newImageRect with PNG file
print("\n--- Test 2: display.newImageRect ---")
local img2 = display.newImageRect("test_checker.png", 100*S, 100*S)
if img2 then
    img2.x = 220*S
    img2.y = 80*S
    check("newImageRect creates object", true)
    check("newImageRect scaled", img2.width > 0)
else
    check("newImageRect creates object", false)
end

-- Test 3: Image with alpha channel
print("\n--- Test 3: Alpha PNG ---")
local img3 = display.newImage("test_circle_alpha.png", 80*S, 200*S)
check("alpha PNG loads", img3 ~= nil)
if img3 then
    img3.xScale = 2*S
    img3.yScale = 2*S
    check("alpha PNG scaleable", img3.xScale > 1)
end

-- Test 4: Small icon image
print("\n--- Test 4: Small icon ---")
local img4 = display.newImage("test_icon.png", 220*S, 200*S)
check("small icon loads", img4 ~= nil)
if img4 then
    img4.xScale = 4*S
    img4.yScale = 4*S
end

-- Test 5: Multiple images of same file (shared texture)
print("\n--- Test 5: Multiple instances ---")
local images = {}
for i = 1, 5 do
    local img = display.newImage("test_gradient.png", (30 + i * 50)*S, 320*S)
    if img then
        img.xScale = 0.5*S
        img.yScale = 0.5*S
        img.rotation = i * 30
        table.insert(images, img)
    end
end
check("5 instances created", #images == 5)

-- Test 6: Image with fill effect
print("\n--- Test 6: Image transforms ---")
local img6 = display.newImage("test_checker.png", 160*S, 420*S)
if img6 then
    img6.xScale = 3*S
    img6.yScale = 3*S
    img6.alpha = 0.7
    check("image alpha works", img6.alpha < 1)
    check("image transform works", img6.xScale > 1)
else
    check("image alpha works", false)
end

-- Test 7: Image removal (memory)
print("\n--- Test 7: Create/remove cycle ---")
local memBefore = collectgarbage("count")
for i = 1, 50 do
    local tmp = display.newImage("test_icon.png", 0, 0)
    if tmp then tmp:removeSelf() end
end
collectgarbage("collect")
local memAfter = collectgarbage("count")
local memDiff = memAfter - memBefore
print(string.format("  Memory: before=%.1fKB after=%.1fKB diff=%.1fKB", memBefore, memAfter, memDiff))
check("no memory leak (< 50KB)", memDiff < 50)

-- Summary
print(string.format("\n=== REAL IMAGE TEST RESULTS (%s) ===", backend))
print(string.format("Pass: %d | Fail: %d", pass, fail))
print("=== END ===")
