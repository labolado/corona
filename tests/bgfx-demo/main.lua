--[[
    main.lua - Solar2D bgfx Test Demo Main Entry
    
    Comprehensive test demo for verifying bgfx rendering backend correctness.
    Uses composer for multi-scene navigation with bottom navigation bar.
--]]

local composer = require("composer")

-- Disable status bar
display.setStatusBar(display.HiddenStatusBar)

-- Enable debugging output
print("=== Solar2D bgfx Test Demo Starting ===")
print("Display size: " .. display.contentWidth .. "x" .. display.contentHeight)
print("Platform: " .. system.getInfo("platform"))
print("Environment: " .. system.getInfo("environment"))

-- Scene definitions
local scenes = {
    { name = "shapes",     label = "Shapes" },
    { name = "images",     label = "Images" },
    { name = "text",       label = "Text" },
    { name = "transforms", label = "Transform" },
    { name = "blend",      label = "Blend" },
    { name = "animation",  label = "Animate" },
    { name = "groups",     label = "Group" },
    { name = "physics",    label = "Physics" },
    { name = "masks",      label = "Mask" },
    { name = "stress",     label = "Stress" },
}

_G.bgfxDemoScenes = scenes
_G.bgfxDemoCurrentScene = 1

-- Create navigation bar
local function createNavigationBar()
    local navGroup = display.newGroup()
    
    -- Navigation bar background
    local navBg = display.newRect(navGroup, display.contentCenterX, display.contentHeight - 25, display.contentWidth, 50)
    navBg:setFillColor(0.15, 0.15, 0.15)
    navBg.strokeWidth = 1
    navBg:setStrokeColor(0.3, 0.3, 0.3)
    
    -- Scene buttons
    local buttonWidth = display.contentWidth / #scenes
    
    for i, sceneInfo in ipairs(scenes) do
        local btnX = (i - 0.5) * buttonWidth
        local btnY = display.contentHeight - 25
        
        -- Button background
        local btn = display.newRect(navGroup, btnX, btnY, buttonWidth - 2, 46)
        btn:setFillColor(0.25, 0.25, 0.25)
        btn.sceneIndex = i
        btn.sceneName = sceneInfo.name
        
        -- Button label
        local label = display.newText({
            parent = navGroup,
            text = tostring(i),
            x = btnX,
            y = btnY,
            font = native.systemFontBold,
            fontSize = 12
        })
        label:setFillColor(0.9, 0.9, 0.9)
        
        -- Touch handler
        btn:addEventListener("touch", function(event)
            if event.phase == "ended" then
                if _G.bgfxDemoCurrentScene ~= i then
                    _G.bgfxDemoCurrentScene = i
                    composer.gotoScene("scene_" .. sceneInfo.name, { effect = "slideLeft", time = 300 })
                    print("[Navigation] Switching to Scene " .. i .. ": " .. sceneInfo.name)
                end
            end
            return true
        end)
        
        -- Store reference for highlighting
        sceneInfo.button = btn
        sceneInfo.labelText = label
    end
    
    -- Highlight current scene function
    function _G.updateNavHighlight()
        for i, sceneInfo in ipairs(scenes) do
            if i == _G.bgfxDemoCurrentScene then
                sceneInfo.button:setFillColor(0.4, 0.6, 0.9)
                sceneInfo.labelText:setFillColor(1, 1, 1)
            else
                sceneInfo.button:setFillColor(0.25, 0.25, 0.25)
                sceneInfo.labelText:setFillColor(0.7, 0.7, 0.7)
            end
        end
    end
    
    _G.updateNavHighlight()
    
    return navGroup
end

-- Create navigation bar (it will stay on top)
createNavigationBar()

-- Go to first scene
print("[Main] Loading Scene 1: Shapes")
composer.gotoScene("scene_shapes")

print("=== Solar2D bgfx Test Demo Initialized ===")
