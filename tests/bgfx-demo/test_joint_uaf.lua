-- test_joint_uaf.lua
-- Reproduce: PhysicsJoint::Finalizer crashes when b2World already destroyed joints
-- Google Play crash: b2Fixture::Destroy SIGSEGV (100+ users), PhysicsJoint::Finalizer (84 users)
local physics = require("physics")
physics.start()
physics.setGravity(0, 9.8)

local W = display.contentWidth
local H = display.contentHeight

local label = display.newText("Joint UAF test - cycles: 0", W/2, 30, native.systemFont, 16)
local cycles = 0
local phase = 1

-- Phase 1: Remove bodies without removing joint first
-- b2World::DestroyBody cascades to destroy joints, but Lua still holds joint reference
local function createAndDestroyJoints()
    local bodyA = display.newRect(W/2 - 50, H/2, 40, 40)
    physics.addBody(bodyA, "dynamic", {density=1, friction=0.3, bounce=0.5})

    local bodyB = display.newRect(W/2 + 50, H/2, 40, 40)
    physics.addBody(bodyB, "dynamic", {density=1, friction=0.3, bounce=0.5})

    local joint = physics.newJoint("distance", bodyA, bodyB, bodyA.x, bodyA.y, bodyB.x, bodyB.y)

    -- Remove bodies — Box2D destroys joint internally
    bodyA:removeSelf()
    bodyB:removeSelf()
    -- joint userdata now holds dangling b2Joint pointer

    -- Force GC to trigger PhysicsJoint::Finalizer
    joint = nil
    collectgarbage("collect")
    collectgarbage("collect")

    cycles = cycles + 1
    label.text = "Phase " .. phase .. " - cycles: " .. cycles
end

-- Run Phase 1 aggressively
timer.performWithDelay(1, function()
    for i = 1, 10 do
        createAndDestroyJoints()
    end
end, 0)

-- Phase 2: physics.stop() while joints exist (destroys b2World)
timer.performWithDelay(3000, function()
    phase = 2
    label.text = "Phase 2: stop/start physics with active joints"
    for i = 1, 5 do
        local a = display.newRect(math.random(50, W-50), math.random(50, H-50), 30, 30)
        local b = display.newRect(math.random(50, W-50), math.random(50, H-50), 30, 30)
        physics.addBody(a, "dynamic")
        physics.addBody(b, "dynamic")
        local j = physics.newJoint("distance", a, b, a.x, a.y, b.x, b.y)
    end
    -- Stop physics — destroys b2World and all joints
    physics.stop()
    -- GC runs — finalizers access destroyed joints
    collectgarbage("collect")
    collectgarbage("collect")

    physics.start()
    label.text = "Phase 2 done - survived!"
end)

-- Phase 3: Multiple joint types
timer.performWithDelay(5000, function()
    phase = 3
    label.text = "Phase 3: multiple joint types"
    for i = 1, 30 do
        local a = display.newRect(math.random(50, W-50), math.random(50, H-50), 20, 20)
        local b = display.newRect(math.random(50, W-50), math.random(50, H-50), 20, 20)
        physics.addBody(a, "dynamic")
        physics.addBody(b, "dynamic")

        -- Mix joint types
        local jtype = (i % 3 == 0) and "pivot" or "distance"
        if jtype == "pivot" then
            physics.newJoint("pivot", a, b, a.x, a.y)
        else
            physics.newJoint("distance", a, b, a.x, a.y, b.x, b.y)
        end

        -- Remove one body, keep joint reference dangling
        a:removeSelf()
    end
    collectgarbage("collect")
    collectgarbage("collect")
    label.text = "Phase 3 done - survived!"
end)

-- Phase 4: SetValueForKey on destroyed joint
timer.performWithDelay(7000, function()
    phase = 4
    label.text = "Phase 4: access joint after body destroyed"
    local a = display.newRect(W/2, H/2, 40, 40)
    local b = display.newRect(W/2 + 80, H/2, 40, 40)
    physics.addBody(a, "dynamic")
    physics.addBody(b, "dynamic")
    local joint = physics.newJoint("distance", a, b, a.x, a.y, b.x, b.y)

    a:removeSelf()
    -- Try to access joint properties after body destroyed
    pcall(function() local _ = joint.length end)
    pcall(function() joint.frequency = 1.0 end)

    b:removeSelf()
    joint = nil
    collectgarbage("collect")
    label.text = "Phase 4 done - survived!"
end)

timer.performWithDelay(9000, function()
    label.text = "ALL PHASES COMPLETE - survived " .. cycles .. " cycles"
    print("test_joint_uaf: ALL PHASES COMPLETE - survived " .. cycles .. " cycles")
end)
