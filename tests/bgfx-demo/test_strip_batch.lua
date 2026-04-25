--[[
    test_strip_batch.lua - Pure strip batching test (deterministic)

    Creates 1000 newRect with no fill image, no mask, same blend mode.
    All objects share default white texture and default shader.
    Ideal scenario for upstream Renderer batching (TriangleStrip + same state).

    Static layout (no animation) — A/B byte-identical comparison.
    Prints SCREENSHOT_READY at frame 60 (~1 sec).

    Usage: SOLAR2D_TEST=strip_batch SOLAR2D_BACKEND=bgfx ./Corona\ Simulator
    A/B   : SOLAR2D_STRIP_BATCH=0 to disable bgfx-layer strip merge
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local stripEnv = os.getenv("SOLAR2D_STRIP_BATCH")
print("=== Strip Batch Test ===")
print("Backend: " .. backend)
print("StripBatch: " .. (stripEnv == "0" and "DISABLED" or "ENABLED"))

-- Deterministic seed
math.randomseed(42)

local W = display.contentWidth
local H = display.contentHeight

-- Black background (1 strip cmd)
local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0, 0, 0)

-- 1000 colored rects (1000 strip inserts; ideally batch into 1-3 draws)
local NUM = 1000
local SIZE = 6
print("Creating " .. NUM .. " rects...")
for i = 1, NUM do
    local x = (i % 50) * 12 + 10
    local y = math.floor(i / 50) * 12 + 60
    local r = display.newRect(x, y, SIZE, SIZE)
    r:setFillColor(math.random(), math.random(), math.random(), 0.8)
end

-- Screenshot signal at frame 60
local frameCount = 0
local function onEnterFrame()
    frameCount = frameCount + 1
    if frameCount == 60 then
        print("SCREENSHOT_READY")
    end
end
Runtime:addEventListener("enterFrame", onEnterFrame)

print("Test ready. Watch [BatchStats] every 5s.")
