-- #30 sRGB-correct alpha blending verification.
--   SRGB30_MODE=t1 (default): red rect alpha 0.5 over white; colorSample center.
--       flag OFF -> ~(254,128,128) [linear-on-sRGB blend]
--       flag ON  -> ~(255,188,188) [gamma-correct blend]  <-- target
--   SRGB30_MODE=t2: an UNtinted textured sprite; colorSample a known texel.
--       Round-trip check: ON and OFF should look the SAME (sprite unchanged),
--       because the texture is sRGB-encoded and decodes/encodes symmetrically.
--   SRGB30_MODE=t3: snapshot (red inner) + luminance mask; colorSample center.
--
-- The opt-in flag is read from config.lua application.sRGBAlphaBlending.
-- Values are printed as SRGB30_SAMPLE for the harness to parse.
print("=== test_srgb30 ===")
display.setStatusBar(display.HiddenStatusBar)
local cx, cy = display.contentCenterX, display.contentCenterY
local mode = os.getenv("SRGB30_MODE") or "t1"
print("SRGB30_MODE=" .. mode)

local function sampleCenter(label)
    timer.performWithDelay(350, function()
        display.colorSample(cx, cy, function(e)
            print(string.format("SRGB30_SAMPLE[%s] r=%d g=%d b=%d a=%d",
                label,
                math.floor(e.r * 255 + 0.5),
                math.floor(e.g * 255 + 0.5),
                math.floor(e.b * 255 + 0.5),
                math.floor(e.a * 255 + 0.5)))
            print("SCREENSHOT_READY")
        end)
    end)
end

if mode == "t1" then
    local white = display.newRect(cx, cy, display.contentWidth, display.contentHeight)
    white:setFillColor(1, 1, 1)
    local red = display.newRect(cx, cy, display.contentWidth, display.contentHeight)
    red:setFillColor(1, 0, 0)
    red.alpha = 0.5
    sampleCenter("t1")

elseif mode == "t2" then
    -- Untinted sprite: a solid mid-gray PNG would be ideal, but to stay
    -- asset-free we synthesize a gray via a snapshot rendered with default
    -- (white) tint so the texture path is exercised without a tint shift.
    local bg = display.newRect(cx, cy, display.contentWidth, display.contentHeight)
    bg:setFillColor(0, 0, 0)
    local snap = display.newSnapshot(200, 200)
    snap.x, snap.y = cx, cy
    local g = display.newRect(snap.group, 0, 0, 200, 200)
    g:setFillColor(0.5, 0.5, 0.5) -- mid gray inside the snapshot (color FBO)
    snap:invalidate()
    sampleCenter("t2")

elseif mode == "t3" then
    local bg = display.newRect(cx, cy, display.contentWidth, display.contentHeight)
    bg:setFillColor(0, 0, 0)
    local snap = display.newSnapshot(200, 200)
    snap.x, snap.y = cx, cy
    local inner = display.newRect(snap.group, 0, 0, 200, 200)
    inner:setFillColor(1, 0, 0)
    inner.alpha = 0.5
    snap:invalidate()
    sampleCenter("t3")
end
