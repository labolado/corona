--[[
    scene_text.lua - Scene 3: Text Rendering
    
    Tests:
    - display.newText - different font sizes
    - Different colors
    - Multi-line text
    - Different alignments
--]]

local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sceneGroup = self.view
    
    local W = display.contentWidth
    local H = display.contentHeight
    local S = W / 320  -- 缩放系数
    
    print("[Scene 3: Text] Creating...")
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.1, 0.1, 0.15)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 3: Text",
        x = 20 * S,
        y = 20 * S,
        font = native.systemFontBold,
        fontSize = 16 * S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Section 1: Different font sizes
    print("[Scene 3: Text] Testing different font sizes...")
    local sizeLabel = display.newText({
        parent = sceneGroup,
        text = "Font Sizes:",
        x = 20 * S,
        y = 55 * S,
        font = native.systemFont,
        fontSize = 12 * S
    })
    sizeLabel.anchorX = 0
    sizeLabel:setFillColor(0.7, 0.7, 0.7)
    
    local sizes = {10 * S, 14 * S, 18 * S, 24 * S, 32 * S}
    local yPos = 80 * S
    for _, size in ipairs(sizes) do
        local txt = display.newText({
            parent = sceneGroup,
            text = "Size " .. size,
            x = 30 * S,
            y = yPos,
            font = native.systemFont,
            fontSize = size
        })
        txt.anchorX = 0
        txt:setFillColor(0.9, 0.9, 0.9)
        yPos = yPos + size + 8 * S
    end
    
    -- Section 2: Different colors
    print("[Scene 3: Text] Testing different colors...")
    local colorLabel = display.newText({
        parent = sceneGroup,
        text = "Colors:",
        x = 160 * S,
        y = 55 * S,
        font = native.systemFont,
        fontSize = 12 * S
    })
    colorLabel.anchorX = 0
    colorLabel:setFillColor(0.7, 0.7, 0.7)
    
    local colors = {
        {name = "Red",    r = 1,   g = 0.3, b = 0.3},
        {name = "Green",  r = 0.3, g = 1,   b = 0.3},
        {name = "Blue",   r = 0.3, g = 0.3, b = 1},
        {name = "Yellow", r = 1,   g = 1,   b = 0.3},
        {name = "Cyan",   r = 0.3, g = 1,   b = 1},
        {name = "Purple", r = 1,   g = 0.3, b = 1},
        {name = "Orange", r = 1,   g = 0.6, b = 0.2},
        {name = "White",  r = 1,   g = 1,   b = 1},
    }
    
    yPos = 80 * S
    for _, color in ipairs(colors) do
        local txt = display.newText({
            parent = sceneGroup,
            text = color.name,
            x = 170 * S,
            y = yPos,
            font = native.systemFont,
            fontSize = 14 * S
        })
        txt.anchorX = 0
        txt:setFillColor(color.r, color.g, color.b)
        yPos = yPos + 22 * S
    end
    
    -- Section 3: Multi-line text
    print("[Scene 3: Text] Testing multi-line text...")
    local multilineLabel = display.newText({
        parent = sceneGroup,
        text = "Multi-line Text:",
        x = 20 * S,
        y = 260 * S,
        font = native.systemFont,
        fontSize = 12 * S
    })
    multilineLabel.anchorX = 0
    multilineLabel:setFillColor(0.7, 0.7, 0.7)
    
    local multilineText = "This is line 1\nThis is line 2\nThis is line 3\nFinal line"
    local mtxt = display.newText({
        parent = sceneGroup,
        text = multilineText,
        x = 30 * S,
        y = 310 * S,
        width = 120 * S,
        font = native.systemFont,
        fontSize = 12 * S,
        align = "left"
    })
    mtxt.anchorX = 0
    mtxt:setFillColor(0.85, 0.85, 0.9)
    
    -- Section 4: Different alignments
    print("[Scene 3: Text] Testing text alignments...")
    local alignLabel = display.newText({
        parent = sceneGroup,
        text = "Alignments:",
        x = 180 * S,
        y = 260 * S,
        font = native.systemFont,
        fontSize = 12 * S
    })
    alignLabel.anchorX = 0
    alignLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Left align
    local leftAlign = display.newText({
        parent = sceneGroup,
        text = "Left aligned text sample",
        x = 180 * S,
        y = 285 * S,
        width = 120 * S,
        font = native.systemFont,
        fontSize = 11 * S,
        align = "left"
    })
    leftAlign.anchorX = 0
    leftAlign:setFillColor(0.8, 0.9, 0.8)
    
    -- Center align
    local centerAlign = display.newText({
        parent = sceneGroup,
        text = "Center aligned text",
        x = 240 * S,
        y = 330 * S,
        width = 120 * S,
        font = native.systemFont,
        fontSize = 11 * S,
        align = "center"
    })
    centerAlign:setFillColor(0.9, 0.8, 0.8)
    
    -- Right align
    local rightAlign = display.newText({
        parent = sceneGroup,
        text = "Right aligned text",
        x = 300 * S,
        y = 370 * S,
        width = 120 * S,
        font = native.systemFont,
        fontSize = 11 * S,
        align = "right"
    })
    rightAlign:setFillColor(0.8, 0.8, 0.9)
    
    -- Section 5: Bold text
    print("[Scene 3: Text] Testing font styles...")
    local styleLabel = display.newText({
        parent = sceneGroup,
        text = "Font Styles:",
        x = 20 * S,
        y = 400 * S,
        font = native.systemFont,
        fontSize = 12 * S
    })
    styleLabel.anchorX = 0
    styleLabel:setFillColor(0.7, 0.7, 0.7)
    
    local regularText = display.newText({
        parent = sceneGroup,
        text = "Regular Font",
        x = 30 * S,
        y = 425 * S,
        font = native.systemFont,
        fontSize = 14 * S
    })
    regularText.anchorX = 0
    regularText:setFillColor(0.9, 0.9, 0.9)
    
    local boldText = display.newText({
        parent = sceneGroup,
        text = "Bold Font",
        x = 30 * S,
        y = 450 * S,
        font = native.systemFontBold,
        fontSize = 14 * S
    })
    boldText.anchorX = 0
    boldText:setFillColor(0.9, 0.9, 0.9)
    
    -- Section 6: Long text with wrapping
    print("[Scene 3: Text] Testing text wrapping...")
    local wrapLabel = display.newText({
        parent = sceneGroup,
        text = "Text Wrapping:",
        x = 160 * S,
        y = 400 * S,
        font = native.systemFont,
        fontSize = 12 * S
    })
    wrapLabel.anchorX = 0
    wrapLabel:setFillColor(0.7, 0.7, 0.7)
    
    local longText = "This is a long text that should wrap automatically within the specified width boundary."
    local wrapText = display.newText({
        parent = sceneGroup,
        text = longText,
        x = 170 * S,
        y = 445 * S,
        width = 140 * S,
        font = native.systemFont,
        fontSize = 11 * S,
        align = "left"
    })
    wrapText.anchorX = 0
    wrapText:setFillColor(0.85, 0.85, 0.7)
    
    print("[Scene 3: Text] Creation complete - All text tests rendered")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 3: Text] Show will")
        _G.bgfxDemoCurrentScene = 3
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 3: Text] Show did")
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 3: Text] Hide will")
    elseif event.phase == "did" then
        print("[Scene 3: Text] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 3: Text] Destroy")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
