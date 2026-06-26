display.setStatusBar(display.HiddenStatusBar)
system.setIdleTimer(false)
local W, H = display.contentWidth, display.contentHeight
display.setDefault("background", 0.04, 0.04, 0.06)

-- Shape vertex counts for instanced shader (hard-coded)
local SHAPE_VERTS = {5, 10, 7, 6, 8, 6, 3, 10}

local group = display.newSDFGroup(200)

local cols, rows = 20, 10
local cellW, cellH = W / cols, (H - 80) / rows

for i = 1, 200 do
    local col = (i - 1) % cols
    local row = math.floor((i - 1) / cols)
    local x = cellW * (col + 0.5)
    local y = 80 + cellH * (row + 0.5)
    local shapeId = i % 8
    group:addShape({
        x = x, y = y,
        w = cellW * 0.45, h = cellH * 0.45,
        rotation = (i % 8) * 0.785,
        shapeId = shapeId,
        r = (col % 5) / 4,
        g = 0.55 + (row % 3) * 0.12,
        b = 0.95 - (col % 4) * 0.12,
        a = 1,
    })
end

local hud = display.newText("SDF Instanced: " .. group:numShapes() .. " shapes / 1 draw", W * 0.5, 20, native.systemFont, 14)
hud:setFillColor(1, 1, 1)

-- Auto-exit after 8s for FTL
timer.performWithDelay(8000, function() os.exit(0) end)
