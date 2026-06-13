local composer = require("composer")
local scene = composer.newScene()

local CX, CY = display.contentCenterX, display.contentCenterY

function scene:create()
	local g = self.view
	local bg = display.newRect(g, CX, CY, display.contentWidth, display.contentHeight)
	bg:setFillColor(0.02, 0.02, 0.02)

	local snap = display.newSnapshot(g, 180, 180)
	snap.x, snap.y = CX, CY
	local fill = display.newRect(snap.group, 0, 0, 180, 180)
	fill:setFillColor(0, 1, 0)
	local marker = display.newText({
		parent = snap.group,
		text = "B",
		x = 0,
		y = 0,
		font = native.systemFontBold,
		fontSize = 72
	})
	marker:setFillColor(0, 0, 0)
	snap:invalidate()
	print("SNAPSHOT_SWITCH_B created green snapshot")
end

scene:addEventListener("create", scene)
return scene
