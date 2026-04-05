--[[
    test_capture.lua - Test display.capture / display.captureBounds

    Usage: SOLAR2D_TEST=capture SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Tests:
    1. display.capture(object) - capture a display object
    2. display.captureBounds(bounds) - capture a screen region
    3. display.captureScreen() - capture full screen
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Capture Test ===")
print("Backend: " .. backend)

-- Background
local bg = display.newRect(display.contentCenterX, display.contentCenterY,
    display.contentWidth, display.contentHeight)
bg:setFillColor(0.1, 0.1, 0.2)

local statusText = display.newText({
    text = "Capture Test: " .. backend,
    x = display.contentCenterX, y = 30,
    font = native.systemFontBold, fontSize = 14
})
statusText:setFillColor(1, 1, 1)

-- Create test objects to capture
local testGroup = display.newGroup()
local rect1 = display.newRect(testGroup, 100, 150, 80, 80)
rect1:setFillColor(1, 0, 0)

local rect2 = display.newRect(testGroup, 200, 150, 80, 80)
rect2:setFillColor(0, 1, 0)

local circle = display.newCircle(testGroup, 150, 250, 40)
circle:setFillColor(0, 0, 1)

local results = {}

local function addResult(name, success, detail)
    table.insert(results, { name = name, success = success, detail = detail or "" })
    local mark = success and "PASS" or "FAIL"
    print(string.format("[%s] %s %s", mark, name, detail or ""))
end

-- Run tests after a short delay to ensure rendering
timer.performWithDelay(500, function()
    -- Test 1: display.capture(object)
    local captured = display.capture(testGroup)
    if captured then
        addResult("capture(group)", true,
            string.format("w=%d h=%d", captured.width, captured.height))
        captured.x = 100
        captured.y = 400
        captured.xScale = 0.5
        captured.yScale = 0.5
    else
        addResult("capture(group)", false, "returned nil")
    end

    -- Test 2: display.captureBounds
    local bounds = { xMin = 50, yMin = 100, xMax = 250, yMax = 300 }
    local boundsCapture = display.captureBounds(bounds)
    if boundsCapture then
        addResult("captureBounds", true,
            string.format("w=%d h=%d", boundsCapture.width, boundsCapture.height))
        boundsCapture.x = 250
        boundsCapture.y = 400
        boundsCapture.xScale = 0.5
        boundsCapture.yScale = 0.5
    else
        addResult("captureBounds", false, "returned nil")
    end

    -- Test 3: display.captureScreen
    local screenCap = display.captureScreen(false)
    if screenCap then
        addResult("captureScreen", true,
            string.format("w=%d h=%d", screenCap.width, screenCap.height))
        screenCap.x = display.contentCenterX
        screenCap.y = display.contentHeight - 60
        screenCap.xScale = 0.3
        screenCap.yScale = 0.3
    else
        addResult("captureScreen", false, "returned nil")
    end

    -- Summary
    local passed = 0
    local total = #results
    for _, r in ipairs(results) do
        if r.success then passed = passed + 1 end
    end

    local summary = string.format("\n=== CAPTURE TEST RESULTS (%s) ===\n", backend)
    for _, r in ipairs(results) do
        local mark = r.success and "PASS" or "FAIL"
        summary = summary .. string.format("[%s] %s %s\n", mark, r.name, r.detail)
    end
    summary = summary .. string.format("\n%d/%d passed\n=== END ===\n", passed, total)
    print(summary)

    statusText.text = string.format("Capture: %d/%d passed (%s)", passed, total, backend)
end)
