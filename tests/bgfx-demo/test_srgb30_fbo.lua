-- #30 sRGB FBO colorspace verification.
-- Confirms color render targets (display.newSnapshot) round-trip through an
-- sRGB FBO consistently with direct rendering -- i.e. no #3-style double-gamma.
--
-- Layout over a white background (vertical halves):
--   TOP half    : red rect alpha 0.5 over white, rendered DIRECTLY to backbuffer
--   BOTTOM half : the SAME white+red(alpha0.5) composited inside a snapshot FBO,
--                 then displayed as a textured quad
--
-- Expected center-of-region pixels:
--                        flag OFF          flag ON (correct)
--   TOP    (direct)   ~(254,129,129)    ~(254,189,189)
--   BOTTOM (snapshot) ~(254,129,129)    ~(254,189,189)   <- MUST MATCH top
--
-- If BOTTOM diverges from TOP with the flag on, the color FBO colorspace is
-- wrong (double-gamma washout or missing sRGB on the RT). Screenshot is
-- authoritative.
print("=== test_srgb30_fbo (direct top / snapshot-FBO bottom) ===")
display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local cx, cy = display.contentCenterX, display.contentCenterY

-- Full-screen white background
local bg = display.newRect(cx, cy, W, H)
bg:setFillColor(1, 1, 1)

-- TOP: direct red alpha 0.5 over white
local red = display.newRect(cx, H * 0.25, W, H * 0.5)
red:setFillColor(1, 0, 0)
red.alpha = 0.5

-- BOTTOM: same blend composited inside a snapshot FBO
local snap = display.newSnapshot(W, H * 0.5)
snap.x, snap.y = cx, H * 0.75
do
    local sw = display.newRect(snap.group, 0, 0, W, H * 0.5)
    sw:setFillColor(1, 1, 1)
    local sr = display.newRect(snap.group, 0, 0, W, H * 0.5)
    sr:setFillColor(1, 0, 0)
    sr.alpha = 0.5
end
snap:invalidate()

local function sample(label, x, y)
    display.colorSample(x, y, function(e)
        print(string.format("SRGB30_SAMPLE[%s] r=%d g=%d b=%d a=%d", label,
            math.floor(e.r * 255 + 0.5), math.floor(e.g * 255 + 0.5),
            math.floor(e.b * 255 + 0.5), math.floor(e.a * 255 + 0.5)))
    end)
end

timer.performWithDelay(500, function()
    sample("direct_top", cx, H * 0.25)
    sample("snapshot_bottom", cx, H * 0.75)
    timer.performWithDelay(150, function() print("SCREENSHOT_READY") end)
end)
