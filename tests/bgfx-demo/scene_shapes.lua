--[[
    scene_shapes.lua - Scene 1: Basic Shapes
    
    Tests:
    - display.newRect (different colors, sizes)
    - display.newCircle (different radii)
    - display.newRoundedRect (rounded corners)
    - display.newLine (different thickness, colors)
    - display.newPolygon (triangle, pentagon, star)
    - Stroke rendering (strokeWidth + strokeColor)
--]]

local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 1: Shapes] Creating...")
    
    -- 缩放系数
    local W = display.contentWidth
    local H = display.contentHeight
    local S = W / 320  -- 缩放系数
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.1, 0.1, 0.15)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 1: Basic Shapes",
        x = 20*S,
        y = 20*S,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Row 1: Rectangles with different colors and sizes
    print("[Scene 1: Shapes] Testing display.newRect...")
    local rect1 = display.newRect(sceneGroup, 50*S, 70*S, 60*S, 40*S)
    rect1:setFillColor(0.9, 0.3, 0.3)
    rect1.strokeWidth = 2*S
    rect1:setStrokeColor(1, 1, 1)
    
    local rect2 = display.newRect(sceneGroup, 130*S, 70*S, 50*S, 50*S)
    rect2:setFillColor(0.3, 0.9, 0.3)
    rect2.strokeWidth = 3*S
    rect2:setStrokeColor(0.9, 0.9, 0.3)
    
    local rect3 = display.newRect(sceneGroup, 200*S, 70*S, 70*S, 30*S)
    rect3:setFillColor(0.3, 0.3, 0.9)
    rect3.strokeWidth = 4*S
    rect3:setStrokeColor(0.9, 0.3, 0.9)
    
    -- Row 2: Circles with different radii
    print("[Scene 1: Shapes] Testing display.newCircle...")
    local circle1 = display.newCircle(sceneGroup, 50*S, 140*S, 20*S)
    circle1:setFillColor(1, 0.5, 0)
    circle1.strokeWidth = 2*S
    circle1:setStrokeColor(1, 1, 0.5)
    
    local circle2 = display.newCircle(sceneGroup, 120*S, 140*S, 30*S)
    circle2:setFillColor(0, 0.8, 0.8)
    circle2.strokeWidth = 3*S
    circle2:setStrokeColor(0.5, 1, 1)
    
    local circle3 = display.newCircle(sceneGroup, 200*S, 140*S, 15*S)
    circle3:setFillColor(0.8, 0, 0.8)
    circle3.strokeWidth = 2*S
    circle3:setStrokeColor(1, 0.5, 1)
    
    -- Row 3: Rounded rectangles
    print("[Scene 1: Shapes] Testing display.newRoundedRect...")
    local roundRect1 = display.newRoundedRect(sceneGroup, 60*S, 210*S, 80*S, 50*S, 10*S)
    roundRect1:setFillColor(0.7, 0.7, 0.2)
    roundRect1.strokeWidth = 2*S
    roundRect1:setStrokeColor(1, 1, 0.4)
    
    local roundRect2 = display.newRoundedRect(sceneGroup, 180*S, 210*S, 70*S, 60*S, 20*S)
    roundRect2:setFillColor(0.2, 0.7, 0.7)
    roundRect2.strokeWidth = 3*S
    roundRect2:setStrokeColor(0.4, 1, 1)
    
    -- Row 4: Lines with different thickness
    print("[Scene 1: Shapes] Testing display.newLine...")
    local line1 = display.newLine(sceneGroup, 30*S, 280*S, 120*S, 280*S)
    line1.strokeWidth = 2*S
    line1:setStrokeColor(1, 0.3, 0.3)
    
    local line2 = display.newLine(sceneGroup, 30*S, 295*S, 120*S, 295*S)
    line2.strokeWidth = 4*S
    line2:setStrokeColor(0.3, 1, 0.3)
    
    local line3 = display.newLine(sceneGroup, 30*S, 315*S, 120*S, 315*S)
    line3.strokeWidth = 6*S
    line3:setStrokeColor(0.3, 0.3, 1)
    
    -- Colored line segments
    local line4 = display.newLine(sceneGroup, 150*S, 280*S, 300*S, 315*S)
    line4.strokeWidth = 5*S
    line4:setStrokeColor(1, 0.8, 0.2)
    line4:append(150*S, 315*S)
    line4:setStrokeColor(0.2, 0.8, 1)
    
    -- Row 5: Polygons - Triangle (tests TriangleFan conversion!)
    print("[Scene 1: Shapes] Testing display.newPolygon (Triangle)...")
    local triangle = display.newPolygon(sceneGroup, 60*S, 370*S, {0, -30*S, 26*S, 15*S, -26*S, 15*S})
    triangle:setFillColor(0.9, 0.5, 0.2)
    triangle.strokeWidth = 2*S
    triangle:setStrokeColor(1, 0.8, 0.5)
    
    -- Pentagon
    print("[Scene 1: Shapes] Testing display.newPolygon (Pentagon)...")
    local pentagonVertices = {}
    for i = 1, 5 do
        local angle = math.rad(i * 72 - 90)
        table.insert(pentagonVertices, 30*S * math.cos(angle))
        table.insert(pentagonVertices, 30*S * math.sin(angle))
    end
    local pentagon = display.newPolygon(sceneGroup, 140*S, 370*S, pentagonVertices)
    pentagon:setFillColor(0.4, 0.6, 0.9)
    pentagon.strokeWidth = 2*S
    pentagon:setStrokeColor(0.7, 0.85, 1)
    
    -- Star (complex polygon, tests TriangleFan conversion!)
    print("[Scene 1: Shapes] Testing display.newPolygon (Star)...")
    local starVertices = {}
    for i = 1, 10 do
        local angle = math.rad(i * 36 - 90)
        local radius = (i % 2 == 1) and 35*S or 15*S
        table.insert(starVertices, radius * math.cos(angle))
        table.insert(starVertices, radius * math.sin(angle))
    end
    local star = display.newPolygon(sceneGroup, 230*S, 370*S, starVertices)
    star:setFillColor(1, 0.8, 0.2)
    star.strokeWidth = 2*S
    star:setStrokeColor(1, 0.95, 0.6)
    
    -- Labels
    local labels = {
        {text = "Rect", x = 50*S, y = 100*S},
        {text = "Circle", x = 50*S, y = 170*S},
        {text = "RoundRect", x = 60*S, y = 245*S},
        {text = "Line", x = 30*S, y = 335*S},
        {text = "Polygons", x = 60*S, y = 410*S},
    }
    
    for _, labelInfo in ipairs(labels) do
        local label = display.newText({
            parent = sceneGroup,
            text = labelInfo.text,
            x = labelInfo.x,
            y = labelInfo.y,
            font = native.systemFont,
            fontSize = 10*S
        })
        label.anchorX = 0
        label:setFillColor(0.7, 0.7, 0.7)
    end
    
    print("[Scene 1: Shapes] Creation complete - All basic shapes rendered")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 1: Shapes] Show will - preparing to display")
        _G.bgfxDemoCurrentScene = 1
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 1: Shapes] Show did - now displayed")
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 1: Shapes] Hide will - preparing to hide")
    elseif event.phase == "did" then
        print("[Scene 1: Shapes] Hide did - now hidden")
    end
end

function scene:destroy(event)
    print("[Scene 1: Shapes] Destroy - cleaning up")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
