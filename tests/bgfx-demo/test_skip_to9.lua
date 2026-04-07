print("=== Skip to test 9-11 ===")

-- Minimal S factor
local W = display.contentWidth
local S = W / 320
local CX = display.contentCenterX
local CY = display.contentCenterY

-- Test 9: masks
print("--- Test 9: masks ---")
local g9 = display.newGroup()
local ok9, err9 = pcall(function()
    -- Container clipping
    local c = display.newContainer(200*S, 150*S)
    c.x = CX; c.y = 200*S
    g9:insert(c)
    local r = display.newRect(0, 0, 300*S, 200*S)
    r:setFillColor(1, 0, 0)
    c:insert(r)

    -- newSnapshot
    local s = display.newSnapshot(g9, 150*S, 100*S)
    s.x = CX; s.y = 380*S
    local cr = display.newCircle(s.group, 0, 0, 40*S)
    cr:setFillColor(0, 0.8, 0.4)
    s:invalidate()
end)
print("[TEST masks] " .. (ok9 and "PASS" or "FAIL: " .. tostring(err9)))

-- Cleanup test 9
timer.performWithDelay(3000, function()
    print("Cleaning up test 9...")
    g9:removeSelf()
    g9 = nil
    
    timer.performWithDelay(500, function()
        -- Test 10: snapshot_fbo
        print("--- Test 10: snapshot_fbo ---")
        local g10 = display.newGroup()
        local ok10, err10 = pcall(function()
            -- Scene content
            for i = 1, 8 do
                local r = display.newRect(g10, math.random(20, 300)*S, math.random(60, 400)*S,
                    (20 + math.random(40))*S, (20 + math.random(40))*S)
                r:setFillColor(math.random(), math.random(), math.random())
            end
            
            -- display.capture
            local cap = display.capture(g10, {saveToPhotoLibrary = false, isFullResolution = false})
            if cap then
                cap.x = CX; cap.y = 300*S
                cap.xScale = 0.3; cap.yScale = 0.3
                g10:insert(cap)
            end
            
            -- display.newSnapshot
            local s = display.newSnapshot(g10, 150*S, 100*S)
            s.x = CX; s.y = 150*S
            local c = display.newCircle(s.group, 0, 0, 40*S)
            c:setFillColor(0, 1, 0)
            s:invalidate()
        end)
        print("[TEST snapshot_fbo] " .. (ok10 and "PASS" or "FAIL: " .. tostring(err10)))
        
        -- Cleanup test 10
        timer.performWithDelay(3000, function()
            print("Cleaning up test 10...")
            g10:removeSelf()
            g10 = nil
            print("=== All done ===")
        end)
    end)
end)
