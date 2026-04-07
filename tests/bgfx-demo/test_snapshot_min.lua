print("=== Minimal snapshot test ===")

local g = display.newGroup()
local r = display.newRect(g, display.contentCenterX, display.contentCenterY, 100, 100)
r:setFillColor(1, 0, 0)

timer.performWithDelay(1000, function()
    print("Attempting display.newSnapshot...")
    local ok, result = pcall(function()
        local s = display.newSnapshot(g, 200, 200)
        s.x = display.contentCenterX
        s.y = 200
        local c = display.newCircle(s.group, 0, 0, 50)
        c:setFillColor(0, 1, 0)
        s:invalidate()
        return s
    end)
    print("snapshot result: ok=" .. tostring(ok) .. " result=" .. tostring(result))
end)
