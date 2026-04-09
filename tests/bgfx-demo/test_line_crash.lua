-- test_line_crash.lua
-- Reproduce: LineObject::UpdateTransform SIGSEGV (fPath NULL)
-- Google Play: 1 user in Brick Car 6+

local W = display.contentWidth
local H = display.contentHeight
local label = display.newText("Line crash test", W/2, 30, native.systemFont, 16)
local cycles = 0

local function stressLines()
    local lines = {}
    for i = 1, 200 do
        local line = display.newLine(
            math.random(0, W), math.random(0, H),
            math.random(0, W), math.random(0, H))
        line:setStrokeColor(math.random(), math.random(), math.random())
        line.strokeWidth = math.random(1, 5)
        for j = 1, 5 do
            line:append(math.random(0, W), math.random(0, H))
        end
        lines[#lines+1] = line
    end

    -- Move to trigger UpdateTransform
    for i, line in ipairs(lines) do
        line.x = math.random(-100, W+100)
        line.y = math.random(-100, H+100)
    end

    -- Destroy
    for i, line in ipairs(lines) do
        line:removeSelf()
    end

    collectgarbage("collect")
    cycles = cycles + 1
    label.text = "Line crash - cycles: " .. cycles
end

timer.performWithDelay(1, stressLines, 0)

timer.performWithDelay(10000, function()
    label.text = "Survived " .. cycles .. " cycles"
    print("test_line_crash: survived " .. cycles .. " cycles")
end)
