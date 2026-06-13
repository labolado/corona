-- Regression probe for composer transition + snapshot dirty semantics.
-- Usage: SOLAR2D_TEST=snapshot_scene_switch SOLAR2D_BACKEND=bgfx ...

display.setStatusBar(display.HiddenStatusBar)

local composer = require("composer")
composer.recycleOnSceneChange = true

print("=== SNAPSHOT_SCENE_SWITCH READY ===")

timer.performWithDelay(500, function()
	print("SNAPSHOT_SCENE_SWITCH goto B")
	composer.gotoScene("snapshot_switch_b", { effect = "slideLeft", time = 350 })
end)

timer.performWithDelay(1500, function()
	print("SNAPSHOT_SCENE_SWITCH capture final")
	local cap = display.capture(display.currentStage, { saveToPhotoLibrary = false })
	if cap then
		display.save(cap, {
			filename = "snapshot_scene_switch_final.png",
			baseDir = system.TemporaryDirectory
		})
		print("SNAPSHOT_SCENE_SWITCH saved=" ..
			tostring(system.pathForFile("snapshot_scene_switch_final.png", system.TemporaryDirectory)))
		display.remove(cap)
	else
		print("SNAPSHOT_SCENE_SWITCH ERROR capture failed")
	end
	print("SNAPSHOT_SCENE_SWITCH DONE")
end)

composer.gotoScene("snapshot_switch_a")
