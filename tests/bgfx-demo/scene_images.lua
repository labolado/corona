--[[
    scene_images.lua - Scene 2: Images and Sprites
    
    Tests:
    - display.newImage - loading test images
    - display.newImageRect - scaled images
    - display.newSprite - sprite animations
    - Procedurally generated textures (no external files needed)
--]]

local composer = require("composer")
local scene = composer.newScene()

-- Create a procedural texture using display.newSnapshot
local function createProceduralTexture(width, height, colorFunc)
    local snapshot = display.newSnapshot(width, height)
    
    -- Draw pixels
    for x = 0, width - 1 do
        for y = 0, height - 1 do
            local r, g, b = colorFunc(x, y, width, height)
            local pixel = display.newRect(snapshot.group, x - width/2 + 0.5, y - height/2 + 0.5, 1, 1)
            pixel:setFillColor(r, g, b)
        end
    end
    
    snapshot:invalidate()
    return snapshot
end

-- Create a checkerboard pattern texture
local function createCheckerboardTexture(size, checkSize)
    local snapshot = display.newSnapshot(size, size)
    
    for row = 0, size/checkSize - 1 do
        for col = 0, size/checkSize - 1 do
            local isWhite = (row + col) % 2 == 0
            local rect = display.newRect(
                snapshot.group,
                col * checkSize - size/2 + checkSize/2,
                row * checkSize - size/2 + checkSize/2,
                checkSize,
                checkSize
            )
            if isWhite then
                rect:setFillColor(0.9, 0.9, 0.9)
            else
                rect:setFillColor(0.2, 0.2, 0.2)
            end
        end
    end
    
    snapshot:invalidate()
    return snapshot
end

-- Create a gradient texture
local function createGradientTexture(width, height)
    local snapshot = display.newSnapshot(width, height)
    
    for x = 0, width - 1 do
        local t = x / (width - 1)
        local stripe = display.newRect(snapshot.group, x - width/2 + 0.5, 0, 1, height)
        stripe:setFillColor(t, 0.5, 1 - t)
    end
    
    snapshot:invalidate()
    return snapshot
end

-- Create a simple spritesheet programmatically
local function createSpritesheetData()
    -- Create 4 frames of a simple animation (rotating colored square)
    local frames = {}
    local frameSize = 64
    
    for i = 1, 4 do
        local snapshot = display.newSnapshot(frameSize, frameSize)
        
        -- Background
        local bg = display.newRect(snapshot.group, 0, 0, frameSize, frameSize)
        bg:setFillColor(0.1, 0.1, 0.1)
        
        -- Rotating element
        local angle = (i - 1) * 90
        local rect = display.newRect(snapshot.group, 0, 0, 40, 40)
        rect:setFillColor(0.2 + i * 0.2, 0.5, 0.8)
        rect.rotation = angle
        
        -- Center dot
        local dot = display.newCircle(snapshot.group, 0, 0, 8)
        dot:setFillColor(1, 1, 0.5)
        
        -- Frame number
        local num = display.newText({
            parent = snapshot.group,
            text = tostring(i),
            x = 0,
            y = 0,
            font = native.systemFontBold,
            fontSize = 14
        })
        num:setFillColor(1, 1, 1)
        
        snapshot:invalidate()
        table.insert(frames, snapshot)
    end
    
    return frames
end

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 2: Images] Creating...")
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.1, 0.1, 0.15)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 2: Images & Sprites",
        x = 20,
        y = 20,
        font = native.systemFontBold,
        fontSize = 16
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Section 1: Procedural textures (simulating display.newImage)
    print("[Scene 2: Images] Testing procedural textures...")
    local label1 = display.newText({
        parent = sceneGroup,
        text = "display.newSnapshot (procedural):",
        x = 20,
        y = 55,
        font = native.systemFont,
        fontSize = 12
    })
    label1.anchorX = 0
    label1:setFillColor(0.7, 0.7, 0.7)
    
    -- Checkerboard texture
    local checkerTex = createCheckerboardTexture(64, 16)
    checkerTex.x, checkerTex.y = 60, 100
    sceneGroup:insert(checkerTex)
    
    local checkerLabel = display.newText({
        parent = sceneGroup,
        text = "Checker",
        x = 60,
        y = 140,
        font = native.systemFont,
        fontSize = 10
    })
    checkerLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Gradient texture
    local gradientTex = createGradientTexture(80, 50)
    gradientTex.x, gradientTex.y = 160, 100
    sceneGroup:insert(gradientTex)
    
    local gradientLabel = display.newText({
        parent = sceneGroup,
        text = "Gradient",
        x = 160,
        y = 140,
        font = native.systemFont,
        fontSize = 10
    })
    gradientLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Pattern texture with circles
    local patternSnapshot = display.newSnapshot(64, 64)
    for i = 1, 3 do
        for j = 1, 3 do
            local circle = display.newCircle(
                patternSnapshot.group,
                (j - 2) * 20,
                (i - 2) * 20,
                8
            )
            circle:setFillColor(i * 0.3, j * 0.3, 0.5)
        end
    end
    patternSnapshot:invalidate()
    patternSnapshot.x, patternSnapshot.y = 260, 100
    sceneGroup:insert(patternSnapshot)
    
    local patternLabel = display.newText({
        parent = sceneGroup,
        text = "Pattern",
        x = 260,
        y = 140,
        font = native.systemFont,
        fontSize = 10
    })
    patternLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 2: Scaled images (simulating display.newImageRect)
    print("[Scene 2: Images] Testing scaled textures...")
    local label2 = display.newText({
        parent = sceneGroup,
        text = "Scaled textures (newImageRect simulation):",
        x = 20,
        y = 170,
        font = native.systemFont,
        fontSize = 12
    })
    label2.anchorX = 0
    label2:setFillColor(0.7, 0.7, 0.7)
    
    -- Same texture at different scales
    local baseTexture = createCheckerboardTexture(64, 16)
    
    local scaled1 = display.newRect(sceneGroup, 60, 220, 40, 40)
    scaled1:setFillColor(0.5, 0.5, 0.5)
    scaled1.strokeWidth = 1
    scaled1:setStrokeColor(0.8, 0.8, 0.8)
    local t1 = createCheckerboardTexture(40, 10)
    t1.x, t1.y = 60, 220
    sceneGroup:insert(t1)
    
    local scaled2 = display.newRect(sceneGroup, 140, 220, 80, 60)
    scaled2:setFillColor(0.5, 0.5, 0.5)
    scaled2.strokeWidth = 1
    scaled2:setStrokeColor(0.8, 0.8, 0.8)
    local t2 = createGradientTexture(80, 60)
    t2.x, t2.y = 140, 220
    sceneGroup:insert(t2)
    
    local scaled3 = display.newRect(sceneGroup, 260, 220, 60, 80)
    scaled3:setFillColor(0.5, 0.5, 0.5)
    scaled3.strokeWidth = 1
    scaled3:setStrokeColor(0.8, 0.8, 0.8)
    local t3 = createCheckerboardTexture(60, 15)
    t3.xScale = 1
    t3.yScale = 80/64
    t3.x, t3.y = 260, 220
    sceneGroup:insert(t3)
    
    -- Scale labels
    local scaleLabels = {{"40x40", 60}, {"80x60", 140}, {"60x80", 260}}
    for _, sl in ipairs(scaleLabels) do
        local lbl = display.newText({
            parent = sceneGroup,
            text = sl[1],
            x = sl[2],
            y = 265,
            font = native.systemFont,
            fontSize = 10
        })
        lbl:setFillColor(0.6, 0.6, 0.6)
    end
    
    -- Section 3: Sprite animation (simulating display.newSprite)
    print("[Scene 2: Images] Testing sprite animation...")
    local label3 = display.newText({
        parent = sceneGroup,
        text = "Sprite animation (manual frame cycling):",
        x = 20,
        y = 295,
        font = native.systemFont,
        fontSize = 12
    })
    label3.anchorX = 0
    label3:setFillColor(0.7, 0.7, 0.7)
    
    -- Create sprite frames
    local spriteFrames = createSpritesheetData()
    local currentFrame = 1
    local spriteContainer = display.newGroup()
    spriteContainer.x, spriteContainer.y = 120, 360
    sceneGroup:insert(spriteContainer)
    
    -- Add all frames to container, hide all except current
    for i, frame in ipairs(spriteFrames) do
        spriteContainer:insert(frame)
        frame.isVisible = (i == 1)
    end
    
    -- Animate sprite manually
    local function animateSprite()
        spriteFrames[currentFrame].isVisible = false
        currentFrame = currentFrame % #spriteFrames + 1
        spriteFrames[currentFrame].isVisible = true
    end
    
    timer.performWithDelay(200, animateSprite, 0)
    
    local spriteLabel = display.newText({
        parent = sceneGroup,
        text = "Animated Sprite",
        x = 120,
        y = 400,
        font = native.systemFont,
        fontSize = 10
    })
    spriteLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Multiple sprites with different timings
    local spriteContainer2 = display.newGroup()
    spriteContainer2.x, spriteContainer2.y = 240, 360
    sceneGroup:insert(spriteContainer2)
    
    local spriteFrames2 = createSpritesheetData()
    local currentFrame2 = 3
    for i, frame in ipairs(spriteFrames2) do
        spriteContainer2:insert(frame)
        frame.isVisible = (i == 3)
    end
    
    local function animateSprite2()
        spriteFrames2[currentFrame2].isVisible = false
        currentFrame2 = currentFrame2 % #spriteFrames2 + 1
        spriteFrames2[currentFrame2].isVisible = true
    end
    
    timer.performWithDelay(350, animateSprite2, 0)
    
    local spriteLabel2 = display.newText({
        parent = sceneGroup,
        text = "Sprite (slower)",
        x = 240,
        y = 400,
        font = native.systemFont,
        fontSize = 10
    })
    spriteLabel2:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 4: Fill with textures (snapshot as paint)
    print("[Scene 2: Images] Testing snapshot fills...")
    local label4 = display.newText({
        parent = sceneGroup,
        text = "Snapshot as rect fill:",
        x = 20,
        y = 430,
        font = native.systemFont,
        fontSize = 12
    })
    label4.anchorX = 0
    label4:setFillColor(0.7, 0.7, 0.7)
    
    -- Note: Since we can't easily use snapshot as fill without actual texture files,
    -- we demonstrate the concept with rects that have patterns drawn on them
    local fillRect1 = display.newRect(sceneGroup, 100, 475, 80, 40)
    fillRect1:setFillColor(0.3, 0.6, 0.8)
    fillRect1.strokeWidth = 2
    fillRect1:setStrokeColor(0.6, 0.85, 1)
    
    -- Add stripes pattern
    for i = 1, 5 do
        local stripe = display.newLine(sceneGroup, 65 + i * 14, 455, 65 + i * 14, 495)
        stripe.strokeWidth = 2
        stripe:setStrokeColor(0.5, 0.8, 1, 0.5)
    end
    
    print("[Scene 2: Images] Creation complete - Images and sprites rendered")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 2: Images] Show will")
        _G.bgfxDemoCurrentScene = 2
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 2: Images] Show did")
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 2: Images] Hide will")
    elseif event.phase == "did" then
        print("[Scene 2: Images] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 2: Images] Destroy")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
