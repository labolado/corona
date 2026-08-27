--[[
    test_sdf_instanced.lua — GPU-instanced SDF shapes, tap to add 50 random shapes
    Uses display.newSDFGroup() — 1 draw call for all shapes.
    Rotation: animated per shape via group:updateShape().
--]]
display.setStatusBar(display.HiddenStatusBar)
system.setIdleTimer(false)
math.randomseed(os.time())
display.setDefault("background", 0.15, 0.15, 0.22)
local W, H = display.contentWidth, display.contentHeight

local group = display.newSDFGroup(1000)

-- Shape vocabulary (matches hard-coded shader: 0=5gon 1=5star 2=arrow 3=6gon 4=4star 5=house 6=triangle 7=6star)
local SHAPE_IDS  = {0,1,2,3,4,5,6,7}
local SHAPE_NAME  = {"5gon","5star","arrow","6gon","4star","house","tri","6star"}
local SHAPE_COLOR = {
    {0.4,0.65,1.0},{1.0,0.78,0.15},{0.25,0.85,0.45},{0.8,0.3,0.75},
    {0.9,0.45,0.2},{0.4,0.85,0.85},{1.0,0.4,0.4},{0.6,0.9,0.3},
    {0.9,0.7,0.2},{0.5,0.3,1.0},{0.2,0.9,0.7},{1.0,0.5,0.7},
}

local sz = 90
local slots = {}   -- {idx, angle, spd, shapeId}

local function addShape(x, y, shapeId, colorId, angle, spd)
    local si = shapeId or math.random(#SHAPE_IDS)
    local ci = colorId or math.random(#SHAPE_COLOR)
    local c = SHAPE_COLOR[ci]
    local idx = group:addShape({
        x = x, y = y, w = sz, h = sz,
        rotation = angle or 0,
        shapeId = SHAPE_IDS[si],
        r = c[1], g = c[2], b = c[3], a = 1,
    })
    if idx then
        local s = spd or (math.random()*0.04 - 0.02)
        if not spd and math.abs(s) < 0.008 then s = 0.012 end
        slots[#slots+1] = {idx=idx, angle=angle or 0, spd=s, shapeId=SHAPE_IDS[si]}
    end
    return idx
end

-- HUD
local hud = display.newText("Instanced: 6 shapes / 1 draw  -- fps", W*0.38, H*0.038, native.systemFont, 11)
hud:setFillColor(1, 0.9, 0.5)

-- Initial 6 shapes
local initDefs = {
    {x=W*0.2, y=H*0.30, sid=1, cid=1, label="Pentagon", spd= 0.20},
    {x=W*0.5, y=H*0.30, sid=2, cid=2, label="5-Star",   spd=-0.25},
    {x=W*0.8, y=H*0.30, sid=3, cid=3, label="Arrow",    spd= 0.15},
    {x=W*0.2, y=H*0.68, sid=4, cid=4, label="Hexagon",  spd=-0.18},
    {x=W*0.5, y=H*0.68, sid=5, cid=5, label="4-Star",   spd= 0.22},
    {x=W*0.8, y=H*0.68, sid=6, cid=6, label="L-shape",  spd=-0.12},
}
for _, d in ipairs(initDefs) do
    addShape(d.x, d.y, d.sid, d.cid, 0, d.spd)
    local lbl = display.newText(d.label, d.x, d.y + sz*0.56, native.systemFont, 10)
    lbl:setFillColor(0.7, 0.7, 0.7)
end

-- Tap to add 50 random shapes
Runtime:addEventListener("tap", function(event)
    for i = 1, 50 do
        addShape(math.random(sz/2, W-sz/2), math.random(sz/2+50, H-sz/2))
    end
    hud:toFront()
    return true
end)

-- FPS counter + rotation animation
local frameCount, lastTime = 0, system.getTimer()
local fpsSmooth = 60
local t = 0

Runtime:addEventListener("enterFrame", function()
    -- Rotate all shapes
    for _, s in ipairs(slots) do
        s.angle = s.angle + s.spd
        group:updateShape(s.idx, {rotation = s.angle})
    end

    t = t + 1
    frameCount = frameCount + 1
    local now = system.getTimer()
    local elapsed = now - lastTime
    if elapsed >= 1000 then
        local fps = math.floor(frameCount / (elapsed / 1000) + 0.5)
        fpsSmooth = fpsSmooth*0.9 + fps*0.1
        frameCount = 0; lastTime = now
    end

    if t % 4 == 0 then
        hud.text = string.format("Instanced: %d shapes / 1 draw  %.0f fps",
            group:numShapes(), fpsSmooth)
        hud:toFront()
    end
end)

-- Auto-exit only for FTL
if _G.gameLoopScenario or os.getenv("FTL_MODE") then
    timer.performWithDelay(8000, function() os.exit(0) end)
end

print("=== SDF_INSTANCED START | " .. group:numShapes() .. " shapes ===")
