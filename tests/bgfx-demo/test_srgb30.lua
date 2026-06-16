-- #30 sRGB-correct alpha blending verification (fixed split scene, no env needed
-- so it runs identically on Android where env vars don't reach the app).
--
-- Layout over a white background:
--   TOP half    : red rect alpha 0.5 over white  -> BLEND test (T1)
--   BOTTOM half : solid opaque gray fillColor(0.5) -> TINT round-trip test (T4)
--
-- Expected center-of-region pixels:
--                       flag OFF        flag ON (correct)
--   TOP  (blend)    ~(254,128,128)   ~(255,188,188)   <- gamma-correct blend
--   BOTTOM (tint)   ~(128,128,128)   ~(128,128,128)   <- UNCHANGED (round-trips)
--
-- If BOTTOM shifts to ~188 with the flag on, the vertex/fill-color decode (1b)
-- is missing. Values printed as SRGB30_SAMPLE for the harness; the screenshot is
-- the authoritative measurement (colorSample may read a different path).
print("=== test_srgb30 (split: blend top / tint bottom) ===")
display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local cx = display.contentCenterX

local white = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
white:setFillColor(1, 1, 1)

-- TOP: red alpha 0.5 over white
local red = display.newRect(cx, H * 0.25, W, H * 0.5)
red:setFillColor(1, 0, 0)
red.alpha = 0.5

-- BOTTOM: solid opaque mid-gray tint
local gray = display.newRect(cx, H * 0.75, W, H * 0.5)
gray:setFillColor(0.5, 0.5, 0.5)

local function sample(label, x, y)
    display.colorSample(x, y, function(e)
        print(string.format("SRGB30_SAMPLE[%s] r=%d g=%d b=%d a=%d", label,
            math.floor(e.r * 255 + 0.5), math.floor(e.g * 255 + 0.5),
            math.floor(e.b * 255 + 0.5), math.floor(e.a * 255 + 0.5)))
    end)
end

timer.performWithDelay(400, function()
    sample("blend_top", cx, H * 0.25)
    sample("tint_bottom", cx, H * 0.75)
    timer.performWithDelay(150, function() print("SCREENSHOT_READY") end)
end)
