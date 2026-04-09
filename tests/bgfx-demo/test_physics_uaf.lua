-- test_physics_uaf.lua
-- Reproduce physics use-after-free crash:
-- PhysicsWorld::StepWorld() → DisplayObject::GetStage() on freed object
--
-- Strategy: rapidly create physics bodies and removeSelf() them
-- while physics is actively stepping, to trigger the race condition.

local physics = require("physics")
physics.start()
physics.setGravity(0, 9.8)

local W = display.contentWidth
local H = display.contentHeight

-- Ground
local ground = display.newRect(W/2, H - 20, W, 40)
ground:setFillColor(0.3, 0.3, 0.3)
physics.addBody(ground, "static", {density=1, friction=0.5, bounce=0.3})

local count = 0
local crashed = false

local label = display.newText("Physics UAF test - objects: 0", W/2, 30, native.systemFont, 16)

-- Method 1: Create and immediately remove in same frame
local function burstCreateAndRemove()
    for i = 1, 20 do
        local obj = display.newRect(math.random(50, W-50), math.random(50, 200),
                                     math.random(10, 40), math.random(10, 40))
        obj:setFillColor(math.random(), math.random(), math.random())
        physics.addBody(obj, "dynamic", {density=2, friction=0.3, bounce=0.5})

        -- Remove some immediately (same frame as addBody)
        if math.random() > 0.5 then
            obj:removeSelf()
            obj = nil
        end
    end
    count = count + 20
end

-- Method 2: Create objects, let physics step, then remove during collision
local activeObjects = {}

local function onCollision(event)
    if event.phase == "began" then
        -- Remove object during collision callback (while physics is stepping)
        local target = event.target
        if target and target.removeSelf then
            timer.performWithDelay(0, function()
                if target and target.removeSelf then
                    target:removeSelf()
                end
            end)
        end
    end
end

local function spawnAndRemoveOnCollision()
    for i = 1, 10 do
        local obj = display.newCircle(math.random(50, W-50), math.random(20, 100),
                                       math.random(5, 20))
        obj:setFillColor(math.random(), math.random(), 0)
        physics.addBody(obj, "dynamic", {density=3, friction=0.1, bounce=0.8, radius=obj.path.radius})
        obj:addEventListener("collision", onCollision)
        activeObjects[#activeObjects + 1] = obj
    end
    count = count + 10
end

-- Method 3: Remove object while applying force (physics accessing it)
local function removeWhileForce()
    local obj = display.newRect(W/2, 100, 30, 30)
    obj:setFillColor(1, 0, 0)
    physics.addBody(obj, "dynamic", {density=5})
    obj:applyLinearImpulse(math.random(-5,5), math.random(-5,5), obj.x, obj.y)

    -- Remove next frame while force is being applied
    timer.performWithDelay(1, function()
        if obj and obj.removeSelf then
            obj:removeSelf()
            obj = nil
        end
    end)
    count = count + 1
end

-- Method 4: Destroy group containing physics objects
local function destroyGroupWithPhysics()
    local g = display.newGroup()
    for i = 1, 15 do
        local r = display.newRect(g, math.random(50, W-50), math.random(50, 200),
                                   math.random(10, 30), math.random(10, 30))
        r:setFillColor(0, math.random(), math.random())
        physics.addBody(r, "dynamic", {density=2, friction=0.5, bounce=0.3})
    end

    -- Remove entire group after a tiny delay
    timer.performWithDelay(16, function()
        if g and g.removeSelf then
            g:removeSelf()
            g = nil
        end
    end)
    count = count + 15
end

-- Method 5: physics.removeBody + removeSelf race
local function removeBodyAndSelfRace()
    for i = 1, 10 do
        local obj = display.newRect(math.random(50, W-50), math.random(50, 150),
                                     math.random(10, 30), math.random(10, 30))
        obj:setFillColor(math.random(), 0, math.random())
        physics.addBody(obj, "dynamic", {density=2, bounce=0.5})

        -- Remove body and display object in rapid succession
        timer.performWithDelay(math.random(1, 3), function()
            if obj and obj.removeSelf then
                pcall(function() physics.removeBody(obj) end)
                obj:removeSelf()
                obj = nil
            end
        end)
    end
    count = count + 10
end

-- Run all methods in a tight loop
local frame = 0
local function onFrame()
    frame = frame + 1

    -- Cycle through different destruction patterns
    local method = frame % 20
    if method == 0 then burstCreateAndRemove()
    elseif method == 4 then spawnAndRemoveOnCollision()
    elseif method == 8 then removeWhileForce()
    elseif method == 12 then destroyGroupWithPhysics()
    elseif method == 16 then removeBodyAndSelfRace()
    end

    -- Cleanup stale refs from activeObjects
    if frame % 100 == 0 then
        local cleaned = {}
        for _, obj in ipairs(activeObjects) do
            if obj and obj.removeSelf and obj.x then
                -- Remove objects that fell off screen
                if obj.y > H + 100 then
                    obj:removeSelf()
                else
                    cleaned[#cleaned + 1] = obj
                end
            end
        end
        activeObjects = cleaned
    end

    label.text = string.format("Physics UAF test - frame:%d objects:%d active:%d",
                                frame, count, #activeObjects)
end

Runtime:addEventListener("enterFrame", onFrame)

print("=== Physics UAF reproduction test started ===")
print("If this crashes, the bug is confirmed.")
print("Expected crash: SIGSEGV in PhysicsWorld::StepWorld() -> DisplayObject::GetStage()")
