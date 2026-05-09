-- Minimal repro for black crescent bug
-- Run: SOLAR2D_TEST=crescent SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

display.setDefault("background", 1, 0.9, 0.7) -- light tan, like tank menu

-- Simple colored rect to see if crescent appears
local bg = display.newRect(display.contentCenterX, display.contentCenterY,
    display.contentWidth, display.contentHeight)
bg:setFillColor(1, 0.9, 0.7)

local text = display.newText("Black crescent bug test", display.contentCenterX, 100, native.systemFont, 24)
text:setFillColor(0)

-- A few simple shapes to see if crescent is global
local circle = display.newCircle(200, 300, 50)
circle:setFillColor(0.5, 0.8, 0.3)

local rect = display.newRect(400, 300, 100, 100)
rect:setFillColor(0.3, 0.5, 0.8)
