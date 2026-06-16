-- #30 colorSpace="linear" texture opt-out verification (#2 / #67).
--
-- Two IDENTICAL mid-gray (128,128,128) RGBA textures over a white background:
--   LEFT  : plain image fill          -> format heuristic = sRGB color texture
--   RIGHT : graphics.newTexture{ colorSpace="linear" } -> opt-out, sampled linear
--
-- With sRGBAlphaBlending ON (and an sRGB backbuffer: Android Vulkan or GLES+EGL):
--   LEFT  : sample sRGB-decode(0.502)->0.216, store-encode back -> ~128  (round-trips)
--   RIGHT : sampled raw 0.502 (no decode), store-encode 0.502 -> ~188  (lighter)
-- So LEFT != RIGHT proves the opt-out actually skips the sRGB decode.
-- With the flag OFF both are ~128 (no decode, no encode) -> identical (control).
print("=== test_srgb30cs (colorSpace=linear opt-out) ===")
display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local cy = display.contentCenterY

local white = display.newRect(display.contentCenterX, cy, W, H)
white:setFillColor(1, 1, 1)

-- Register the right-hand texture as linear BEFORE it is used as a fill; the fill
-- path reuses this cached Texture by filename (Rtt_BitmapPaint FindOrCreate).
local linTex = graphics.newTexture{ type = "image", filename = "srgb_gray_lin.png",
    baseDir = system.ResourceDirectory, colorSpace = "linear" }

local left = display.newRect(W * 0.25, cy, W * 0.4, H * 0.6)
left.fill = { type = "image", filename = "srgb_gray.png", baseDir = system.ResourceDirectory }

local right = display.newRect(W * 0.75, cy, W * 0.4, H * 0.6)
right.fill = { type = "image", filename = "srgb_gray_lin.png", baseDir = system.ResourceDirectory }

local function sample(label, x, y)
    display.colorSample(x, y, function(e)
        print(string.format("SRGB30CS_SAMPLE[%s] r=%d g=%d b=%d a=%d", label,
            math.floor(e.r * 255 + 0.5), math.floor(e.g * 255 + 0.5),
            math.floor(e.b * 255 + 0.5), math.floor(e.a * 255 + 0.5)))
    end)
end

timer.performWithDelay(400, function()
    sample("left_heuristic", W * 0.25, cy)
    sample("right_linear", W * 0.75, cy)
    timer.performWithDelay(150, function() print("SCREENSHOT_READY") end)
    if linTex then linTex:releaseSelf() end
end)
