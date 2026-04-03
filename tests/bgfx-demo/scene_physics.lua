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
        x = 20,
        y = 20,
        font = native.systemFontBold,
        fontSize = 16
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
            fontSize = 16,
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
    local ground = display.newRect(sceneGroup, display.contentCenterX, display.contentHeight - 80, display.contentWidth - 20, 20)
    ground:setFillColor(0.4, 0.3, 0.2)
    ground.strokeWidth = 2
    ground:setStrokeColor(0.6, 0.5, 0.4)
    physics.addBody(ground, "static", {friction = 0.5, bounce = 0.2})
    ground.tag = "ground"
    
    local groundLabel = display.newText({
        parent = sceneGroup,
        text = "Static Ground",
        x = display.contentCenterX,
        y = display.contentHeight - 95,
        font = native.systemFont,
        fontSize = 10
    })
    groundLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 2: Dynamic bodies (rectangles)
    print("[Scene 8: Physics] Creating dynamic rectangle bodies...")
    local rectBodies = {}
    for i = 1, 3 do
        local x = 60 + (i - 1) * 50
        local rect = display.newRect(sceneGroup, x, 100 + i * 30, 30, 30)
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
        local x = 220 + (i - 1) * 40
        local circle = display.newCircle(sceneGroup, x, 80 + i * 25, 15)
        circle:setFillColor(0.9, 0.3 + i * 0.2, 0.3)
        physics.addBody(circle, "dynamic", {
            density = 1.0,
            friction = 0.3,
            bounce = 0.6,
            radius = 15
        })
        circle.tag = "ball"
        circle.id = i
        table.insert(circleBodies, circle)
    end
    
    -- Section 4: Polygon body (triangle)
    print("[Scene 8: Physics] Creating polygon body...")
    local triangle = display.newPolygon(sceneGroup, 80, 200, {0, -20, 17, 10, -17, 10})
    triangle:setFillColor(0.5, 0.9, 0.4)
    physics.addBody(triangle, "dynamic", {
        density = 1.0,
        friction = 0.3,
        bounce = 0.3,
        shape = {0, -20, 17, 10, -17, 10}
    })
    triangle.tag = "triangle"
    triangle.angularVelocity = 100
    
    -- Section 5: Collision callback demonstration
    print("[Scene 8: Physics] Setting up collision callbacks...")
    local collisionLabel = display.newText({
        parent = sceneGroup,
        text = "Collision Event Log:",
        x = 20,
        y = 55,
        font = native.systemFont,
        fontSize = 11
    })
    collisionLabel.anchorX = 0
    collisionLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Collision log display
    local collisionLog = {}
    local logText = display.newText({
        parent = sceneGroup,
        text = "Waiting for collisions...",
        x = 20,
        y = 75,
        width = 200,
        font = native.systemFont,
        fontSize = 9,
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
        x = 200,
        y = 270,
        font = native.systemFont,
        fontSize = 11
    })
    jointLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Two bodies connected by a pivot
    local bodyA = display.newRect(sceneGroup, 240, 300, 40, 20)
    bodyA:setFillColor(0.9, 0.6, 0.2)
    physics.addBody(bodyA, "dynamic", {density = 1.0, friction = 0.3})
    bodyA.tag = "bodyA"
    
    local bodyB = display.newRect(sceneGroup, 290, 300, 40, 20)
    bodyB:setFillColor(0.2, 0.6, 0.9)
    physics.addBody(bodyB, "dynamic", {density = 1.0, friction = 0.3})
    bodyB.tag = "bodyB"
    
    -- Create pivot joint
    if physics.newJoint then
        local pivotJoint = physics.newJoint("pivot", bodyA, bodyB, 265, 300)
        pivotJoint.isLimitEnabled = true
        pivotJoint:setRotationLimits(-45, 45)
        print("[Scene 8: Physics] Pivot joint created")
        
        -- Visual indicator for joint
        local jointVisual = display.newCircle(sceneGroup, 265, 300, 5)
        jointVisual:setFillColor(1, 1, 0)
        jointVisual.strokeWidth = 2
        jointVisual:setStrokeColor(0.8, 0.8, 0)
    end
    
    -- Apply initial forces
    for _, body in ipairs(rectBodies) do
        body:applyLinearImpulse(math.random(-10, 10) / 100, 0, body.x, body.y)
    end
    for _, body in ipairs(circleBodies) do
        body:applyLinearImpulse(math.random(-15, 15) / 100, 0, body.x, body.y)
    end
    triangle:applyLinearImpulse(0.05, 0, triangle.x, triangle.y)
    
    -- Apply angular impulse to pivot bodies
    bodyA:applyAngularImpulse(5)
    
    -- Section 7: Spawner button
    print("[Scene 8: Physics] Creating object spawner...")
    local spawnButton = display.newRoundedRect(sceneGroup, display.contentCenterX, 440, 100, 30, 5)
    spawnButton:setFillColor(0.3, 0.6, 0.9)
    spawnButton.strokeWidth = 2
    spawnButton:setStrokeColor(0.5, 0.8, 1)
    
    local spawnLabel = display.newText({
        parent = sceneGroup,
        text = "Spawn Object",
        x = display.contentCenterX,
        y = 440,
        font = native.systemFont,
        fontSize = 12
    })
    spawnLabel:setFillColor(1, 1, 1)
    
    local spawnCount = 0
    spawnButton:addEventListener("tap", function()
        spawnCount = spawnCount + 1
        local x = 50 + math.random(0, 220)
        local isCircle = math.random() > 0.5
        local obj
        
        if isCircle then
            obj = display.newCircle(sceneGroup, x, 100, 12 + math.random(0, 8))
            obj:setFillColor(math.random(), math.random(), math.random())
            physics.addBody(obj, "dynamic", {
                density = 1.0,
                friction = 0.3,
                bounce = 0.5,
                radius = obj.path.radius
            })
            obj.tag = "spawned_circle"
        else
            local size = 20 + math.random(0, 15)
            obj = display.newRect(sceneGroup, x, 100, size, size)
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
    local sensor = display.newCircle(sceneGroup, 280, 380, 25)
    sensor:setFillColor(0.9, 0.9, 0.2, 0.3)
    sensor.strokeWidth = 2
    sensor:setStrokeColor(0.9, 0.9, 0.2)
    physics.addBody(sensor, "static", {isSensor = true, radius = 25})
    sensor.tag = "sensor"
    
    local sensorLabel = display.newText({
        parent = sceneGroup,
        text = "Sensor",
        x = 280,
        y = 415,
        font = native.systemFont,
        fontSize = 10
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
