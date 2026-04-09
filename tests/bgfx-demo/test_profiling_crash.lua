-- test_profiling_crash.lua
-- Reproduce: Profiling::EntryRAII SIGSEGV
-- Root cause: *GetProfilingState() dereferences NULL during Display teardown
-- Strategy: rapid create/destroy to stress Display lifecycle timing

local W = display.contentWidth
local H = display.contentHeight
local label = display.newText("Profiling crash test", W/2, 30, native.systemFont, 16)
local cycles = 0
local scenes = {}

local function createHeavyScene()
    local g = display.newGroup()
    for i = 1, 100 do
        local r = display.newRect(g, math.random(0, W), math.random(0, H),
                                   math.random(10, 50), math.random(10, 50))
        r:setFillColor(math.random(), math.random(), math.random(), 0.5)
    end
    return g
end

local function cycleScenes()
    for i, g in ipairs(scenes) do
        if g.removeSelf then g:removeSelf() end
    end
    scenes = {}
    for i = 1, 3 do
        scenes[#scenes+1] = createHeavyScene()
    end
    cycles = cycles + 1
    label.text = "Profiling crash - cycles: " .. cycles
    collectgarbage("collect")
end

timer.performWithDelay(1, cycleScenes, 0)

timer.performWithDelay(10000, function()
    label.text = "Stress test survived " .. cycles .. " cycles"
    print("test_profiling_crash: survived " .. cycles .. " cycles")
end)
