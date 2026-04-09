-- Test: built-in generator effects in bgfx
display.setDefault("background", 0.2, 0.2, 0.2)

-- Test 1: linearGradient (built-in generator)
local r1 = display.newRect(display.contentCenterX, 100, 250, 60)
r1.fill.effect = "generator.linearGradient"
r1.fill.effect.color1 = {1, 0, 0, 1}
r1.fill.effect.color2 = {0, 0, 1, 1}

-- Test 2: radialGradient (built-in generator)
local r2 = display.newRect(display.contentCenterX, 200, 200, 200)
r2.fill.effect = "generator.radialGradient"
r2.fill.effect.color1 = {1, 1, 0, 1}
r2.fill.effect.color2 = {0, 0.5, 0, 1}

-- Test 3: checkerboard (built-in generator)
local r3 = display.newRect(display.contentCenterX, 350, 200, 60)
r3.fill.effect = "generator.checkerboard"

local label = display.newText("Generator Effects Test (bgfx)", display.contentCenterX, 420, native.systemFont, 14)
label:setFillColor(1, 1, 1)

print("TEST: built-in generator effects loaded")
