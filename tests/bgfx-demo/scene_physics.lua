--[[
    scene_physics.lua - Scene 8: Physics
    
    Tests:
    - physics.start()
    - Dynamic bodies falling
    - Static ground
    - Collision callbacks
    - Different shapes (rect, circle, polygon)
    - Joints (pivot) connecting bodies
--]]

local composer = require("composer")
local scene = composer.newScene()

-- Physics variables
local physics = nil
local physicsStarted = false

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 8: Physics] Creating...")
    
    -- Scaling variables for high resolution
    local W = display.contentWidth
    local H = display.contentHeight
    local S = W / 320  -- Scaling factor
    
    -- Try to load physics
    local success, result = pcall(function()
        return require("physics")
    end)
    
    if success then
        physics = result
        print("[Scene 8: Physics] Physics module loaded successfully")
    else
        print("[Scene 8: Physics] WARNING: Physics module not available - " .. tostring(result))
    end
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.1, 0.1, 0.15)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 8: Physics",
        x = 20*S,
        y = 20*S,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    if not physics then
        -- Physics not available
        local warning = display.newText({
            parent = sceneGroup,
            text = "Physics module not available\nin this build",
            x = display.contentCenterX,
            y = display.contentCenterY,
            font = native.systemFont,
            fontSize = 16*S,
            align = "center"
        })
        warning:setFillColor(0.9, 0.5, 0.3)
        print("[Scene 8: Physics] Creation complete - Physics not available")
        return
    end
    
    -- Start physics
    physics.start()
    physics.setGravity(0, 9.8)
    physics.setScale(30) -- Pixels to meters ratio
    physicsStarted = true
    print("[Scene 8: Physics] Physics engine started")
    
    -- Section 1: Static ground
    print("[Scene 8: Physics] Creating static ground...")
    local ground = display.newRect(sceneGroup, display.contentCenterX, display.contentHeight - 80*S, display.contentWidth - 20*S, 20*S)
    ground:setFillColor(0.4, 0.3, 0.2)
    ground.strokeWidth = 2*S
    ground:setStrokeColor(0.6, 0.5, 0.4)
    physics.addBody(ground, "static", {friction = 0.5, bounce = 0.2})
    ground.tag = "ground"
    
    local groundLabel = display.newText({
        parent = sceneGroup,
        text = "Static Ground",
        x = display.contentCenterX,
        y = display.contentHeight - 95*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    groundLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 2: Dynamic bodies (rectangles)
    print("[Scene 8: Physics] Creating dynamic rectangle bodies...")
    local rectBodies = {}
    for i = 1, 3 do
        local x = 60*S + (i - 1) * 50*S
        local rect = display.newRect(sceneGroup, x, 100*S + i * 30*S, 30*S, 30*S)
        rect:setFillColor(0.2 + i * 0.25, 0.5, 0.8 - i * 0.1)
        physics.addBody(rect, "dynamic", {
            density = 1.0,
            friction = 0.3,
            bounce = 0.4
        })
        rect.tag = "box"
        rect.id = i
        table.insert(rectBodies, rect)
    end
    
    -- Section 3: Dynamic circle bodies
    print("[Scene 8: Physics] Creating dynamic circle bodies...")
    local circleBodies = {}
    for i = 1, 3 do
        local x = 220*S + (i - 1) * 40*S
        local circle = display.newCircle(sceneGroup, x, 80*S + i * 25*S, 15*S)
        circle:setFillColor(0.9, 0.3 + i * 0.2, 0.3)
        physics.addBody(circle, "dynamic", {
            density = 1.0,
            friction = 0.3,
            bounce = 0.6,
            radius = 15*S
        })
        circle.tag = "ball"
        circle.id = i
        table.insert(circleBodies, circle)
    end
    
    -- Section 4: Polygon body (triangle)
    print("[Scene 8: Physics] Creating polygon body...")
    local triangle = display.newPolygon(sceneGroup, 80*S, 200*S, {0, -20*S, 17*S, 10*S, -17*S, 10*S})
    triangle:setFillColor(0.5, 0.9, 0.4)
    physics.addBody(triangle, "dynamic", {
        density = 1.0,
        friction = 0.3,
        bounce = 0.3,
        Shape = {0, -20*S, 17*S, 10*S, -17*S, 10*S}
    })
    triangle.tag = "triangle"
    triangle.angularVelocity = 100
    
    -- Section 5: Collision callback demonstration
    print("[Scene 8: Physics] Setting up collision callbacks...")
    local collisionLabel = display.newText({
        parent = sceneGroup,
        text = "Collision Event Log:",
        x = 20*S,
        y = 55*S,
        font = native.systemFont,
        fontSize = 11*S
    })
    collisionLabel.anchorX = 0
    collisionLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Collision log display
    local collisionLog = {}
    local logText = display.newText({
        parent = sceneGroup,
        text = "Waiting for collisions...",
        x = 20*S,
        y = 75*S,
        width = 200*S,
        font = native.systemFont,
        fontSize = 9*S,
        align = "left"
    })
    logText.anchorX = 0
    logText:setFillColor(0.8, 0.8, 0.6)
    
    local function logCollision(msg)
        table.insert(collisionLog, 1, msg)
        if #collisionLog > 3 then
            table.remove(collisionLog)
        end
        logText.text = table.concat(collisionLog, "\n")
    end
    
    -- Global collision listener
    local function onCollision(event)
        if event.phase == "began" then
            local obj1 = event.object1
            local obj2 = event.object2
            local name1 = obj1.tag or "unknown"
            local name2 = obj2.tag or "unknown"
            print("[Scene 8: Physics] Collision: " .. name1 .. " hit " .. name2)
            logCollision(name1 .. " ⟷ " .. name2)
        end
    end
    
    Runtime:addEventListener("collision", onCollision)
    self.collisionListener = onCollision
    
    -- Section 6: Joint (pivot/hinge)
    print("[Scene 8: Physics] Creating pivot joint...")
    local jointLabel = display.newText({
        parent = sceneGroup,
        text = "Pivot Joint:",
        x = 200*S,
        y = 270*S,
        font = native.systemFont,
        fontSize = 11*S
    })
    jointLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Two bodies connected by a pivot
    local bodyA = display.newRect(sceneGroup, 240*S, 300*S, 40*S, 20*S)
    bodyA:setFillColor(0.9, 0.6, 0.2)
    physics.addBody(bodyA, "dynamic", {density = 1.0, friction = 0.3})
    bodyA.tag = "bodyA"
    
    local bodyB = display.newRect(sceneGroup, 290*S, 300*S, 40*S, 20*S)
    bodyB:setFillColor(0.2, 0.6, 0.9)
    physics.addBody(bodyB, "dynamic", {density = 1.0, friction = 0.3})
    bodyB.tag = "bodyB"
    
    -- Create pivot joint
    if physics.newJoint then
        local pivotJoint = physics.newJoint("pivot", bodyA, bodyB, 265*S, 300*S)
        pivotJoint.isLimitEnabled = true
        pivotJoint:setRotationLimits(-45, 45)
        print("[Scene 8: Physics] Pivot joint created")
        
        -- Visual indicator for joint
        local jointVisual = display.newCircle(sceneGroup, 265*S, 300*S, 5*S)
        jointVisual:setFillColor(1, 1, 0)
        jointVisual.strokeWidth = 2*S
        jointVisual:setStrokeColor(0.8, 0.8, 0)
    end
    
    -- Apply initial forces
    for _, body in ipairs(rectBodies) do
        body:applyLinearImpulse(math.random(-10*S, 10*S) / 100, 0, body.x, body.y)
    end
    for _, body in ipairs(circleBodies) do
        body:applyLinearImpulse(math.random(-15*S, 15*S) / 100, 0, body.x, body.y)
    end
    triangle:applyLinearImpulse(0.05*S, 0, triangle.x, triangle.y)
    
    -- Apply angular impulse to pivot bodies
    bodyA:applyAngularImpulse(5)
    
    -- Section 7: Spawner button
    print("[Scene 8: Physics] Creating object spawner...")
    local spawnButton = display.newRoundedRect(sceneGroup, display.contentCenterX, 440*S, 100*S, 30*S, 5*S)
    spawnButton:setFillColor(0.3, 0.6, 0.9)
    spawnButton.strokeWidth = 2*S
    spawnButton:setStrokeColor(0.5, 0.8, 1)
    
    local spawnLabel = display.newText({
        parent = sceneGroup,
        text = "Spawn Object",
        x = display.contentCenterX,
        y = 440*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    spawnLabel:setFillColor(1, 1, 1)
    
    local spawnCount = 0
    spawnButton:addEventListener("tap", function()
        spawnCount = spawnCount + 1
        local x = 50*S + math.random(0, 220*S)
        local isCircle = math.random() > 0.5
        local obj
        
        if isCircle then
            obj = display.newCircle(sceneGroup, x, 100*S, (12 + math.random(0, 8))*S)
            obj:setFillColor(math.random(), math.random(), math.random())
            physics.addBody(obj, "dynamic", {
                density = 1.0,
                friction = 0.3,
                bounce = 0.5,
                radius = obj.path.radius
            })
            obj.tag = "spawned_circle"
        else
            local size = (20 + math.random(0, 15))*S
            obj = display.newRect(sceneGroup, x, 100*S, size, size)
            obj:setFillColor(math.random(), math.random(), math.random())
            physics.addBody(obj, "dynamic", {
                density = 1.0,
                friction = 0.3,
                bounce = 0.3
            })
            obj.tag = "spawned_box"
        end
        obj.id = "spawned_" .. spawnCount
        print("[Scene 8: Physics] Spawned object #" .. spawnCount)
    end)
    
    -- Section 8: Sensor body
    print("[Scene 8: Physics] Creating sensor body...")
    local sensor = display.newCircle(sceneGroup, 280*S, 380*S, 25*S)
    sensor:setFillColor(0.9, 0.9, 0.2, 0.3)
    sensor.strokeWidth = 2*S
    sensor:setStrokeColor(0.9, 0.9, 0.2)
    physics.addBody(sensor, "static", {isSensor = true, radius = 25*S})
    sensor.tag = "sensor"
    
    local sensorLabel = display.newText({
        parent = sceneGroup,
        text = "Sensor",
        x = 280*S,
        y = 415*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    sensorLabel:setFillColor(0.8, 0.8, 0.4)
    
    print("[Scene 8: Physics] Creation complete - Physics simulation running")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 8: Physics] Show will")
        _G.bgfxDemoCurrentScene = 8
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 8: Physics] Show did")
        if physics and physics.start then
            physics.start()
        end
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 8: Physics] Hide will - pausing physics")
        if physics and physics.pause then
            physics.pause()
        end
    elseif event.phase == "did" then
        print("[Scene 8: Physics] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 8: Physics] Destroy - stopping physics")
    if self.collisionListener then
        Runtime:removeEventListener("collision", self.collisionListener)
        self.collisionListener = nil
    end
    if physicsStarted and physics and physics.stop then
        physics.stop()
        physicsStarted = false
    end
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
