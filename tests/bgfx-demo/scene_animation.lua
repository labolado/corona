--[[
    scene_animation.lua - Scene 6: Animation
    
    Tests:
    - transition.to - move, rotate, scale, fade
    - Multiple simultaneous animations
    - Chained animations (onComplete)
    - timer.performWithDelay
--]]

local composer = require("composer")
local scene = composer.newScene()

-- Animation references for cleanup
local activeTransitions = {}
local activeTimers = {}

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 6: Animation] Creating...")
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.1, 0.1, 0.15)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 6: Animation",
        x = 20,
        y = 20,
        font = native.systemFontBold,
        fontSize = 16
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Section 1: Basic transitions (move, rotate, scale, fade)
    print("[Scene 6: Animation] Setting up basic transitions...")
    local basicLabel = display.newText({
        parent = sceneGroup,
        text = "Basic transitions:",
        x = 20,
        y = 55,
        font = native.systemFont,
        fontSize = 12
    })
    basicLabel.anchorX = 0
    basicLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Move animation
    local moveRect = display.newRect(sceneGroup, 40, 90, 30, 30)
    moveRect:setFillColor(0.9, 0.4, 0.4)
    local moveLabel = display.newText({
        parent = sceneGroup,
        text = "Move",
        x = 40,
        y = 120,
        font = native.systemFont,
        fontSize = 9
    })
    moveLabel:setFillColor(0.6, 0.6, 0.6)
    
    local function animateMove()
        local t = transition.to(moveRect, {
            x = 100,
            time = 1000,
            transition = easing.inOutQuad,
            onComplete = function()
                transition.to(moveRect, {
                    x = 40,
                    time = 1000,
                    transition = easing.inOutQuad,
                    onComplete = animateMove
                })
            end
        })
        table.insert(activeTransitions, t)
    end
    animateMove()
    
    -- Rotate animation
    local rotateRect = display.newRect(sceneGroup, 140, 90, 30, 30)
    rotateRect:setFillColor(0.4, 0.9, 0.4)
    local rotateLabel = display.newText({
        parent = sceneGroup,
        text = "Rotate",
        x = 140,
        y = 120,
        font = native.systemFont,
        fontSize = 9
    })
    rotateLabel:setFillColor(0.6, 0.6, 0.6)
    
    local function animateRotate()
        local t = transition.to(rotateRect, {
            rotation = 360,
            time = 2000,
            transition = easing.inOutQuad,
            onComplete = function()
                rotateRect.rotation = 0
                animateRotate()
            end
        })
        table.insert(activeTransitions, t)
    end
    animateRotate()
    
    -- Scale animation
    local scaleRect = display.newRect(sceneGroup, 220, 90, 30, 30)
    scaleRect:setFillColor(0.4, 0.4, 0.9)
    local scaleLabel = display.newText({
        parent = sceneGroup,
        text = "Scale",
        x = 220,
        y = 120,
        font = native.systemFont,
        fontSize = 9
    })
    scaleLabel:setFillColor(0.6, 0.6, 0.6)
    
    local function animateScale()
        local t = transition.to(scaleRect, {
            xScale = 1.5,
            yScale = 1.5,
            time = 1000,
            transition = easing.inOutQuad,
            onComplete = function()
                transition.to(scaleRect, {
                    xScale = 1.0,
                    yScale = 1.0,
                    time = 1000,
                    transition = easing.inOutQuad,
                    onComplete = animateScale
                })
            end
        })
        table.insert(activeTransitions, t)
    end
    animateScale()
    
    -- Fade animation
    local fadeRect = display.newRect(sceneGroup, 290, 90, 30, 30)
    fadeRect:setFillColor(0.9, 0.9, 0.4)
    local fadeLabel = display.newText({
        parent = sceneGroup,
        text = "Fade",
        x = 290,
        y = 120,
        font = native.systemFont,
        fontSize = 9
    })
    fadeLabel:setFillColor(0.6, 0.6, 0.6)
    
    local function animateFade()
        local t = transition.to(fadeRect, {
            alpha = 0.2,
            time = 1000,
            transition = easing.inOutQuad,
            onComplete = function()
                transition.to(fadeRect, {
                    alpha = 1.0,
                    time = 1000,
                    transition = easing.inOutQuad,
                    onComplete = animateFade
                })
            end
        })
        table.insert(activeTransitions, t)
    end
    animateFade()
    
    -- Section 2: Multiple simultaneous animations
    print("[Scene 6: Animation] Setting up simultaneous animations...")
    local multiLabel = display.newText({
        parent = sceneGroup,
        text = "Simultaneous properties:",
        x = 20,
        y = 150,
        font = native.systemFont,
        fontSize = 12
    })
    multiLabel.anchorX = 0
    multiLabel:setFillColor(0.7, 0.7, 0.7)
    
    local multiRect = display.newRect(sceneGroup, 160, 195, 40, 40)
    multiRect:setFillColor(0.9, 0.5, 0.2)
    local multiSublabel = display.newText({
        parent = sceneGroup,
        text = "Move+Rotate+Scale+Fade",
        x = 160,
        y = 240,
        font = native.systemFont,
        fontSize = 10
    })
    multiSublabel:setFillColor(0.6, 0.6, 0.6)
    
    local function animateMulti()
        local t = transition.to(multiRect, {
            x = 240,
            rotation = 180,
            xScale = 1.5,
            yScale = 0.7,
            alpha = 0.5,
            time = 1500,
            transition = easing.inOutQuad,
            onComplete = function()
                local t2 = transition.to(multiRect, {
                    x = 160,
                    rotation = 0,
                    xScale = 1.0,
                    yScale = 1.0,
                    alpha = 1.0,
                    time = 1500,
                    transition = easing.inOutQuad,
                    onComplete = animateMulti
                })
                table.insert(activeTransitions, t2)
            end
        })
        table.insert(activeTransitions, t)
    end
    animateMulti()
    
    -- Section 3: Chained animations (onComplete)
    print("[Scene 6: Animation] Setting up chained animations...")
    local chainLabel = display.newText({
        parent = sceneGroup,
        text = "Chained animation sequence:",
        x = 20,
        y = 270,
        font = native.systemFont,
        fontSize = 12
    })
    chainLabel.anchorX = 0
    chainLabel:setFillColor(0.7, 0.7, 0.7)
    
    local chainRect = display.newRect(sceneGroup, 60, 320, 35, 35)
    chainRect:setFillColor(0.6, 0.3, 0.9)
    
    local chainText = display.newText({
        parent = sceneGroup,
        text = "Step 1",
        x = 60,
        y = 365,
        font = native.systemFont,
        fontSize = 10
    })
    chainText:setFillColor(0.6, 0.6, 0.6)
    
    local function chainAnimation1()
        chainText.text = "Move →"
        local t = transition.to(chainRect, {
            x = 160,
            time = 500,
            onComplete = function()
                local t2 = transition.to(chainRect, {
                    rotation = 90,
                    time = 500,
                    onComplete = function()
                        chainText.text = "Rotate ↓"
                        local t3 = transition.to(chainRect, {
                            y = 360,
                            time = 500,
                            onComplete = function()
                                local t4 = transition.to(chainRect, {
                                    xScale = 1.5,
                                    yScale = 1.5,
                                    time = 500,
                                    onComplete = function()
                                        chainText.text = "Scale ←"
                                        local t5 = transition.to(chainRect, {
                                            x = 60,
                                            y = 320,
                                            rotation = 0,
                                            xScale = 1,
                                            yScale = 1,
                                            time = 800,
                                            onComplete = function()
                                                chainText.text = "Reset ↻"
                                                timer.performWithDelay(200, chainAnimation1)
                                            end
                                        })
                                        table.insert(activeTransitions, t5)
                                    end
                                })
                                table.insert(activeTransitions, t4)
                            end
                        })
                        table.insert(activeTransitions, t3)
                    end
                })
                table.insert(activeTransitions, t2)
            end
        })
        table.insert(activeTransitions, t)
    end
    chainAnimation1()
    
    -- Section 4: Timer-based animations
    print("[Scene 6: Animation] Setting up timer animations...")
    local timerLabel = display.newText({
        parent = sceneGroup,
        text = "Timer.performWithDelay:",
        x = 20,
        y = 400,
        font = native.systemFont,
        fontSize = 12
    })
    timerLabel.anchorX = 0
    timerLabel:setFillColor(0.7, 0.7, 0.7)
    
    local timerRect = display.newRect(sceneGroup, 220, 330, 30, 30)
    timerRect:setFillColor(0.2, 0.8, 0.6)
    
    local timerCounter = 0
    local timerText = display.newText({
        parent = sceneGroup,
        text = "Count: 0",
        x = 220,
        y = 370,
        font = native.systemFont,
        fontSize = 10
    })
    timerText:setFillColor(0.6, 0.6, 0.6)
    
    local tm = timer.performWithDelay(500, function()
        timerCounter = timerCounter + 1
        timerText.text = "Count: " .. timerCounter
        timerRect.rotation = timerRect.rotation + 30
        timerRect:setFillColor(
            0.2 + (timerCounter % 3) * 0.3,
            0.6 + (timerCounter % 2) * 0.2,
            0.4 + (timerCounter % 4) * 0.15
        )
    end, 0)
    table.insert(activeTimers, tm)
    
    -- Section 5: Multiple objects with staggered timing
    print("[Scene 6: Animation] Setting up staggered animations...")
    local staggerLabel = display.newText({
        parent = sceneGroup,
        text = "Staggered:",
        x = 20,
        y = 430,
        font = native.systemFont,
        fontSize = 12
    })
    staggerLabel.anchorX = 0
    staggerLabel:setFillColor(0.7, 0.7, 0.7)
    
    local staggerRects = {}
    for i = 1, 5 do
        local rect = display.newRect(sceneGroup, 120 + i * 30, 465, 20, 20)
        rect:setFillColor(0.5 + i * 0.1, 0.4, 0.9 - i * 0.1)
        table.insert(staggerRects, rect)
        
        local function staggerAnim()
            local t = transition.to(rect, {
                y = 505,
                time = 600,
                delay = (i - 1) * 100,
                transition = easing.outBounce,
                onComplete = function()
                    local t2 = transition.to(rect, {
                        y = 465,
                        time = 400,
                        transition = easing.inQuad,
                        onComplete = function()
                            timer.performWithDelay(500, staggerAnim)
                        end
                    })
                    table.insert(activeTransitions, t2)
                end
            })
            table.insert(activeTransitions, t)
        end
        timer.performWithDelay(i * 200, staggerAnim)
    end
    
    print("[Scene 6: Animation] Creation complete - All animations started")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 6: Animation] Show will")
        _G.bgfxDemoCurrentScene = 6
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 6: Animation] Show did")
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 6: Animation] Hide will - cancelling animations")
        -- Cancel all active transitions
        for _, t in ipairs(activeTransitions) do
            if t and t.cancel then
                transition.cancel(t)
            end
        end
        activeTransitions = {}
        
        -- Cancel all active timers
        for _, tm in ipairs(activeTimers) do
            if tm then
                timer.cancel(tm)
            end
        end
        activeTimers = {}
        transition.cancelAll()
    elseif event.phase == "did" then
        print("[Scene 6: Animation] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 6: Animation] Destroy")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
