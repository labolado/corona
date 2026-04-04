--[[
    main.lua - Scene Transition Test
    
    Test 1: Direct launch scene_images
    Test 2: Switch from scene_shapes to scene_images after 3 seconds
--]]

local composer = require("composer")

-- Disable status bar
display.setStatusBar(display.HiddenStatusBar)

print("=== Solar2D bgfx Test Demo Starting ===")
print("Display size: " .. display.contentWidth .. "x" .. display.contentHeight)

-- Check if we should test direct launch or scene switch
local testDirectLaunch = false  -- Set to true to test direct scene_images launch

if testDirectLaunch then
    print("[Test] Direct launch scene_images")
    composer.gotoScene("scene_images")
else
    print("[Test] Launch scene_shapes, then switch to scene_images after 3 seconds")
    composer.gotoScene("scene_shapes")
    
    timer.performWithDelay(3000, function()
        print("[Test] === SWITCHING TO SCENE IMAGES ===")
        _G.bgfxDemoCurrentScene = 2
        composer.gotoScene("scene_images", { effect = "slideLeft", time = 300 })
    end)
end

print("=== Test Initialized ===")
