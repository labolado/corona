--[[
    test_realworld.lua - Real-world game scenario benchmark

    Usage: SOLAR2D_TEST=realworld SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Simulates a typical 2D game: static UI + dynamic gameplay objects.
    This is where static geometry cache and draw call batching matter.
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Real-World Benchmark ===")
print("Backend: " .. backend)

-- Configuration
local NUM_STATIC_UI = 120      -- buttons, labels, panels, HUD elements
local NUM_DYNAMIC_OBJECTS = 200 -- moving game entities
local NUM_PARTICLES = 300      -- small particle-like objects
local MEASURE_FRAMES = 300
local WARMUP_FRAMES = 30

-------------------------------
-- Layer 1: Static Background
-------------------------------
local bgGroup = display.newGroup()

-- Sky gradient (multiple rects)
for i = 0, 9 do
    local r = display.newRect(bgGroup, display.contentCenterX, i * 48, display.contentWidth, 48)
    local t = i / 9
    r:setFillColor(0.1 + t * 0.1, 0.15 + t * 0.2, 0.3 + t * 0.3)
end

-- Ground
local ground = display.newRect(bgGroup, display.contentCenterX, display.contentHeight - 20, display.contentWidth, 40)
ground:setFillColor(0.2, 0.35, 0.15)

-------------------------------
-- Layer 2: Static UI (HUD)
-------------------------------
local uiGroup = display.newGroup()
local staticCount = 0

-- Top bar
local topBar = display.newRect(uiGroup, display.contentCenterX, 15, display.contentWidth, 30)
topBar:setFillColor(0, 0, 0, 0.7)

-- Score, lives, level labels
local scoreLabel = display.newText({ parent = uiGroup, text = "Score: 0", x = 50, y = 15, font = native.systemFontBold, fontSize = 11 })
local livesLabel = display.newText({ parent = uiGroup, text = "Lives: 3", x = 160, y = 15, font = native.systemFontBold, fontSize = 11 })
local levelLabel = display.newText({ parent = uiGroup, text = "Level: 1", x = 270, y = 15, font = native.systemFontBold, fontSize = 11 })
staticCount = staticCount + 4

-- Bottom toolbar with buttons
local toolbar = display.newRect(uiGroup, display.contentCenterX, display.contentHeight - 15, display.contentWidth, 30)
toolbar:setFillColor(0.1, 0.1, 0.1, 0.8)
staticCount = staticCount + 1

for i = 1, 8 do
    local btnX = (i - 0.5) * (display.contentWidth / 8)
    local btn = display.newRoundedRect(uiGroup, btnX, display.contentHeight - 15, 35, 22, 4)
    btn:setFillColor(0.25, 0.25, 0.35)
    btn.strokeWidth = 1
    btn:setStrokeColor(0.4, 0.4, 0.5)
    local lbl = display.newText({ parent = uiGroup, text = tostring(i), x = btnX, y = display.contentHeight - 15, font = native.systemFont, fontSize = 9 })
    staticCount = staticCount + 2
end

-- Side panels
for side = 0, 1 do
    local x = side == 0 and 15 or (display.contentWidth - 15)
    local panel = display.newRoundedRect(uiGroup, x, display.contentCenterY, 25, 200, 5)
    panel:setFillColor(0, 0, 0, 0.5)
    staticCount = staticCount + 1

    for j = 1, 6 do
        local icon = display.newCircle(uiGroup, x, display.contentCenterY - 80 + j * 28, 8)
        icon:setFillColor(0.3 + j * 0.1, 0.4, 0.6)
        staticCount = staticCount + 1
    end
end

-- Fill remaining static UI (inventory grid, status icons, etc.)
local gridStartX, gridStartY = 40, 40
while staticCount < NUM_STATIC_UI do
    local col = (staticCount - 20) % 12
    local row = math.floor((staticCount - 20) / 12)
    local x = gridStartX + col * 22
    local y = gridStartY + row * 22

    if y < display.contentHeight - 40 and x < display.contentWidth - 30 then
        local item = display.newRect(uiGroup, x, y, 18, 18)
        item:setFillColor(0.2 + math.random() * 0.3, 0.2 + math.random() * 0.3, 0.3 + math.random() * 0.3, 0.3)
    end
    staticCount = staticCount + 1
end

print(string.format("  Static UI: %d objects", staticCount))

-------------------------------
-- Layer 3: Dynamic game objects
-------------------------------
local dynamicGroup = display.newGroup()
local dynamicObjects = {}

for i = 1, NUM_DYNAMIC_OBJECTS do
    local x = math.random(30, display.contentWidth - 30)
    local y = math.random(35, display.contentHeight - 35)
    local obj

    local kind = i % 4
    if kind == 0 then
        obj = display.newRect(dynamicGroup, x, y, 10, 10)
        obj:setFillColor(0.8, 0.3, 0.2)
    elseif kind == 1 then
        obj = display.newCircle(dynamicGroup, x, y, 5)
        obj:setFillColor(0.2, 0.7, 0.3)
    elseif kind == 2 then
        obj = display.newRoundedRect(dynamicGroup, x, y, 12, 8, 2)
        obj:setFillColor(0.3, 0.3, 0.8)
    else
        obj = display.newRect(dynamicGroup, x, y, 6, 14)
        obj:setFillColor(0.7, 0.6, 0.2)
    end

    obj.vx = (math.random() - 0.5) * 3
    obj.vy = (math.random() - 0.5) * 3
    obj.rotSpeed = (math.random() - 0.5) * 8
    table.insert(dynamicObjects, obj)
end

-------------------------------
-- Layer 4: Particles
-------------------------------
local particleGroup = display.newGroup()
local particles = {}

for i = 1, NUM_PARTICLES do
    local p = display.newRect(particleGroup, math.random(30, display.contentWidth - 30),
        math.random(35, display.contentHeight - 35), 3, 3)
    p:setFillColor(1, 0.8 + math.random() * 0.2, 0.3, 0.4 + math.random() * 0.4)
    p.vx = (math.random() - 0.5) * 2
    p.vy = -math.random() * 2 - 1
    p.life = math.random(60, 180)
    p.age = math.random(0, p.life)
    table.insert(particles, p)
end

print(string.format("  Dynamic: %d objects + %d particles", NUM_DYNAMIC_OBJECTS, NUM_PARTICLES))
print(string.format("  Total: %d display objects", staticCount + NUM_DYNAMIC_OBJECTS + NUM_PARTICLES + 11))

-------------------------------
-- Status display
-------------------------------
local fpsText = display.newText({
    text = "FPS: --", x = display.contentCenterX, y = 28,
    font = native.systemFontBold, fontSize = 10
})
fpsText:setFillColor(1, 1, 0)

local infoText = display.newText({
    text = string.format("%s | static:%d dyn:%d part:%d", backend, NUM_STATIC_UI, NUM_DYNAMIC_OBJECTS, NUM_PARTICLES),
    x = display.contentCenterX, y = display.contentHeight - 28,
    font = native.systemFont, fontSize = 8
})
infoText:setFillColor(0.7, 0.7, 0.7)

-------------------------------
-- Update loop
-------------------------------
local frameCount = 0
local frameTimes = {}
local lastFrameTime = 0
local phase = "warmup"
local score = 0

local function onEnterFrame()
    -- Update dynamic objects
    local left, right = 30, display.contentWidth - 30
    local top, bottom = 35, display.contentHeight - 35

    for i = 1, #dynamicObjects do
        local obj = dynamicObjects[i]
        obj.x = obj.x + obj.vx
        obj.y = obj.y + obj.vy
        obj.rotation = obj.rotation + obj.rotSpeed
        if obj.x < left or obj.x > right then obj.vx = -obj.vx end
        if obj.y < top or obj.y > bottom then obj.vy = -obj.vy end
    end

    -- Update particles (respawn when dead)
    for i = 1, #particles do
        local p = particles[i]
        p.x = p.x + p.vx
        p.y = p.y + p.vy
        p.age = p.age + 1
        p.alpha = math.max(0, 1 - p.age / p.life)

        if p.age >= p.life then
            p.x = math.random(30, display.contentWidth - 30)
            p.y = display.contentHeight - 40
            p.vy = -math.random() * 2 - 1
            p.age = 0
        end
    end

    -- Update score (static text changes occasionally)
    score = score + 1
    if score % 30 == 0 then
        scoreLabel.text = "Score: " .. score
    end

    -- Measure
    frameCount = frameCount + 1

    if phase == "warmup" then
        if frameCount >= WARMUP_FRAMES then
            phase = "measure"
            frameCount = 0
            frameTimes = {}
            lastFrameTime = system.getTimer()
        end
    elseif phase == "measure" then
        local now = system.getTimer()
        local dt = now - lastFrameTime
        lastFrameTime = now
        if dt > 0 then table.insert(frameTimes, 1000 / dt) end

        if frameCount >= MEASURE_FRAMES then
            phase = "done"
            local sum, min, max = 0, 999, 0
            for _, fps in ipairs(frameTimes) do
                sum = sum + fps
                if fps < min then min = fps end
                if fps > max then max = fps end
            end
            local avg = sum / #frameTimes

            local result = string.format(
                "\n=== REALWORLD BENCHMARK (%s) ===\n" ..
                "Static: %d | Dynamic: %d | Particles: %d | Total: %d\n" ..
                "FPS: avg=%.1f min=%.1f max=%.1f\n" ..
                "=== END ===\n",
                backend, NUM_STATIC_UI, NUM_DYNAMIC_OBJECTS, NUM_PARTICLES,
                staticCount + NUM_DYNAMIC_OBJECTS + NUM_PARTICLES,
                avg, min, max
            )
            print(result)
            fpsText.text = string.format("FPS: %.1f", avg)
            fpsText:setFillColor(avg >= 55 and 0.3 or 1, avg >= 55 and 1 or 0.3, 0.3)
        end
    end
end

Runtime:addEventListener("enterFrame", onEnterFrame)
