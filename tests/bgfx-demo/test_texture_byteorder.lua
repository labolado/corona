-- test_texture_byteorder.lua - Texture RGBA byte order validation
-- Root cause: tank camouflage R↔B swap when texture format mismatches
-- This test places pure R/G/B images side by side for visual inspection

display.setDefault("background", 0.1, 0.1, 0.1)

local W, H = display.contentWidth, display.contentHeight

local function label(text, x, y)
    local t = display.newText(text, x, y, native.systemFont, 11)
    t:setFillColor(1, 1, 1)
    return t
end

label("Texture Byte Order Test", W/2, 15)
label("Red | Green | Blue | Magenta | Cyan | Yellow", W/2, 30)

local boxW = math.min(80, (W - 40) / 6)
local startX = (W - boxW * 6) / 2 + boxW / 2
local y1 = H * 0.35
local y2 = H * 0.75

-- Row 1: Pure color squares loaded from PNG files
-- If R↔B byte order is wrong, "red" will appear blue, "blue" will appear red
local colors = {
    {name="red",    file="test_red.png",    r=1, g=0, b=0},
    {name="green",  file="test_green.png",  r=0, g=1, b=0},
    {name="blue",   file="test_blue.png",   r=0, g=0, b=1},
    {name="magenta",file="test_magenta.png",r=1, g=0, b=1},
    {name="cyan",   file="test_cyan.png",   r=0, g=1, b=1},
    {name="yellow", file="test_yellow.png", r=1, g=1, b=0},
}

for i, c in ipairs(colors) do
    local x = startX + (i - 1) * boxW

    -- Image loaded from file
    local img = display.newImageRect(c.file, boxW - 4, boxW - 4)
    img.x, img.y = x, y1

    -- Label below
    label(c.name, x, y1 + boxW/2 + 12)

    -- Solid rect with expected color for comparison (right half of screen concept)
    local rect = display.newRect(boxW - 4, boxW - 4)
    rect:setFillColor(c.r, c.g, c.b)
    rect.x, rect.y = x, y2

    if i == 1 then
        label("PNG loaded", x, y1 - boxW/2 - 12)
        label("Expected", x, y2 - boxW/2 - 12)
    end
end

-- Row 2 label
label("If byte order is wrong, top row colors will NOT match bottom row", W/2, H - 15)

local backend = os.getenv("SOLAR2D_BACKEND") or "unknown"
label("Backend: " .. backend, W/2, H - 30)

print("=== TEXTURE BYTEORDER TEST: R G B Magenta Cyan Yellow ===")
print("If R↔B swap occurs, red↔blue and magenta↔cyan will mismatch")
