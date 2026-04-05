--[[
    test_scene.lua - Load a specific scene for verification
    Usage: SOLAR2D_TEST=scene SOLAR2D_SCENE=masks ./Corona\ Simulator ...
--]]

local composer = require("composer")
display.setStatusBar(display.HiddenStatusBar)

local sceneName = os.getenv("SOLAR2D_SCENE") or "shapes"
print("=== Loading scene: " .. sceneName .. " ===")
composer.gotoScene("scene_" .. sceneName)
