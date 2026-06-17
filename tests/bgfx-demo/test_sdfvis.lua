-- SDF visual verification — display static shapes with SDF ON.
-- Run: SOLAR2D_TEST=sdfvis. Captures nothing; visually inspect / screenshot.
local W, H = display.contentWidth, display.contentHeight
local cx, cy = display.contentCenterX, display.contentCenterY

display.setDefault("background", 0.15, 0.15, 0.2)

local sdfWanted = (os.getenv("SDF_VIS_OFF") ~= "1")
graphics.setSDF(sdfWanted)
print("SDF_VIS|setSDF(" .. tostring(sdfWanted) .. ")|actual=" .. tostring(graphics.getSDF()))

local noShapes = (os.getenv("SDF_VIS_NOSHAPES") == "1")
if not noShapes then
-- Row of circles (different colors)
local c1 = display.newCircle(cx - 200, cy - 120, 70); c1:setFillColor(1.0, 0.3, 0.3)
local c2 = display.newCircle(cx,       cy - 120, 70); c2:setFillColor(0.3, 1.0, 0.3)
local c3 = display.newCircle(cx + 200, cy - 120, 70); c3:setFillColor(0.3, 0.5, 1.0)

-- Row of rounded rects
local r1 = display.newRoundedRect(cx - 200, cy + 100, 130, 130, 30); r1:setFillColor(1.0, 0.8, 0.2)
local r2 = display.newRoundedRect(cx,       cy + 100, 130, 130, 60); r2:setFillColor(0.9, 0.4, 0.9)
local r3 = display.newRect(cx + 200, cy + 100, 130, 130);            r3:setFillColor(0.4, 0.9, 0.9)
end

local label = display.newText("SDF visual test — 3 circles + 2 roundedRect + 1 rect", cx, 30, native.systemFont, 16)
label:setFillColor(1, 1, 1)

print("SDF_VIS|shapes created|circles=3 roundedRect=2 rect=1")
