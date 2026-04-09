--[[
    test_api_isolate.lua - Per-API isolation test

    Tests each rendering API individually with real images.
    Each test renders one thing, waits, then moves to next.
    Compare bgfx vs GL screenshots at each step.

    Usage: SOLAR2D_TEST=api_isolate SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...
--]]

display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320

print("=== API Isolation Test (" .. backend .. ") ===")

local tests = {}
local testIndex = 0
local currentGroup = nil

local function clearScreen()
    if currentGroup then
        currentGroup:removeSelf()
        currentGroup = nil
    end
    currentGroup = display.newGroup()
    -- dark background
    local bg = display.newRect(currentGroup, W/2, H/2, W, H)
    bg:setFillColor(0.15, 0.15, 0.2)
end

local function runNext()
    testIndex = testIndex + 1
    if testIndex > #tests then
        print("\n=== ALL API ISOLATION TESTS COMPLETE ===")
        return
    end
    clearScreen()
    local t = tests[testIndex]
    print(string.format("\n--- API Test %d: %s ---", testIndex, t.name))
    t.fn()
    -- Auto advance after delay
    timer.performWithDelay(3000, runNext)
end

-- ============================================
-- Test 1: display.newRect + setFillColor
-- ============================================
table.insert(tests, {
    name = "newRect + setFillColor",
    fn = function()
        local r1 = display.newRect(currentGroup, W*0.25, H*0.3, 100*S, 80*S)
        r1:setFillColor(1, 0, 0) -- pure red

        local r2 = display.newRect(currentGroup, W*0.5, H*0.3, 100*S, 80*S)
        r2:setFillColor(0, 1, 0) -- pure green

        local r3 = display.newRect(currentGroup, W*0.75, H*0.3, 100*S, 80*S)
        r3:setFillColor(0, 0, 1) -- pure blue

        -- With alpha
        local r4 = display.newRect(currentGroup, W*0.5, H*0.6, 200*S, 80*S)
        r4:setFillColor(1, 1, 0, 0.5) -- semi-transparent yellow

        local label = display.newText(currentGroup, "Test 1: newRect RGB + Alpha", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] newRect: red/green/blue/yellow-alpha rendered")
    end
})

-- ============================================
-- Test 2: display.newImage (PNG file)
-- ============================================
table.insert(tests, {
    name = "newImage (PNG)",
    fn = function()
        local img1 = display.newImage(currentGroup, "test_gradient.png", W*0.25, H*0.3)
        local img2 = display.newImage(currentGroup, "test_checker.png", W*0.5, H*0.3)
        local img3 = display.newImage(currentGroup, "test_circle_alpha.png", W*0.75, H*0.3)
        local img4 = display.newImage(currentGroup, "test_icon.png", W*0.5, H*0.6)
        if img4 then img4.xScale = 4*S; img4.yScale = 4*S end

        local label = display.newText(currentGroup, "Test 2: newImage (4 PNGs)", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] newImage: gradient/checker/circle_alpha/icon loaded")
        print("[API]   img1=" .. tostring(img1) .. " img2=" .. tostring(img2))
        print("[API]   img3=" .. tostring(img3) .. " img4=" .. tostring(img4))
    end
})

-- ============================================
-- Test 3: display.newImageRect (scaled PNG)
-- ============================================
table.insert(tests, {
    name = "newImageRect (scaled PNG)",
    fn = function()
        local img1 = display.newImageRect(currentGroup, "test_gradient.png", 150*S, 150*S)
        if img1 then img1.x = W*0.3; img1.y = H*0.3 end

        local img2 = display.newImageRect(currentGroup, "test_checker.png", 200*S, 100*S)
        if img2 then img2.x = W*0.7; img2.y = H*0.3 end

        -- Rotated
        local img3 = display.newImageRect(currentGroup, "test_gradient.png", 100*S, 100*S)
        if img3 then img3.x = W*0.5; img3.y = H*0.6; img3.rotation = 45 end

        local label = display.newText(currentGroup, "Test 3: newImageRect (scaled+rotated)", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] newImageRect: large gradient/stretched checker/rotated")
    end
})

-- ============================================
-- Test 4: display.newText
-- ============================================
table.insert(tests, {
    name = "newText",
    fn = function()
        local t1 = display.newText(currentGroup, "Hello Solar2D", W/2, H*0.2, native.systemFontBold, 24*S)
        t1:setFillColor(1, 1, 1)

        local t2 = display.newText(currentGroup, "Red Text", W/2, H*0.35, native.systemFont, 18*S)
        t2:setFillColor(1, 0, 0)

        local t3 = display.newText(currentGroup, "Semi-transparent", W/2, H*0.5, native.systemFont, 16*S)
        t3:setFillColor(0, 1, 1)
        t3.alpha = 0.5

        local t4 = display.newText({
            parent = currentGroup,
            text = "Multi-line\ntext test\nwith wrapping",
            x = W/2, y = H*0.7,
            width = 200*S,
            font = native.systemFont,
            fontSize = 14*S,
            align = "center"
        })
        t4:setFillColor(1, 1, 0)

        local label = display.newText(currentGroup, "Test 4: newText (colors+alpha+multiline)", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] newText: white/red/cyan-alpha/yellow-multiline")
    end
})

-- ============================================
-- Test 5: Overlapping images (painter's algorithm / z-order)
-- ============================================
table.insert(tests, {
    name = "Z-order / painter's algorithm",
    fn = function()
        -- Later objects should be on top
        local r1 = display.newRect(currentGroup, W*0.4, H*0.4, 150*S, 150*S)
        r1:setFillColor(1, 0, 0) -- red behind

        local img = display.newImageRect(currentGroup, "test_gradient.png", 120*S, 120*S)
        if img then img.x = W*0.5; img.y = H*0.45 end -- image in middle

        local r2 = display.newRect(currentGroup, W*0.6, H*0.5, 80*S, 80*S)
        r2:setFillColor(0, 0, 1, 0.7) -- blue on top (semi-transparent)

        local t = display.newText(currentGroup, "TEXT ON TOP", W*0.5, H*0.45, native.systemFontBold, 16*S)
        t:setFillColor(1, 1, 0) -- yellow text on very top

        local label = display.newText(currentGroup, "Test 5: Z-order (red→image→blue→text)", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] Z-order: red rect → gradient image → blue rect → yellow text")
    end
})

-- ============================================
-- Test 6: Groups with transforms
-- ============================================
table.insert(tests, {
    name = "Groups + transforms",
    fn = function()
        local g1 = display.newGroup()
        currentGroup:insert(g1)
        g1.x = W*0.3; g1.y = H*0.35

        local r1 = display.newRect(g1, 0, 0, 60*S, 60*S)
        r1:setFillColor(1, 0.5, 0)
        local img1 = display.newImage(g1, "test_icon.png", 0, 0)
        if img1 then img1.xScale = 2*S; img1.yScale = 2*S end

        -- Rotated group
        local g2 = display.newGroup()
        currentGroup:insert(g2)
        g2.x = W*0.7; g2.y = H*0.35
        g2.rotation = 30

        local r2 = display.newRect(g2, 0, 0, 60*S, 60*S)
        r2:setFillColor(0, 0.5, 1)
        local img2 = display.newImage(g2, "test_checker.png", 0, 0)

        -- Scaled group
        local g3 = display.newGroup()
        currentGroup:insert(g3)
        g3.x = W*0.5; g3.y = H*0.65
        g3.xScale = 1.5; g3.yScale = 1.5

        local r3 = display.newRect(g3, 0, 0, 50*S, 50*S)
        r3:setFillColor(0.5, 1, 0)
        local img3 = display.newImage(g3, "test_gradient.png", 20*S, 0)

        local label = display.newText(currentGroup, "Test 6: Groups (normal/rotated/scaled)", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] Groups: normal/rotated30/scaled1.5 with images inside")
    end
})

-- ============================================
-- Test 7: Snapshot (FBO)
-- ============================================
table.insert(tests, {
    name = "Snapshot (FBO)",
    fn = function()
        local snap = display.newSnapshot(currentGroup, 150*S, 150*S)
        snap.x = W*0.35; snap.y = H*0.4

        local r = display.newRect(snap.group, 0, 0, 100*S, 100*S)
        r:setFillColor(1, 0.3, 0.3)
        local c = display.newCircle(snap.group, 30*S, 30*S, 40*S)
        c:setFillColor(0.3, 0.3, 1)
        snap:invalidate()

        -- Snapshot with image
        local snap2 = display.newSnapshot(currentGroup, 120*S, 120*S)
        snap2.x = W*0.7; snap2.y = H*0.4

        local img = display.newImage(snap2.group, "test_gradient.png", 0, 0)
        snap2:invalidate()

        local label = display.newText(currentGroup, "Test 7: Snapshot/FBO (shapes + image)", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] Snapshot: shapes FBO + image FBO")
    end
})

-- ============================================
-- Test 8: ImageSheet + Sprite
-- ============================================
table.insert(tests, {
    name = "ImageSheet + Sprite",
    fn = function()
        -- Create a simple 4-frame sprite sheet from checker image
        -- Each "frame" is 16x32 from the 32x32 checker
        local options = {
            width = 16,
            height = 32,
            numFrames = 2,
            sheetContentWidth = 32,
            sheetContentHeight = 32
        }
        local sheet = graphics.newImageSheet("test_checker.png", options)
        if sheet then
            local seq = { name="test", start=1, count=2, time=500, loopCount=0 }
            local sprite = display.newSprite(currentGroup, sheet, seq)
            sprite.x = W*0.5; sprite.y = H*0.4
            sprite.xScale = 4*S; sprite.yScale = 4*S
            sprite:play()
            print("[API] ImageSheet + Sprite: created and playing")
        else
            print("[API] ImageSheet: FAILED to create sheet")
        end

        local label = display.newText(currentGroup, "Test 8: ImageSheet + Sprite", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)
    end
})

-- ============================================
-- Test 9: Multiple same images (texture sharing)
-- ============================================
table.insert(tests, {
    name = "Multiple same images (texture sharing)",
    fn = function()
        for i = 1, 20 do
            local x = ((i-1) % 5) * 60*S + 40*S
            local y = math.floor((i-1) / 5) * 60*S + 100*S
            local img = display.newImage(currentGroup, "test_icon.png", x, y)
            if img then
                img.xScale = 2*S; img.yScale = 2*S
                img.rotation = i * 18
            end
        end

        local label = display.newText(currentGroup, "Test 9: 20x same image (texture sharing)", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] 20 instances of test_icon.png with rotation")
    end
})

-- ============================================
-- Test 10: Mixed content stress
-- ============================================
table.insert(tests, {
    name = "Mixed content (rect+image+text stacked)",
    fn = function()
        for i = 1, 10 do
            local y = 60*S + i * 35*S
            -- rect background
            local r = display.newRect(currentGroup, W*0.3, y, 80*S, 25*S)
            r:setFillColor(0.2*i/10, 0.5, 1-0.1*i)
            -- image
            local img = display.newImage(currentGroup, "test_icon.png", W*0.55, y)
            if img then img.xScale = S; img.yScale = S end
            -- text
            local t = display.newText(currentGroup, "Item " .. i, W*0.75, y, native.systemFont, 10*S)
            t:setFillColor(1,1,1)
        end

        local label = display.newText(currentGroup, "Test 10: Mixed rect+image+text x10", W/2, 30*S, native.systemFontBold, 14*S)
        label:setFillColor(1,1,1)

        print("[API] Mixed: 10 rows of rect+image+text interleaved")
    end
})

-- Start
timer.performWithDelay(500, runNext)
