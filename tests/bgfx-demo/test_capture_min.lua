print("=== Minimal capture test ===")
local r = display.newRect(display.contentCenterX, display.contentCenterY, 100, 100)
r:setFillColor(1, 0, 0)

timer.performWithDelay(1000, function()
    print("Attempting display.capture...")
    local ok, result = pcall(function()
        return display.capture(display.currentStage)
    end)
    print("capture result: ok=" .. tostring(ok) .. " result=" .. tostring(result))
    if ok and result then
        print("Capture succeeded!")
    else
        print("Capture failed: " .. tostring(result))
    end
end)
