--[[
    scene_transforms.lua - Scene 4: Transforms
    
    Tests:
    - rotation
    - xScale/yScale scaling
    - alpha transparency (0.2, 0.5, 0.8, 1.0)
    - anchorX/anchorY anchor point changes
    - Combined transforms (rotation + scale + alpha)
--]]

local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 4: Transforms] Creating...")
    
    -- Scaling variables for high resolution
    local W = display.contentWidth
    local H = display.contentHeight
    local S = W / 320  -- Scaling factor
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.1, 0.1, 0.15)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 4: Transforms",
        x = 20*S,
        y = 20*S,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Helper function to create a reference rectangle
    local function createReferenceRect(x, y)
        local ref = display.newRect(sceneGroup, x, y, 40*S, 40*S)
        ref:setFillColor(0.3, 0.3, 0.3, 0.3)
        ref.strokeWidth = 1*S
        ref:setStrokeColor(0.5, 0.5, 0.5, 0.5)
        return ref
    end
    
    -- Section 1: Rotation
    print("[Scene 4: Transforms] Testing rotation...")
    local rotLabel = display.newText({
        parent = sceneGroup,
        text = "Rotation:",
        x = 20*S,
        y = 55*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    rotLabel.anchorX = 0
    rotLabel:setFillColor(0.7, 0.7, 0.7)
    
    local rotations = {0, 30, 60, 90, 135, 180}
    local xStart = 40*S
    for i, angle in ipairs(rotations) do
        local x = xStart + (i - 1) * 45*S
        createReferenceRect(x, 90*S)
        local rect = display.newRect(sceneGroup, x, 90*S, 30*S, 30*S)
        rect:setFillColor(0.9, 0.4, 0.4)
        rect.rotation = angle
        
        local label = display.newText({
            parent = sceneGroup,
            text = angle .. "°",
            x = x,
            y = 120*S,
            font = native.systemFont,
            fontSize = 10*S
        })
        label:setFillColor(0.6, 0.6, 0.6)
    end
    
    -- Section 2: Scale (xScale/yScale)
    print("[Scene 4: Transforms] Testing scale...")
    local scaleLabel = display.newText({
        parent = sceneGroup,
        text = "Scale:",
        x = 20*S,
        y = 145*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    scaleLabel.anchorX = 0
    scaleLabel:setFillColor(0.7, 0.7, 0.7)
    
    local scales = {{1, 1}, {1.5, 1}, {0.5, 1}, {1, 1.5}, {1, 0.5}, {1.5, 1.5}}
    local scaleLabels = {"1x1", "1.5x", "0.5x", "1.5y", "0.5y", "1.5xy"}
    xStart = 40*S
    for i, scale in ipairs(scales) do
        local x = xStart + (i - 1) * 50*S
        createReferenceRect(x, 180*S)
        local rect = display.newRect(sceneGroup, x, 180*S, 30*S, 30*S)
        rect:setFillColor(0.4, 0.9, 0.4)
        rect.xScale = scale[1]
        rect.yScale = scale[2]
        
        local label = display.newText({
            parent = sceneGroup,
            text = scaleLabels[i],
            x = x,
            y = 215*S,
            font = native.systemFont,
            fontSize = 10*S
        })
        label:setFillColor(0.6, 0.6, 0.6)
    end
    
    -- Section 3: Alpha transparency
    print("[Scene 4: Transforms] Testing alpha transparency...")
    local alphaLabel = display.newText({
        parent = sceneGroup,
        text = "Alpha:",
        x = 20*S,
        y = 240*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    alphaLabel.anchorX = 0
    alphaLabel:setFillColor(0.7, 0.7, 0.7)
    
    local alphas = {1.0, 0.8, 0.5, 0.3, 0.2, 0.0}
    xStart = 40*S
    for i, alpha in ipairs(alphas) do
        local x = xStart + (i - 1) * 48*S
        createReferenceRect(x, 275*S)
        local rect = display.newRect(sceneGroup, x, 275*S, 35*S, 35*S)
        rect:setFillColor(0.4, 0.4, 0.9)
        rect.alpha = alpha
        
        local label = display.newText({
            parent = sceneGroup,
            text = string.format("%.1f", alpha),
            x = x,
            y = 310*S,
            font = native.systemFont,
            fontSize = 10*S
        })
        label:setFillColor(0.6, 0.6, 0.6)
    end
    
    -- Section 4: Anchor points
    print("[Scene 4: Transforms] Testing anchor points...")
    local anchorLabel = display.newText({
        parent = sceneGroup,
        text = "Anchor Points:",
        x = 20*S,
        y = 335*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    anchorLabel.anchorX = 0
    anchorLabel:setFillColor(0.7, 0.7, 0.7)
    
    local anchors = {
        {0, 0, "0,0"},
        {0.5, 0, "0.5,0"},
        {1, 0, "1,0"},
        {0, 0.5, "0,0.5"},
        {0.5, 0.5, "center"},
        {1, 0.5, "1,0.5"}
    }
    xStart = 40*S
    for i, anchor in ipairs(anchors) do
        local x = xStart + ((i - 1) % 3) * 100*S
        local y = i <= 3 and 370*S or 425*S
        
        -- Draw cross at anchor position
        local hLine = display.newLine(sceneGroup, x - 20*S, y, x + 20*S, y)
        hLine.strokeWidth = 1*S
        hLine:setStrokeColor(0.4, 0.4, 0.4)
        local vLine = display.newLine(sceneGroup, x, y - 20*S, x, y + 20*S)
        vLine.strokeWidth = 1*S
        vLine:setStrokeColor(0.4, 0.4, 0.4)
        
        local rect = display.newRect(sceneGroup, x, y, 40*S, 40*S)
        rect:setFillColor(0.9, 0.7, 0.3)
        rect.anchorX = anchor[1]
        rect.anchorY = anchor[2]
        rect.strokeWidth = 1*S
        rect:setStrokeColor(0.3, 0.3, 0.3)
        
        -- Show anchor with small circle
        local anchorDot = display.newCircle(sceneGroup, x, y, 3*S)
        anchorDot:setFillColor(1, 0, 0)
        
        local label = display.newText({
            parent = sceneGroup,
            text = anchor[3],
            x = x,
            y = y + 35*S,
            font = native.systemFont,
            fontSize = 9*S
        })
        label:setFillColor(0.6, 0.6, 0.6)
    end
    
    -- Section 5: Combined transforms
    print("[Scene 4: Transforms] Testing combined transforms...")
    local combinedLabel = display.newText({
        parent = sceneGroup,
        text = "Combined (Rot+Scale+Alpha):",
        x = 20*S,
        y = 470*S,
        font = native.systemFont,
        fontSize = 11*S
    })
    combinedLabel.anchorX = 0
    combinedLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Original
    createReferenceRect(50*S, 510*S)
    local orig = display.newRect(sceneGroup, 50*S, 510*S, 30*S, 30*S)
    orig:setFillColor(0.5, 0.5, 0.5)
    local origLabel = display.newText({
        parent = sceneGroup,
        text = "Orig",
        x = 50*S,
        y = 540*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    origLabel:setFillColor(0.5, 0.5, 0.5)
    
    -- Rotated + Scaled
    createReferenceRect(130*S, 510*S)
    local combo1 = display.newRect(sceneGroup, 130*S, 510*S, 30*S, 30*S)
    combo1:setFillColor(0.9, 0.5, 0.3)
    combo1.rotation = 45
    combo1.xScale = 1.5
    combo1.yScale = 0.7
    local combo1Label = display.newText({
        parent = sceneGroup,
        text = "45°+Scale",
        x = 130*S,
        y = 540*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    combo1Label:setFillColor(0.6, 0.6, 0.6)
    
    -- Scaled + Alpha
    createReferenceRect(210*S, 510*S)
    local combo2 = display.newRect(sceneGroup, 210*S, 510*S, 30*S, 30*S)
    combo2:setFillColor(0.3, 0.7, 0.9)
    combo2.xScale = 1.3
    combo2.yScale = 1.3
    combo2.alpha = 0.5
    local combo2Label = display.newText({
        parent = sceneGroup,
        text = "Scale+0.5α",
        x = 210*S,
        y = 540*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    combo2Label:setFillColor(0.6, 0.6, 0.6)
    
    -- Full combo
    createReferenceRect(290*S, 510*S)
    local combo3 = display.newRect(sceneGroup, 290*S, 510*S, 30*S, 30*S)
    combo3:setFillColor(0.8, 0.4, 0.8)
    combo3.rotation = 30
    combo3.xScale = 1.2
    combo3.yScale = 0.8
    combo3.alpha = 0.6
    local combo3Label = display.newText({
        parent = sceneGroup,
        text = "All",
        x = 290*S,
        y = 540*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    combo3Label:setFillColor(0.6, 0.6, 0.6)
    
    print("[Scene 4: Transforms] Creation complete - All transform tests rendered")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 4: Transforms] Show will")
        _G.bgfxDemoCurrentScene = 4
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 4: Transforms] Show did")
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 4: Transforms] Hide will")
    elseif event.phase == "did" then
        print("[Scene 4: Transforms] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 4: Transforms] Destroy")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
