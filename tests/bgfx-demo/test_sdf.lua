-- SDF visual test
-- Draws various shapes with SDF enabled to ensure geometry and shaders work
local W = display.contentWidth
local H = display.contentHeight

display.setDefault("background", 0.2, 0.2, 0.2)

local title = display.newText("SDF Visual Test", W/2, 20, native.systemFontBold, 16)
title:setFillColor(1, 1, 1)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local status = display.newText("Backend: " .. backend, W/2, 45, native.systemFont, 14)
status:setFillColor(0.8, 1, 0.5)

-- Enable SDF
graphics.setSDF(true)

-- Create a grid of shapes
local startX = 60
local startY = 100
local spacingX = 80
local spacingY = 100

-- Row 1: Circles
for i = 1, 4 do
    local c = display.newCircle(startX + (i-1)*spacingX, startY, 10 + i*5)
    c:setFillColor(0.2*i, 0.5, 1.0 - 0.2*i)
end

-- Row 2: Rectangles
for i = 1, 4 do
    local r = display.newRect(startX + (i-1)*spacingX, startY + spacingY, 20 + i*10, 20 + i*10)
    r:setFillColor(1.0, 0.2*i, 0.2)
end

-- Row 3: Rounded Rects
for i = 1, 4 do
    local rr = display.newRoundedRect(startX + (i-1)*spacingX, startY + spacingY*2, 40, 40, 2 + i*2)
    rr:setFillColor(0.2, 0.8, 0.2*i)
end

-- Row 4: Polygons
for i = 1, 4 do
    local s = 10 + i*5
    local p = display.newPolygon(startX + (i-1)*spacingX, startY + spacingY*3, {0, -s, s, s, -s, s})
    p:setFillColor(0.8, 0.8, 0.2)
end
