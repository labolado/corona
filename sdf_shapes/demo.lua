------------------------------------------------------------------------------
-- SDF Shapes Visual Demo
-- Tests all 15 shapes in a 5x4 grid layout
------------------------------------------------------------------------------

local sdf = require("sdf_shapes")

-- Background
display.setDefault("background", 0.15, 0.15, 0.15)

-- Layout config
local GRID_START_X = 60
local GRID_START_Y = 80
local GRID_SPACING_X = 70
local GRID_SPACING_Y = 80
local SHAPE_COLOR = {0.3, 0.7, 1.0}
local LABEL_COLOR = {1, 1, 1}

-- Helper: create label text below shape
local function addLabel(x, y, text)
    local label = display.newText(text, x, y, native.systemFont, 12)
    label:setFillColor(LABEL_COLOR[1], LABEL_COLOR[2], LABEL_COLOR[3])
    return label
end

-- Helper: set fill color on shape
local function colorize(shape)
    shape:setFillColor(SHAPE_COLOR[1], SHAPE_COLOR[2], SHAPE_COLOR[3])
end

-- Grid helper: compute (x, y) for position i (0-19)
local function getGridPos(i)
    local col = i % 5
    local row = math.floor(i / 5)
    return GRID_START_X + col * GRID_SPACING_X,
           GRID_START_Y + row * GRID_SPACING_Y
end

-- Row 1: circle, ellipse, rect, roundedRect, hexagon
local i = 0
local x, y

x, y = getGridPos(i)
local circle = sdf.newCircle(x, y, 25)
colorize(circle)
addLabel(x, y + 40, "circle")
i = i + 1

x, y = getGridPos(i)
local ellipse = sdf.newEllipse(x, y, 50, 35)
colorize(ellipse)
addLabel(x, y + 45, "ellipse")
i = i + 1

x, y = getGridPos(i)
local rect = sdf.newRect(x, y, 50, 40)
colorize(rect)
addLabel(x, y + 45, "rect")
i = i + 1

x, y = getGridPos(i)
local roundedRect = sdf.newRoundedRect(x, y, 50, 40, 8)
colorize(roundedRect)
addLabel(x, y + 45, "roundedRect")
i = i + 1

x, y = getGridPos(i)
local hexagon = sdf.newHexagon(x, y, 25)
colorize(hexagon)
addLabel(x, y + 40, "hexagon")
i = i + 1

-- Row 2: pentagon, octagon, triangle, diamond, star(5pt)
x, y = getGridPos(i)
local pentagon = sdf.newPentagon(x, y, 25)
colorize(pentagon)
addLabel(x, y + 40, "pentagon")
i = i + 1

x, y = getGridPos(i)
local octagon = sdf.newOctagon(x, y, 25)
colorize(octagon)
addLabel(x, y + 40, "octagon")
i = i + 1

x, y = getGridPos(i)
local triangle = sdf.newTriangle(x, y, 25)
colorize(triangle)
addLabel(x, y + 40, "triangle")
i = i + 1

x, y = getGridPos(i)
local diamond = sdf.newDiamond(x, y, 45, 45)
colorize(diamond)
addLabel(x, y + 40, "diamond")
i = i + 1

x, y = getGridPos(i)
local star5 = sdf.newStar(x, y, 25, 5, 12)
colorize(star5)
addLabel(x, y + 40, "star(5pt)")
i = i + 1

-- Row 3: ring, arc(270°), crescent, heart, cross
x, y = getGridPos(i)
local ring = sdf.newRing(x, y, 25, 15, 0, math.pi * 2)
colorize(ring)
addLabel(x, y + 40, "ring")
i = i + 1

x, y = getGridPos(i)
local arc = sdf.newRing(x, y, 25, 15, 0, math.pi * 1.5)
colorize(arc)
addLabel(x, y + 40, "arc(270°)")
i = i + 1

x, y = getGridPos(i)
local crescent = sdf.newCrescent(x, y, 25, 8)
colorize(crescent)
addLabel(x, y + 40, "crescent")
i = i + 1

x, y = getGridPos(i)
local heart = sdf.newHeart(x, y, 25)
colorize(heart)
addLabel(x, y + 40, "heart")
i = i + 1

x, y = getGridPos(i)
local cross = sdf.newCross(x, y, 40, 8)
colorize(cross)
addLabel(x, y + 40, "cross")
i = i + 1

-- Row 4: pill, star(3pt), star(8pt), star(12pt), removeSelf test
x, y = getGridPos(i)
local pill = sdf.newPill(x, y, 45, 40)
colorize(pill)
addLabel(x, y + 45, "pill")
i = i + 1

x, y = getGridPos(i)
local star3 = sdf.newStar(x, y, 25, 3, 10)
colorize(star3)
addLabel(x, y + 40, "star(3pt)")
i = i + 1

x, y = getGridPos(i)
local star8 = sdf.newStar(x, y, 25, 8, 12)
colorize(star8)
addLabel(x, y + 40, "star(8pt)")
i = i + 1

x, y = getGridPos(i)
local star12 = sdf.newStar(x, y, 25, 12, 15)
colorize(star12)
addLabel(x, y + 40, "star(12pt)")
i = i + 1

-- ─── Stroke Tests ───
local strokeTestY = GRID_START_Y + 5 * GRID_SPACING_Y
local strokeCircle = sdf.newCircle(GRID_START_X + 0 * GRID_SPACING_X, strokeTestY, 25)
strokeCircle:setFillColor(0.3, 0.7, 1.0)
strokeCircle:setStrokeColor(1, 1, 0)
strokeCircle.strokeWidth = 3
addLabel(GRID_START_X + 0 * GRID_SPACING_X, strokeTestY + 40, "stroke")

local strokeRect = sdf.newRoundedRect(GRID_START_X + 1 * GRID_SPACING_X, strokeTestY, 50, 40, 8)
strokeRect:setFillColor(0.3, 0.7, 1.0)
strokeRect:setStrokeColor(1, 0.5, 0)
strokeRect.strokeWidth = 2
addLabel(GRID_START_X + 1 * GRID_SPACING_X, strokeTestY + 45, "strRect")

local strokeStar = sdf.newStar(GRID_START_X + 2 * GRID_SPACING_X, strokeTestY, 25, 5, 12)
strokeStar:setFillColor(0.3, 0.7, 1.0)
strokeStar:setStrokeColor(1, 0, 0.5)
strokeStar.strokeWidth = 3
addLabel(GRID_START_X + 2 * GRID_SPACING_X, strokeTestY + 40, "strStar")

-- Animated stroke width
local dynCircle = sdf.newCircle(GRID_START_X + 3 * GRID_SPACING_X, strokeTestY, 25)
dynCircle:setFillColor(0.3, 0.7, 1.0)
dynCircle:setStrokeColor(0, 1, 0.5)
local sw = 0
timer.performWithDelay(50, function()
    sw = (sw + 0.5) % 8
    dynCircle.strokeWidth = sw
end, 0)
addLabel(GRID_START_X + 3 * GRID_SPACING_X, strokeTestY + 40, "anim")

-- ─── Shadow Tests ───
local shadowCircle = sdf.newCircle(GRID_START_X + 4 * GRID_SPACING_X, strokeTestY, 25)
shadowCircle:setFillColor(0.3, 0.7, 1.0)
shadowCircle.shadow = { offsetX = 3, offsetY = 3, blur = 6, color = {0,0,0,0.4} }
addLabel(GRID_START_X + 4 * GRID_SPACING_X, strokeTestY + 40, "shadow")

-- Combined stroke + shadow
local shadowTestY = GRID_START_Y + 6 * GRID_SPACING_Y
local combo = sdf.newRoundedRect(GRID_START_X + 1 * GRID_SPACING_X, shadowTestY, 120, 50, 12)
combo:setFillColor(1, 1, 1)
combo:setStrokeColor(0.2, 0.5, 1.0)
combo.strokeWidth = 2
combo.shadow = { offsetX = 4, offsetY = 4, blur = 10, color = {0,0,0,0.3} }
addLabel(GRID_START_X + 1 * GRID_SPACING_X, shadowTestY + 45, "stroke+shadow")

-- Row 4, Col 5: removeSelf test (red circle, auto-remove after 2s)
x, y = getGridPos(i)
local testRemove = sdf.newCircle(x, y, 20)
testRemove:setFillColor(1, 0.2, 0.2)
addLabel(x, y + 40, "remove @2s")

-- Schedule removal
timer.performWithDelay(2000, function()
    testRemove:removeSelf()
end)

print("SDF Shapes Demo loaded. 15 shapes + removeSelf test.")
