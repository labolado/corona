------------------------------------------------------------------------------
-- SDF Shapes Visual Demo
-- Tests all 15 shapes + stroke + shadow + gradient + boolean ops
------------------------------------------------------------------------------

local sdf = require("sdf_shapes")

-- Background
display.setDefault("background", 0.12, 0.12, 0.14)

-- Layout config — bigger shapes, more spacing
local COL = 4
local R = 38             -- default radius
local SX, SY = 55, 90    -- grid start
local DX, DY = 90, 100   -- grid spacing
local LABEL_OFFSET = 48
local SHAPE_COLOR = {0.3, 0.7, 1.0}

local function addLabel(x, y, text)
    local t = display.newText(text, x, y, native.systemFont, 10)
    t:setFillColor(0.6, 0.6, 0.6)
end

local function color(shape)
    shape:setFillColor(SHAPE_COLOR[1], SHAPE_COLOR[2], SHAPE_COLOR[3])
end

local function pos(col, row)
    return SX + col * DX, SY + row * DY
end

local x, y

-- ═══════════════════════════════════════════════
-- Row 0: circle, ellipse, rect, roundedRect
-- ═══════════════════════════════════════════════
x,y = pos(0,0); local c = sdf.newCircle(x,y,R); color(c); addLabel(x,y+LABEL_OFFSET,"circle")
x,y = pos(1,0); local e = sdf.newEllipse(x,y,72,48); color(e); addLabel(x,y+LABEL_OFFSET,"ellipse")
x,y = pos(2,0); local r = sdf.newRect(x,y,68,50); color(r); addLabel(x,y+LABEL_OFFSET,"rect")
x,y = pos(3,0); local rr = sdf.newRoundedRect(x,y,68,50,12); color(rr); addLabel(x,y+LABEL_OFFSET,"roundedRect")

-- ═══════════════════════════════════════════════
-- Row 1: hexagon, pentagon, octagon, triangle
-- ═══════════════════════════════════════════════
x,y = pos(0,1); local hex = sdf.newHexagon(x,y,R); color(hex); addLabel(x,y+LABEL_OFFSET,"hexagon")
x,y = pos(1,1); local pen = sdf.newPentagon(x,y,R); color(pen); addLabel(x,y+LABEL_OFFSET,"pentagon")
x,y = pos(2,1); local oct = sdf.newOctagon(x,y,R); color(oct); addLabel(x,y+LABEL_OFFSET,"octagon")
x,y = pos(3,1); local tri = sdf.newTriangle(x,y,R); color(tri); addLabel(x,y+LABEL_OFFSET,"triangle")

-- ═══════════════════════════════════════════════
-- Row 2: diamond, star5, star3, star8
-- ═══════════════════════════════════════════════
x,y = pos(0,2); local dia = sdf.newDiamond(x,y,60,72); color(dia); addLabel(x,y+LABEL_OFFSET,"diamond")
x,y = pos(1,2); local s5 = sdf.newStar(x,y,R,5,R*0.4); color(s5); addLabel(x,y+LABEL_OFFSET,"star 5pt")
x,y = pos(2,2); local s3 = sdf.newStar(x,y,R,3,R*0.4); color(s3); addLabel(x,y+LABEL_OFFSET,"star 3pt")
x,y = pos(3,2); local s8 = sdf.newStar(x,y,R,8,R*0.35); color(s8); addLabel(x,y+LABEL_OFFSET,"star 8pt")

-- ═══════════════════════════════════════════════
-- Row 3: ring, arc, crescent, heart
-- ═══════════════════════════════════════════════
x,y = pos(0,3); local rng = sdf.newRing(x,y,R,R*0.6); color(rng); addLabel(x,y+LABEL_OFFSET,"ring")
x,y = pos(1,3); local arc = sdf.newRing(x,y,R,R*0.6,0,270); color(arc); addLabel(x,y+LABEL_OFFSET,"arc 270")
x,y = pos(2,3); local cre = sdf.newCrescent(x,y,R,0.35); color(cre); addLabel(x,y+LABEL_OFFSET,"crescent")
x,y = pos(3,3); local hrt = sdf.newHeart(x,y,R); color(hrt); addLabel(x,y+LABEL_OFFSET,"heart")

-- ═══════════════════════════════════════════════
-- Row 4: cross, pill, star12, (removeSelf)
-- ═══════════════════════════════════════════════
x,y = pos(0,4); local crs = sdf.newCross(x,y,R*2,0.3); color(crs); addLabel(x,y+LABEL_OFFSET,"cross")
x,y = pos(1,4); local pil = sdf.newPill(x,y,80,36); color(pil); addLabel(x,y+LABEL_OFFSET,"pill")
x,y = pos(2,4); local s12 = sdf.newStar(x,y,R,12,R*0.3); color(s12); addLabel(x,y+LABEL_OFFSET,"star 12pt")
x,y = pos(3,4)
local tmp = sdf.newCircle(x,y,R*0.6)
tmp:setFillColor(1, 0.3, 0.3)
addLabel(x,y+LABEL_OFFSET,"remove @2s")
timer.performWithDelay(2000, function() tmp:removeSelf() end)

-- ═══════════════════════════════════════════════
-- Row 5: Stroke tests
-- ═══════════════════════════════════════════════
x,y = pos(0,5)
local sc = sdf.newCircle(x,y,R)
sc:setFillColor(0.3, 0.7, 1.0)
sc:setStrokeColor(1, 1, 0)
sc.strokeWidth = 4
addLabel(x,y+LABEL_OFFSET,"stroke")

x,y = pos(1,5)
local sr = sdf.newRoundedRect(x,y,68,50,12)
sr:setFillColor(0.3, 0.7, 1.0)
sr:setStrokeColor(1, 0.5, 0)
sr.strokeWidth = 3
addLabel(x,y+LABEL_OFFSET,"strRect")

x,y = pos(2,5)
local ss = sdf.newStar(x,y,R,5,R*0.4)
ss:setFillColor(0.3, 0.7, 1.0)
ss:setStrokeColor(1, 0, 0.5)
ss.strokeWidth = 4
addLabel(x,y+LABEL_OFFSET,"strStar")

x,y = pos(3,5)
local dc = sdf.newCircle(x,y,R)
dc:setFillColor(0.3, 0.7, 1.0)
dc:setStrokeColor(0, 1, 0.5)
local swAnim = 0
timer.performWithDelay(50, function()
    swAnim = (swAnim + 0.5) % 10
    dc.strokeWidth = swAnim
end, 0)
addLabel(x,y+LABEL_OFFSET,"anim stroke")

-- ═══════════════════════════════════════════════
-- Row 6: Shadow + combined
-- ═══════════════════════════════════════════════
x,y = pos(0,6)
local shc = sdf.newCircle(x,y,R)
shc:setFillColor(0.3, 0.7, 1.0)
shc.shadow = { offsetX=4, offsetY=4, blur=8, color={0,0,0,0.5} }
addLabel(x,y+LABEL_OFFSET,"shadow")

x,y = pos(1.5,6)
local combo = sdf.newRoundedRect(x,y,150,56,14)
combo:setFillColor(1, 1, 1)
combo:setStrokeColor(0.2, 0.5, 1.0)
combo.strokeWidth = 3
combo.shadow = { offsetX=5, offsetY=5, blur=12, color={0,0,0,0.35} }
addLabel(x,y+LABEL_OFFSET,"stroke + shadow")

-- ═══════════════════════════════════════════════
-- Row 7: Gradient + Boolean ops
-- ═══════════════════════════════════════════════
x,y = pos(0,7)
local gc = sdf.newCircle(x,y,R)
gc:setFillGradient({ color1={1,0,0}, color2={0,0,1}, direction="down" })
addLabel(x,y+LABEL_OFFSET,"gradient")

x,y = pos(1,7)
local gr = sdf.newRoundedRect(x,y,68,50,12)
gr:setFillGradient({ color1={1,1,0}, color2={0,1,0}, direction="right" })
addLabel(x,y+LABEL_OFFSET,"gradRect")

x,y = pos(2,7)
local uA = sdf.newCircle(-12, 0, 24); uA:setFillColor(1, 0.3, 0.3)
local uB = sdf.newCircle(12, 0, 24);  uB:setFillColor(0.3, 0.3, 1)
local un = sdf.union(uA, uB)
un.x, un.y = x, y
addLabel(x,y+LABEL_OFFSET,"union")

x,y = pos(3,7)
local iA = sdf.newCircle(-12, 0, 24); iA:setFillColor(1, 0.5, 0)
local iB = sdf.newCircle(12, 0, 24);  iB:setFillColor(1, 0.5, 0)
local inter = sdf.intersect(iA, iB)
inter.x, inter.y = x, y
addLabel(x,y+LABEL_OFFSET,"intersect")

print("SDF Shapes Demo loaded.")
