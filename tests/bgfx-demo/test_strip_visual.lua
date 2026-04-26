--[[
    test_strip_visual.lua - Deterministic A/B visual test

    Covers 5 categories the brief calls out:
      shapes   — mass strip (newRect with setFillColor)
      images   — texture switching (newImageRect from atlas)
      text     — newText interleaved with strip rect
      blend    — different blendMode draws back-to-back
      masks    — newRect inside masked group

    No animation. SCREENSHOT_READY printed at frame 60.
    Same pixel output expected for SOLAR2D_STRIP_BATCH=0 / =1.
--]]

display.setStatusBar(display.HiddenStatusBar)

local stripEnv = os.getenv("SOLAR2D_STRIP_BATCH")
print("=== Strip Visual A/B Test ===")
print("StripBatch: " .. (stripEnv == "0" and "DISABLED" or "ENABLED"))
math.randomseed(7)

local W = display.contentWidth
local H = display.contentHeight

-- 1) Background (single big strip)
local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.1, 0.1, 0.15)

-- 2) shapes: 200 rects with random colors (same texture/program)
local shapesGroup = display.newGroup()
for i = 1, 200 do
    local x = (i % 20) * 14 + 12
    local y = math.floor(i / 20) * 14 + 30
    local r = display.newRect(shapesGroup, x, y, 10, 10)
    r:setFillColor(math.random(), math.random(), math.random(), 0.85)
end

-- 3) images: alternating textures (force FillDirty0)
local images = display.newGroup()
images.y = 200
local textures = { "shape_white.png", "grass1.png", "soil2.jpg", "desert-track-3.png" }
for i = 1, 60 do
    local tex = textures[(i % #textures) + 1]
    local img = display.newImageRect(images, tex, 14, 14)
    img.x = (i % 20) * 16 + 12
    img.y = math.floor(i / 20) * 16
end

-- 4) text interleaved with strip
local textGroup = display.newGroup()
textGroup.y = 280
for i = 1, 12 do
    local r = display.newRect(textGroup, 20 + i*20, 0, 14, 14)
    r:setFillColor(i / 12, 0.5, 0.8)
    local t = display.newText(textGroup, tostring(i), 20 + i*20, 18, native.systemFont, 8)
    t:setFillColor(1, 1, 1)
end

-- 5) blend modes
local blendGroup = display.newGroup()
blendGroup.y = 320
local modes = { "normal", "add", "multiply", "screen" }
for i, m in ipairs(modes) do
    for j = 1, 5 do
        local r = display.newRect(blendGroup, 20 + (i-1)*60 + j*10, 0, 16, 16)
        r:setFillColor(0.4 + j*0.1, 0.3, 0.7, 0.7)
        r.blendMode = m
    end
end

-- 6) mask: rect inside masked group
local maskGroup = display.newGroup()
maskGroup.y = 360
local maskedRects = display.newGroup()
for k = 1, 30 do
    local r = display.newRect(maskedRects, k * 12, 0, 10, 10)
    r:setFillColor(1, k/30, 0.5)
end
maskGroup:insert(maskedRects)
local mask = graphics.newMask("shape_white.png")
maskedRects:setMask(mask)
maskedRects.maskScaleX = 200
maskedRects.maskScaleY = 30
maskedRects.maskX = 100
maskedRects.maskY = 0

-- Frame counter for screenshot signal
local frameCount = 0
Runtime:addEventListener("enterFrame", function()
    frameCount = frameCount + 1
    if frameCount == 60 then
        print("SCREENSHOT_READY")
    end
end)

print("Test ready.")
