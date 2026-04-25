--[[
    test_scene.lua - Load a specific scene for verification
    Usage:
      Desktop: SOLAR2D_TEST=scene SOLAR2D_SCENE=masks ./Corona\ Simulator ...
      Android: write scene name to solar2d_scene.txt in DocumentsDirectory before launch
               (run-as <pkg> sh -c 'echo masks > files/solar2d_scene.txt')
--]]

local composer = require("composer")
display.setStatusBar(display.HiddenStatusBar)

local sceneName = os.getenv("SOLAR2D_SCENE")
if not sceneName then
    -- Android fallback: read flag file (consumed once per launch)
    local path = system.pathForFile("solar2d_scene.txt", system.DocumentsDirectory)
    if path then
        local f = io.open(path, "r")
        if f then sceneName = f:read("*l"); f:close(); os.remove(path) end
    end
end
sceneName = sceneName or "shapes"
print("=== Loading scene: " .. sceneName .. " ===")
composer.gotoScene("scene_" .. sceneName)
