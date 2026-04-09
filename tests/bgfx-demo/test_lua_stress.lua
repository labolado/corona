-- test_lua_stress.lua
-- Reproduce: lua_pcall / luaM_realloc_ / lua_gc SIGSEGV/SIGABRT
-- Google Play: 14 users across 6 apps

local W = display.contentWidth
local H = display.contentHeight
local label = display.newText("Lua stress test", W/2, 30, native.systemFont, 16)
local cycles = 0

-- Method 1: Memory pressure with rapid alloc/free
local function memoryPressure()
    local t = {}
    for i = 1, 10000 do
        t[i] = string.rep("x", math.random(100, 1000))
    end
    t = nil
    collectgarbage("collect")
end

-- Method 2: Rapid display object create/destroy + GC
local function stressDisplayAndGC()
    for i = 1, 100 do
        pcall(function()
            local objs = {}
            for j = 1, 50 do
                objs[j] = display.newRect(0, 0, 1, 1)
            end
            for j = 1, 50 do
                objs[j]:removeSelf()
            end
        end)
        collectgarbage("collect")
    end
end

-- Method 3: Deep pcall nesting
local function deepPcall(n)
    if n <= 0 then return true end
    local ok, err = pcall(deepPcall, n - 1)
    return ok
end

-- Method 4: Stack pressure
local function stackPressure()
    local ok, err = pcall(function()
        local t = {}
        for i = 1, 500 do
            t[i] = function() return i end
        end
        for i = 1, 500 do
            t[i]()
        end
    end)
end

timer.performWithDelay(1, function()
    memoryPressure()
    deepPcall(200)
    stressDisplayAndGC()
    stackPressure()
    cycles = cycles + 1
    label.text = "Lua stress - cycles: " .. cycles
end, 0)

timer.performWithDelay(10000, function()
    label.text = "Survived " .. cycles .. " cycles"
    print("test_lua_stress: survived " .. cycles .. " cycles")
end)
