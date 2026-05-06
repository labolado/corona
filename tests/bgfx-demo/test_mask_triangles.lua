--[[
    test_mask_triangles.lua — Phase F bench (mask-PV v3).

    100 newCircle (kTriangles geometry), each in its own group with its own
    setMask matrix. Targets the per-vertex mask UV path: same maskCount
    (1) across all draws, varied mat3 → BakeMaskUVs writes per-vertex
    UV; CanBatchDraws should now allow merging across mat3 differences.

    Baseline (pre-PV) success ~10%. Target 20-30% (gemini revised).
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "bgfx"
print("=== test_mask_triangles ===")
print("Backend: " .. backend)

local W = display.contentWidth
local H = display.contentHeight

local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.04, 0.05, 0.08)

local status = display.newText{
    text = "test_mask_triangles | " .. backend,
    x = W/2, y = 14,
    font = native.systemFontBold, fontSize = 11,
}
status:setFillColor(0.95, 0.95, 0.95)

local MASK_FILE = "test_mask_circle.png"
local maskHandle = graphics.newMask(MASK_FILE)

-- 10 x 10 grid of independently-masked newCircle (kTriangles geometry).
local grid = display.newGroup()
grid.x, grid.y = 6, 36
local rows, cols = 10, 10
local cellW = math.floor((W - 12) / cols)
local cellH = math.floor((H - 80) / rows)

local circles = {}
for r = 1, rows do
    for c = 1, cols do
        local g = display.newGroup()
        g.x = (c - 1) * cellW + cellW / 2
        g.y = (r - 1) * cellH + cellH / 2
        local idx = (r - 1) * cols + c
        local circ = display.newCircle(g, 0, 0, math.min(cellW, cellH) * 0.4)
        circ:setFillColor(
            0.4 + ((r * 17 + c * 3) % 60) / 100,
            0.4 + ((r * 7  + c * 13) % 60) / 100,
            0.4 + ((r * 11 + c * 5) % 60) / 100
        )
        g:setMask(maskHandle)
        -- Per-instance mask matrix variation (rotation + slight scale).
        g.maskRotation = (idx * 7) % 360
        g.maskScaleX = 0.8 + (idx % 5) * 0.05
        g.maskScaleY = 0.8 + ((idx * 3) % 5) * 0.05
        grid:insert(g)
        circles[idx] = g
    end
end

-- Slow rotation/translation per frame so mat3 actually changes frame-to-frame.
local frame = 0
Runtime:addEventListener("enterFrame", function()
    frame = frame + 1
    for i, g in ipairs(circles) do
        g.maskRotation = (g.maskRotation + 0.3 + (i % 5) * 0.05) % 360
    end
end)

-- BatchStats are auto-dumped by Rtt_BgfxCommandBuffer::DumpBatchStats every 300 frames.
print("=== test_mask_triangles ready (100 circles, 100 setMask) ===")
