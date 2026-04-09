-- Deterministic repro: simulate tank's destructible terrain pattern
-- Multiple bullets hitting same terrain, each triggering removeBody+addBody
-- via timer.performWithDelay(1, ...) — exact pattern from spine_tank_gun.lua:364

local physics = require("physics")
physics.start()
physics.setGravity(0, 9.8)

local W = display.contentWidth
local H = display.contentHeight

display.newText("Multi-hit terrain UAF repro", W/2, 20, native.systemFont, 14)
local statusLabel = display.newText("cycle: 0", W/2, 40, native.systemFont, 14)

local ground = display.newRect(W/2, H - 10, W, 20)
physics.addBody(ground, "static")

local cycle = 0

-- Simulate the exact destructible_terrain.lua pattern:
-- physics.removeBody(terrain) then physics.addBody(terrain, type, unpack(shapes))
-- where shapes may be empty after clipper erosion
local function damageTerrainLikeGame(terrain, damageLevel)
    if not terrain or not terrain.removeSelf then return end

    -- Check if terrain still has physics body
    local hasBody = pcall(function()
        local _, _ = terrain.bodyType, terrain.x
    end)

    pcall(function() physics.removeBody(terrain) end)

    -- Simulate clipper output: fewer shapes as damage increases
    local shapes = {}
    local hw, hh = 25, 25
    local remaining = math.max(0, 6 - damageLevel)

    for i = 1, remaining do
        local shrink = math.max(0.1, 1.0 - damageLevel * 0.15)
        local ox = (i - remaining/2) * 8 * shrink
        local sz = 7 * shrink
        -- Filter by area (same as destructible_terrain.lua line 462: area > 16)
        local area = sz * sz * 4
        if area > 16 then
            shapes[#shapes + 1] = {
                density = 1, friction = 0.5, bounce = 0.2,
                shape = {ox-sz, -sz, ox+sz, -sz, ox+sz, sz, ox-sz, sz}
            }
        end
    end

    -- This is line 476: physics.addBody(terrian, type, unpack(shapes))
    if #shapes > 0 then
        pcall(function()
            physics.addBody(terrain, "static", unpack(shapes))
        end)
    else
        -- Terrain fully destroyed - remove it
        terrain:removeSelf()
        return "destroyed"
    end
    return "damaged"
end

local function runCycle()
    cycle = cycle + 1

    -- Create terrain (in a group, like the game)
    local group = display.newGroup()
    local terrain = display.newRect(group, W/2, H/2, 50, 50)
    terrain:setFillColor(0.3, 0.7, 0.2)
    physics.addBody(terrain, "static", {density=1, friction=0.5})

    -- Simulate multiple bullets hitting same terrain in rapid succession
    -- Each bullet triggers a timer.performWithDelay(1, ...) like the game
    local numBullets = math.random(3, 8)
    for b = 1, numBullets do
        timer.performWithDelay(1, function()
            -- Each bullet does removeBody + addBody with increasing damage
            if terrain and terrain.removeSelf and terrain.x then
                local result = damageTerrainLikeGame(terrain, b)
                if result == "destroyed" then
                    terrain = nil
                end
            end
        end)
    end

    -- After all bullets, destroy the group (like scene cleanup)
    timer.performWithDelay(math.random(50, 150), function()
        if group and group.removeSelf then
            group:removeSelf()
            group = nil
        end
        -- Force GC
        collectgarbage("collect")
        collectgarbage("collect")
    end)

    statusLabel.text = string.format("cycle:%d bullets:%d", cycle, numBullets)
end

-- Run a new cycle every 10 frames
local frame = 0
Runtime:addEventListener("enterFrame", function()
    frame = frame + 1
    if frame % 10 == 0 then
        runCycle()
    end
    -- Periodic GC stress
    if frame % 5 == 0 then
        collectgarbage("step", 100)
    end
end)

print("=== Multi-hit terrain UAF repro ===")
print("Simulating multiple bullets hitting same terrain + group cleanup.")
