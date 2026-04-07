-- test_capture_flip.lua
-- Test: display.capture() Y-flip bug in bgfx/Metal
-- Expected: captured image should match on-screen rendering (not flipped)

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0.9, 0.9, 0.9)

-- Create asymmetric content: RED at top, BLUE at bottom
-- If capture is Y-flipped, red and blue will swap
local topRect = display.newRect(display.contentCenterX, H * 0.25, W * 0.6, H * 0.3)
topRect:setFillColor(1, 0, 0) -- RED at top

local bottomRect = display.newRect(display.contentCenterX, H * 0.75, W * 0.6, H * 0.3)
bottomRect:setFillColor(0, 0, 1) -- BLUE at bottom

-- Add text labels
local topLabel = display.newText("TOP (RED)", display.contentCenterX, H * 0.25, native.systemFontBold, 20)
topLabel:setFillColor(1, 1, 1)

local bottomLabel = display.newText("BOTTOM (BLUE)", display.contentCenterX, H * 0.75, native.systemFontBold, 20)
bottomLabel:setFillColor(1, 1, 1)

-- Arrow pointing DOWN (asymmetric shape to detect flip)
local arrow = display.newPolygon(display.contentCenterX, display.contentCenterY, {
    0, -30,   -- top point
    20, 0,    -- right
    8, 0,     -- right inner
    8, 30,    -- bottom right
    -8, 30,   -- bottom left
    -8, 0,    -- left inner
    -20, 0,   -- left
})
arrow:setFillColor(0, 0.8, 0) -- GREEN arrow pointing down

local label = display.newText("Arrow points DOWN", display.contentCenterX, display.contentCenterY + 50, native.systemFont, 14)
label:setFillColor(0, 0, 0)

-- Wait for rendering, then capture
timer.performWithDelay(1000, function()
    -- Method 1: display.capture (whole screen)
    local captured = display.captureScreen(false)
    if captured then
        -- Save to file for inspection
        display.save(captured, {
            filename = "capture_test.png",
            baseDir = system.TemporaryDirectory,
        })
        local path = system.pathForFile("capture_test.png", system.TemporaryDirectory)
        print("CAPTURE saved to: " .. tostring(path))

        -- Display the captured image on the right side for visual comparison
        captured.x = W * 0.75
        captured.y = display.contentCenterY
        captured.xScale = 0.4
        captured.yScale = 0.4
        captured:setStrokeColor(0, 0, 0)
        captured.strokeWidth = 2

        local captureLabel = display.newText("Captured →", W * 0.75, 20, native.systemFont, 12)
        captureLabel:setFillColor(0, 0, 0)

        print("CAPTURE TEST: Check if RED is at top and BLUE is at bottom in the captured image")
        print("If flipped: RED would be at bottom and BLUE at top")
    else
        print("ERROR: display.captureScreen returned nil")
    end
end)

-- Method 2: display.capture (object)
timer.performWithDelay(2000, function()
    local group = display.newGroup()
    local r1 = display.newRect(group, 50, 30, 80, 40)
    r1:setFillColor(1, 0, 0) -- RED at top
    local r2 = display.newRect(group, 50, 80, 80, 40)
    r2:setFillColor(0, 0, 1) -- BLUE at bottom
    local arr = display.newPolygon(group, 50, 55, {0,-10, 8,5, -8,5})
    arr:setFillColor(0, 0.8, 0) -- GREEN arrow pointing down

    local objCapture = display.capture(group)
    if objCapture then
        objCapture.x = W * 0.25
        objCapture.y = H - 60
        objCapture:setStrokeColor(0, 0, 0)
        objCapture.strokeWidth = 2
        local objLabel = display.newText("Object capture →", W * 0.25, H - 100, native.systemFont, 12)
        objLabel:setFillColor(0, 0, 0)
        print("OBJECT CAPTURE: Check if RED is at top, GREEN arrow points down")
    else
        print("ERROR: display.capture returned nil")
    end
    group:removeSelf()
end)
