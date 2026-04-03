--[[
    scene_blend.lua - Scene 5: Blend Modes
    
    Tests:
    - normal blend mode
    - add blend mode
    - multiply blend mode
    - screen blend mode
    - Text labels for each mode
--]]

local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 5: Blend] Creating...")
    
    -- Background (medium gray to show blend effects)
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.3, 0.3, 0.35)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 5: Blend Modes",
        x = 20,
        y = 20,
        font = native.systemFontBold,
        fontSize = 16
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Helper to create blend mode demo
    local function createBlendDemo(x, y, mode, label)
        -- Container group
        local container = display.newGroup()
        container.x, container.y = x, y
        sceneGroup:insert(container)
        
        -- Background pattern (checkered)
        for row = 0, 1 do
            for col = 0, 1 do
                local check = display.newRect(container, (col - 0.5) * 35, (row - 0.5) * 35, 35, 35)
                if (row + col) % 2 == 0 then
                    check:setFillColor(0.2, 0.2, 0.2)
                else
                    check:setFillColor(0.8, 0.8, 0.8)
                end
            end
        end
        
        -- Base shape (blue rectangle)
        local baseShape = display.newRect(container, -10, -10, 40, 40)
        baseShape:setFillColor(0.2, 0.5, 0.9)
        
        -- Overlapping shape (red circle) with blend mode
        local blendShape = display.newCircle(container, 10, 10, 25)
        blendShape:setFillColor(0.9, 0.3, 0.2)
        
        -- Apply blend mode if supported
        if blendShape.blendMode then
            blendShape.blendMode = mode
        end
        
        -- Mode label
        local modeLabel = display.newText({
            parent = sceneGroup,
            text = label,
            x = x,
            y = y + 50,
            font = native.systemFont,
            fontSize = 11
        })
        modeLabel:setFillColor(0.9, 0.9, 0.9)
        
        return container
    end
    
    -- Section 1: Normal blend
    print("[Scene 5: Blend] Testing normal blend mode...")
    local normalLabel = display.newText({
        parent = sceneGroup,
        text = "Normal (default):",
        x = 20,
        y = 55,
        font = native.systemFont,
        fontSize = 12
    })
    normalLabel.anchorX = 0
    normalLabel:setFillColor(0.7, 0.7, 0.7)
    
    createBlendDemo(70, 110, "normal", "normal")
    
    -- Section 2: Add blend
    print("[Scene 5: Blend] Testing add blend mode...")
    local addLabel = display.newText({
        parent = sceneGroup,
        text = "Add:",
        x = 170,
        y = 55,
        font = native.systemFont,
        fontSize = 12
    })
    addLabel.anchorX = 0
    addLabel:setFillColor(0.7, 0.7, 0.7)
    
    createBlendDemo(220, 110, "add", "add")
    
    -- Section 3: Multiply blend
    print("[Scene 5: Blend] Testing multiply blend mode...")
    local multiplyLabel = display.newText({
        parent = sceneGroup,
        text = "Multiply:",
        x = 20,
        y = 185,
        font = native.systemFont,
        fontSize = 12
    })
    multiplyLabel.anchorX = 0
    multiplyLabel:setFillColor(0.7, 0.7, 0.7)
    
    createBlendDemo(70, 240, "multiply", "multiply")
    
    -- Section 4: Screen blend
    print("[Scene 5: Blend] Testing screen blend mode...")
    local screenLabel = display.newText({
        parent = sceneGroup,
        text = "Screen:",
        x = 170,
        y = 185,
        font = native.systemFont,
        fontSize = 12
    })
    screenLabel.anchorX = 0
    screenLabel:setFillColor(0.7, 0.7, 0.7)
    
    createBlendDemo(220, 240, "screen", "screen")
    
    -- Section 5: Multiple overlapping circles with different blends
    print("[Scene 5: Blend] Testing complex blend scenarios...")
    local complexLabel = display.newText({
        parent = sceneGroup,
        text = "Complex blend example:",
        x = 20,
        y = 310,
        font = native.systemFont,
        fontSize = 12
    })
    complexLabel.anchorX = 0
    complexLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Dark background for complex blend
    local complexBg = display.newRect(sceneGroup, 160, 380, 280, 100)
    complexBg:setFillColor(0.1, 0.1, 0.15)
    
    -- Overlapping colored circles
    local colors = {
        {r = 1, g = 0, b = 0},    -- Red
        {r = 0, g = 1, b = 0},    -- Green
        {r = 0, g = 0, b = 1},    -- Blue
        {r = 1, g = 1, b = 0},    -- Yellow
        {r = 1, g = 0, b = 1},    -- Magenta
        {r = 0, g = 1, b = 1},    -- Cyan
    }
    
    local positions = {
        {80, 360},
        {140, 360},
        {200, 360},
        {110, 400},
        {170, 400},
        {230, 400},
    }
    
    for i, color in ipairs(colors) do
        local circle = display.newCircle(sceneGroup, positions[i][1], positions[i][2], 30)
        circle:setFillColor(color.r, color.g, color.b)
        circle.alpha = 0.6
    end
    
    -- Section 6: Alpha blending demonstration
    print("[Scene 5: Blend] Testing alpha blending...")
    local alphaLabel = display.newText({
        parent = sceneGroup,
        text = "Alpha blending:",
        x = 20,
        y = 450,
        font = native.systemFont,
        fontSize = 12
    })
    alphaLabel.anchorX = 0
    alphaLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Stack of transparent rectangles
    local stackX = 70
    local stackY = 500
    local alphas = {0.9, 0.7, 0.5, 0.3}
    local colors2 = {
        {1, 0.3, 0.3},
        {0.3, 1, 0.3},
        {0.3, 0.3, 1},
        {1, 1, 0.3},
    }
    
    for i, alpha in ipairs(alphas) do
        local rect = display.newRect(sceneGroup, stackX + (i - 1) * 35, stackY, 40, 60)
        rect:setFillColor(colors2[i][1], colors2[i][2], colors2[i][3])
        rect.alpha = alpha
        
        local aLabel = display.newText({
            parent = sceneGroup,
            text = string.format("%.0f%%", alpha * 100),
            x = stackX + (i - 1) * 35,
            y = stackY + 40,
            font = native.systemFont,
            fontSize = 9
        })
        aLabel:setFillColor(0.6, 0.6, 0.6)
    end
    
    -- Note about blend modes
    local noteLabel = display.newText({
        parent = sceneGroup,
        text = "Note: blendMode may vary by renderer",
        x = 220,
        y = 500,
        font = native.systemFont,
        fontSize = 10
    })
    noteLabel:setFillColor(0.5, 0.5, 0.5)
    
    print("[Scene 5: Blend] Creation complete - All blend mode tests rendered")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 5: Blend] Show will")
        _G.bgfxDemoCurrentScene = 5
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 5: Blend] Show did")
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 5: Blend] Hide will")
    elseif event.phase == "did" then
        print("[Scene 5: Blend] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 5: Blend] Destroy")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
