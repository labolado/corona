--[[
    test_mask_text.lua — Phase F bench (mask-PV v3).

    100 newText (kIndexedTriangles geometry, glyph quads), each in its own
    group with its own setMask matrix. Same goal as test_mask_triangles
    but exercises the kIndexedTriangles batch path.
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "bgfx"
print("=== test_mask_text ===")
print("Backend: " .. backend)

local W = display.contentWidth
local H = display.contentHeight

local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.04, 0.05, 0.08)

local status = display.newText{
    text = "test_mask_text | " .. backend,
    x = W/2, y = 14,
    font = native.systemFontBold, fontSize = 11,
}
status:setFillColor(0.95, 0.95, 0.95)

local MASK_FILE = "test_mask_circle.png"
local maskHandle = graphics.newMask(MASK_FILE)

local grid = display.newGroup()
grid.x, grid.y = 6, 36
local rows, cols = 10, 10
local cellW = math.floor((W - 12) / cols)
local cellH = math.floor((H - 80) / rows)

local labels = {}
for r = 1, rows do
    for c = 1, cols do
        local g = display.newGroup()
        g.x = (c - 1) * cellW + cellW / 2
        g.y = (r - 1) * cellH + cellH / 2
        local idx = (r - 1) * cols + c
        local txt = display.newText{
            text = string.format("%02d", idx),
            x = 0, y = 0,
            font = native.systemFontBold,
            fontSize = math.min(cellW, cellH) * 0.55,
        }
        txt:setFillColor(
            0.5 + ((r * 17 + c * 3) % 50) / 100,
            0.5 + ((r * 7  + c * 13) % 50) / 100,
            0.5 + ((r * 11 + c * 5) % 50) / 100
        )
        g:insert(txt)
        g:setMask(maskHandle)
        g.maskRotation = (idx * 7) % 360
        grid:insert(g)
        labels[idx] = g
    end
end

local frame = 0
Runtime:addEventListener("enterFrame", function()
    frame = frame + 1
    for i, g in ipairs(labels) do
        g.maskRotation = (g.maskRotation + 0.3 + (i % 5) * 0.05) % 360
    end
end)

print("=== test_mask_text ready (100 text, 100 setMask) ===")
