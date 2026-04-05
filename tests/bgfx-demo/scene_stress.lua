--[[
    scene_stress.lua - Scene 10: Stress Test
    
    Tests:
    - 500 moving rectangles
    - Per-frame position updates
    - FPS counter display
    - Performance comparison baseline
--]]

local composer = require("composer")
local scene = composer.newScene()

-- Scaling variables for high resolution
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320  -- Scale factor

-- Performance monitoring
local fpsText = nil
local objectCountText = nil
local frameCount = 0
local lastTime = 0
local fps = 60
local objects = {}
local updateTimer = nil
local fpsTimer = nil
local objectsGroup = nil

-- Configuration
local NUM_OBJECTS = 500
local OBJECT_SIZE = 8 * S

-- Per-frame update function
local function updateObjects()
    local bounds = {
        left = 5*S,
        right = display.contentWidth - 5*S,
        top = 5*S,
        bottom = display.contentHeight - 135*S
    }
    
    for i = 1, #objects do
        local obj = objects[i]
        
        -- Update position
        obj.x = obj.x + obj.vx
        obj.y = obj.y + obj.vy
        obj.rotation = obj.rotation + obj.rotationSpeed
        
        -- Bounce off walls
        if obj.x < bounds.left or obj.x > bounds.right then
            obj.vx = -obj.vx
            obj.x = math.max(bounds.left, math.min(bounds.right, obj.x))
        end
        if obj.y < bounds.top or obj.y > bounds.bottom then
            obj.vy = -obj.vy
            obj.y = math.max(bounds.top, math.min(bounds.bottom, obj.y))
        end
    end
    
    frameCount = frameCount + 1
end

-- FPS update function
local function updateFPS()
    local currentTime = system.getTimer()
    local deltaTime = currentTime - lastTime
    
    if deltaTime > 0 then
        fps = frameCount / (deltaTime / 1000)
        frameCount = 0
        lastTime = currentTime
        
        -- Update FPS display
        if fpsText then
            fpsText.text = string.format("FPS: %.1f", fps)
            
            -- Color code FPS
            if fps >= 55 then
                fpsText:setFillColor(0.3, 1, 0.3)  -- Green
            elseif fps >= 30 then
                fpsText:setFillColor(1, 1, 0.3)    -- Yellow
            else
                fpsText:setFillColor(1, 0.3, 0.3)  -- Red
            end
        end
        
        print("[Scene 10: Stress] FPS: " .. string.format("%.1f", fps))
    end
end

-- Runtime enter frame listener for updates
local function onEnterFrame(event)
    updateObjects()
end

-- Function to start animations (called from create and show)
local function startAnimations()
    print("[Scene 10: Stress] Starting animations...")
    
    -- Cancel any existing timers/listeners first
    if fpsTimer then
        timer.cancel(fpsTimer)
        fpsTimer = nil
    end
    
    -- Reset frame count and time
    frameCount = 0
    lastTime = system.getTimer()
    
    -- Reset object positions to random positions within bounds
    local bounds = {
        left = 10*S,
        right = display.contentWidth - 10*S,
        top = 10*S,
        bottom = display.contentHeight - 140*S
    }
    for i = 1, #objects do
        local obj = objects[i]
        obj.x = math.random(bounds.left, bounds.right)
        obj.y = math.random(bounds.top, bounds.bottom)
        obj.rotation = 0
    end
    
    -- Restart FPS timer (update every 500ms)
    fpsTimer = timer.performWithDelay(500, updateFPS, 0)
    
    print("[Scene 10: Stress] Animation loop started")
end

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 10: Stress] Creating...")
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.05, 0.05, 0.08)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 10: Stress Test",
        x = 20*S,
        y = 20*S,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Info text
    local infoText = display.newText({
        parent = sceneGroup,
        text = NUM_OBJECTS .. " objects updating per frame",
        x = 20*S,
        y = 45*S,
        font = native.systemFont,
        fontSize = 11*S
    })
    infoText.anchorX = 0
    infoText:setFillColor(0.7, 0.7, 0.7)
    
    -- FPS counter background
    local fpsBg = display.newRect(sceneGroup, display.contentWidth - 60*S, 35*S, 100*S, 50*S)
    fpsBg:setFillColor(0.15, 0.15, 0.2)
    fpsBg.strokeWidth = 2*S
    fpsBg:setStrokeColor(0.3, 0.3, 0.4)
    
    -- FPS text
    fpsText = display.newText({
        parent = sceneGroup,
        text = "FPS: 60",
        x = display.contentWidth - 60*S,
        y = 30*S,
        font = native.systemFontBold,
        fontSize = 18*S
    })
    fpsText:setFillColor(0.3, 1, 0.3)
    
    -- Object count text
    objectCountText = display.newText({
        parent = sceneGroup,
        text = "Objects: " .. NUM_OBJECTS,
        x = display.contentWidth - 60*S,
        y = 50*S,
        font = native.systemFont,
        fontSize = 11*S
    })
    objectCountText:setFillColor(0.8, 0.8, 0.8)
    
    -- Create objects container group
    objectsGroup = display.newGroup()
    objectsGroup.y = 75*S  -- Offset to account for UI
    sceneGroup:insert(objectsGroup)
    
    -- Create 500 moving objects
    print("[Scene 10: Stress] Creating " .. NUM_OBJECTS .. " objects...")
    local startTime = system.getTimer()
    
    for i = 1, NUM_OBJECTS do
        local x = math.random(10*S, display.contentWidth - 10*S)
        local y = math.random(10*S, display.contentHeight - 140*S)
        
        local rect = display.newRect(objectsGroup, x, y, OBJECT_SIZE, OBJECT_SIZE)
        
        -- Vary colors based on position/index
        local hue = (i / NUM_OBJECTS) * 0.8 + 0.1
        rect:setFillColor(hue, 0.6 + math.random() * 0.4, 0.8)
        
        -- Store velocity and other properties
        rect.vx = (math.random() - 0.5) * 4
        rect.vy = (math.random() - 0.5) * 4
        rect.rotationSpeed = (math.random() - 0.5) * 10
        
        table.insert(objects, rect)
    end
    
    local creationTime = system.getTimer() - startTime
    print("[Scene 10: Stress] Created " .. NUM_OBJECTS .. " objects in " .. creationTime .. "ms")
    
    -- Stats text
    local statsText = display.newText({
        parent = sceneGroup,
        text = "Creation: " .. creationTime .. "ms",
        x = 20*S,
        y = display.contentHeight - 100*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    statsText.anchorX = 0
    statsText:setFillColor(0.6, 0.6, 0.6)
    
    -- Performance comparison note
    local noteText = display.newText({
        parent = sceneGroup,
        text = "Compare GL vs bgfx performance\nLower is better for same FPS",
        x = display.contentCenterX,
        y = display.contentHeight - 70*S,
        font = native.systemFont,
        fontSize = 10*S,
        align = "center"
    })
    noteText:setFillColor(0.5, 0.5, 0.5)
    
    -- Start animations
    startAnimations()
    
    -- Runtime enter frame listener for updates (only added once in create)
    Runtime:addEventListener("enterFrame", onEnterFrame)
    self.enterFrameListener = onEnterFrame
    
    print("[Scene 10: Stress] Animation loop started")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 10: Stress] Show will")
        _G.bgfxDemoCurrentScene = 10
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 10: Stress] Show did")
        -- Restart animations when scene is shown (for re-entry)
        startAnimations()
        -- Re-add enter frame listener if needed
        if not self.enterFrameListener then
            Runtime:addEventListener("enterFrame", onEnterFrame)
            self.enterFrameListener = onEnterFrame
        end
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 10: Stress] Hide will - pausing stress test")
        -- Pause updates but don't destroy
        if self.enterFrameListener then
            Runtime:removeEventListener("enterFrame", self.enterFrameListener)
            self.enterFrameListener = nil
        end
        if fpsTimer then
            timer.cancel(fpsTimer)
            fpsTimer = nil
        end
    elseif event.phase == "did" then
        print("[Scene 10: Stress] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 10: Stress] Destroy - cleaning up " .. #objects .. " objects")
    
    -- Remove enter frame listener
    if self.enterFrameListener then
        Runtime:removeEventListener("enterFrame", self.enterFrameListener)
        self.enterFrameListener = nil
    end
    
    -- Cancel timers
    if fpsTimer then
        timer.cancel(fpsTimer)
        fpsTimer = nil
    end
    if updateTimer then
        timer.cancel(updateTimer)
        updateTimer = nil
    end
    
    -- Clear objects table (actual display objects will be garbage collected with scene)
    objects = {}
    
    print("[Scene 10: Stress] Cleanup complete")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
