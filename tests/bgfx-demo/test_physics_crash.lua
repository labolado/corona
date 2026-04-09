-- test_physics_crash.lua
-- Reproduce physics UAF: snapshot.group gets physics body, snapshot removed,
-- body's UserData still points to freed DisplayObject → crash on next Step.
--
-- Key: create dangling pointers WITHOUT huge memory pressure.
-- MallocScribble=1 (macOS) or ASAN makes freed memory 0x55/poison → fast crash.
-- On iOS the natural allocator reuse is enough if we churn malloc.
--
-- Expected:
--   Bug present  → SIGSEGV within seconds (MallocScribble) or ~10-30s (iOS)
--   Bug fixed    → runs 60+ seconds stable, no crash, no OOM

local physics = require("physics")
physics.start()
physics.setGravity(0, 9.8)

local W = display.contentWidth
local H = display.contentHeight

local ground = display.newRect(W/2, H - 10, W, 20)
physics.addBody(ground, "static")

local frame = 0
local totalDangling = 0
local startTime = system.getTimer()
local label = display.newText("UAF test", W/2, 30, native.systemFont, 20)

-- Core UAF trigger: create snapshot → add physics to snapshot.group →
-- remove physics body → remove snapshot → body.UserData is dangling
local function plantBombs()
    for i = 1, 8 do
        local snap = display.newSnapshot(64, 64)
        if not snap then return end
        snap.x, snap.y = math.random(50, W-50), math.random(50, H-50)

        -- Add a small rect inside so snapshot has content
        local r = display.newRect(snap.group, 0, 0, 30, 30)
        r:setFillColor(math.random(), math.random(), math.random())

        pcall(function()
            physics.addBody(snap.group, "dynamic", {
                shape = {-20,-20, 20,-20, 20,20, -20,20}, density = 1
            })
        end)
        -- Remove body first, then snapshot — but the bug is in the destructor:
        -- ~DisplayObjectExtensions checks GetParent() which returns NULL for
        -- snapshot.group → skips SetUserData(NULL) → dangling pointer
        pcall(function() physics.removeBody(snap.group) end)
        display.remove(snap)
        totalDangling = totalDangling + 1
    end
end

-- Light memory churn to reuse freed pages (no OOM bomb)
-- Small canvas textures + Lua string churn to trigger malloc reuse
local canvasPool = {}
local function memoryChurn()
    -- Create small canvas textures (256x256 = 256KB each, very light)
    for i = 1, 3 do
        local c = graphics.newTexture({type="canvas", width=256, height=256})
        if c then
            local r = display.newRect(0, 0, 256, 256)
            r:setFillColor(math.random(), math.random(), math.random())
            c:draw(r)
            c:invalidate()
            display.remove(r)
            canvasPool[#canvasPool + 1] = c
        end
    end

    -- Keep pool small — just enough to churn memory, not OOM
    while #canvasPool > 8 do
        local old = table.remove(canvasPool, 1)
        old:releaseSelf()
    end

    -- Light Lua string churn to hit system malloc
    local strs = {}
    for i = 1, 10 do
        strs[i] = string.rep(string.char(math.random(1, 255)), 10000)
    end
    strs = nil
    collectgarbage("collect")
end

-- Moderate game simulation
local objects = {}
local function gameChurn()
    -- Physics objects
    for i = 1, 5 do
        local r = display.newRect(math.random(0, W), math.random(0, H),
                                   math.random(10, 30), math.random(10, 30))
        r:setFillColor(math.random(), math.random(), math.random())
        physics.addBody(r, "dynamic", {density = math.random() * 3})
        objects[#objects + 1] = r
    end

    -- Bullets
    for i = 1, 5 do
        local c = display.newCircle(math.random(0, W), 50, 3)
        physics.addBody(c, "dynamic", {density=5, radius=3})
        c:applyLinearImpulse(math.random(-3, 3), math.random(3, 8), c.x, c.y)
        objects[#objects + 1] = c
    end

    -- Cleanup old — keep count low
    while #objects > 200 do
        local old = table.remove(objects, 1)
        if old and old.removeSelf then old:removeSelf() end
    end
end

Runtime:addEventListener("enterFrame", function()
    frame = frame + 1

    plantBombs()
    gameChurn()
    memoryChurn()

    collectgarbage("collect")

    local elapsed = math.floor((system.getTimer() - startTime) / 1000)
    label.text = string.format("f=%d d=%d tex=%d t=%ds", frame, totalDangling, #canvasPool, elapsed)

    if frame % 30 == 0 then
        print(string.format("[UAF] f=%d dangling=%d objs=%d tex=%d elapsed=%ds",
            frame, totalDangling, #objects, #canvasPool, elapsed))
    end

    -- Success: survived 60 seconds
    if elapsed >= 60 and not _G._uafTestDone then
        _G._uafTestDone = true
        print("[UAF] PASS: 60 seconds without crash")
        label.text = "PASS: 60s no crash"
        label:setFillColor(0, 1, 0)
    end
end)

print("[UAF] Starting physics UAF reproduction test...")
print("[UAF] Bug present → SIGSEGV within seconds")
print("[UAF] Bug fixed → stable 60s, no crash, no OOM")
