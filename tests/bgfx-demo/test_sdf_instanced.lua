display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight

display.setDefault("background", 0.04, 0.04, 0.06)

local title = display.newText("Instanced SDF test", W * 0.5, 20, native.systemFont, 16)
title:setFillColor(1, 1, 1)

local subtitle = display.newText("200 shapes target path", W * 0.5, 44, native.systemFont, 12)
subtitle:setFillColor(0.75, 0.85, 1.0)

local cols, rows = 20, 10
local cellW, cellH = W / cols, (H - 80) / rows

for i = 1, 200 do
	local col = (i - 1) % cols
	local row = math.floor((i - 1) / cols)
	local x = cellW * (col + 0.5)
	local y = 80 + cellH * (row + 0.5)
	local r = display.newRect(x, y, math.max(8, cellW * 0.48), math.max(8, cellH * 0.48))
	r:setFillColor((col % 5) / 4, 0.55 + (row % 3) * 0.12, 0.95 - (col % 4) * 0.12)
	r.rotation = (i % 8) * 9
end
