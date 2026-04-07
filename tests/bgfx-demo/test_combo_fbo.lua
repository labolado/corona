print("=== Combo FBO test (capture + snapshot) ===")

local g = display.newGroup()

-- Create scene content
for i = 1, 8 do
    local r = display.newRect(g, math.random(20, 300), math.random(60, 400), 50, 50)
    r:setFillColor(math.random(), math.random(), math.random())
    r.rotation = math.random(360)
end

timer.performWithDelay(1000, function()
    -- Step 1: display.capture
    print("Step 1: display.capture...")
    local ok1, cap = pcall(function()
        return display.capture(g, {saveToPhotoLibrary = false, isFullResolution = false})
    end)
    print("  capture: ok=" .. tostring(ok1))

    if ok1 and cap then
        cap.x = 160; cap.y = 300
        cap.xScale = 0.3; cap.yScale = 0.3
        g:insert(cap)
    end

    -- Step 2: display.newSnapshot
    print("Step 2: display.newSnapshot...")
    local ok2, snap = pcall(function()
        local s = display.newSnapshot(g, 150, 100)
        s.x = 160; s.y = 150
        for j = 1, 5 do
            local c = display.newCircle(s.group, (j-3)*25, 0, 15)
            c:setFillColor(j/5, 1-j/5, 0.5)
        end
        s:invalidate()
        return s
    end)
    print("  snapshot: ok=" .. tostring(ok2))
    print("=== Combo test DONE ===")
end)
