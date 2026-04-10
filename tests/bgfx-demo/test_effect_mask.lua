-- test_effect_mask.lua - Runtime shader + mask bug reproduction
-- Usage: SOLAR2D_TEST=effect_mask SOLAR2D_BACKEND=bgfx
--
-- Tests whether mask sampling is applied when a custom runtime effect is active.
-- In GL: mask always works regardless of effect.
-- Bug: In bgfx, TransformFragmentKernel replaces "return expr" with
--      "gl_FragColor = expr" but does NOT add mask multiplication.

local W = display.contentWidth
local H = display.contentHeight

local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.1, 0.1, 0.15)

-- Define a simple passthrough custom effect
-- Note: category "filter", name "passthrough" -> used as "filter.custom.passthrough"
graphics.defineEffect({
    category = "filter",
    name = "passthrough",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            return CoronaColorScale(texture2D(CoronaSampler0, texCoord));
        }
    ]]
})

local title = display.newText({
    text = "Runtime Shader + Mask Test",
    x = W/2, y = 20,
    font = native.systemFontBold, fontSize = 14
})
title:setFillColor(1, 1, 1)

-- Load the pre-created circular mask (RGBA PNG: white circle, black bg)
local mask = graphics.newMask("test_mask_circle.png")
if not mask then
    print("ERROR: failed to load mask PNG")
else
    print("Mask loaded OK")
end

local startY = 80
local rowH = 115
local colA = 75
local colB = 215

-- Helper: make labeled rect with color (using image fill so effect works)
local function makeRect(cx, cy, r, g, b, label)
    local rect = display.newRect(cx, cy, 110, 80)
    rect:setFillColor(r, g, b)
    local lbl = display.newText(label, cx, cy + 50, native.systemFont, 10)
    lbl:setFillColor(0.8, 0.8, 0.8)
    return rect
end

-- Helper: apply mask to a display object
local function applyMask(obj)
    obj:setMask(mask)
    -- Scale mask to roughly cover the object area
    obj.maskScaleX = 0.86   -- 110/128 = 0.86
    obj.maskScaleY = 0.625  -- 80/128 = 0.625
end

-- Row 1: Default shader (precompiled) - use image fill
local r1a = display.newRect(colA, startY, 110, 80)
r1a.fill = { type = "image", filename = "test_red.png" }
local l1a = display.newText("Default / No Mask", colA, startY + 50, native.systemFont, 10)
l1a:setFillColor(0.8, 0.8, 0.8)

local r1b = display.newRect(colB, startY, 110, 80)
r1b.fill = { type = "image", filename = "test_red.png" }
applyMask(r1b)
local l1b = display.newText("Default / +Mask", colB, startY + 50, native.systemFont, 10)
l1b:setFillColor(0.8, 0.8, 0.8)

-- Row 2: Custom runtime effect (the bug case)
local r2a = display.newRect(colA, startY + rowH, 110, 80)
r2a.fill = { type = "image", filename = "test_green.png" }
r2a.fill.effect = "filter.custom.passthrough"
local l2a = display.newText("Custom / No Mask", colA, startY + rowH + 50, native.systemFont, 10)
l2a:setFillColor(0.8, 0.8, 0.8)

local r2b = display.newRect(colB, startY + rowH, 110, 80)
r2b.fill = { type = "image", filename = "test_green.png" }
r2b.fill.effect = "filter.custom.passthrough"
applyMask(r2b)
local l2b = display.newText("Custom / +Mask", colB, startY + rowH + 50, native.systemFont, 10)
l2b:setFillColor(0.8, 0.8, 0.8)

-- Row 3: Built-in effect + mask
local r3a = display.newRect(colA, startY + rowH*2, 110, 80)
r3a.fill = { type = "image", filename = "test_blue.png" }
r3a.fill.effect = "filter.grayscale"
local l3a = display.newText("Grayscale / No Mask", colA, startY + rowH*2 + 50, native.systemFont, 10)
l3a:setFillColor(0.8, 0.8, 0.8)

local r3b = display.newRect(colB, startY + rowH*2, 110, 80)
r3b.fill = { type = "image", filename = "test_blue.png" }
r3b.fill.effect = "filter.grayscale"
applyMask(r3b)
local l3b = display.newText("Grayscale / +Mask", colB, startY + rowH*2 + 50, native.systemFont, 10)
l3b:setFillColor(0.8, 0.8, 0.8)

-- Legend
local legend = display.newText({
    text = "RIGHT col has mask. Expect: oval circle cutout.\nIf right rect shows full rectangle -> BUG.",
    x = W/2, y = startY + rowH*3 + 5,
    width = W - 20,
    font = native.systemFont, fontSize = 10,
    align = "center"
})
legend:setFillColor(1, 0.9, 0.5)

-- Column labels at top
local lA = display.newText("No Mask", colA, startY - 55, native.systemFontBold, 11)
lA:setFillColor(0.7, 1, 0.7)
local lB = display.newText("+Mask", colB, startY - 55, native.systemFontBold, 11)
lB:setFillColor(1, 0.7, 0.7)

print("=== Effect+Mask Test Ready ===")
print("Col A: no mask | Col B: with mask (should show oval circle cutout)")
print("Row 1: Default shader + image fill")
print("Row 2: Custom runtime effect (passthrough) -- BUG: mask may be missing in bgfx")
print("Row 3: Built-in grayscale effect")
