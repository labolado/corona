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

-- Scaling variables for high resolution displays
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320  -- Scale factor

-- Animation references for cleanup
local activeTransitions = {}
local activeTimers = {}

-- Store references to display objects for reset
local sceneObjects = {}

-- Function to start all animations (called from create and show)
local function startAnimations(sceneGroup)
    print("[Scene 6: Animation] Starting animations...")
    
    -- Clear any existing animations first
    for _, t in ipairs(activeTransitions) do
        if t and t.cancel then
            transition.cancel(t)
        end
    end
    activeTransitions = {}
    
    for _, tm in ipairs(activeTimers) do
        if tm then
            timer.cancel(tm)
        end
    end
    activeTimers = {}
    
    -- Reset all object properties to initial values
    if sceneObjects.moveRect then
        sceneObjects.moveRect.x = 40*S
        sceneObjects.moveRect.y = 90*S
    end
    if sceneObjects.rotateRect then
        sceneObjects.rotateRect.rotation = 0
    end
    if sceneObjects.scaleRect then
        sceneObjects.scaleRect.xScale = 1.0
        sceneObjects.scaleRect.yScale = 1.0
    end
    if sceneObjects.fadeRect then
        sceneObjects.fadeRect.alpha = 1.0
    end
    if sceneObjects.multiRect then
        sceneObjects.multiRect.x = 160*S
        sceneObjects.multiRect.rotation = 0
        sceneObjects.multiRect.xScale = 1.0
        sceneObjects.multiRect.yScale = 1.0
        sceneObjects.multiRect.alpha = 1.0
    end
    if sceneObjects.chainRect then
        sceneObjects.chainRect.x = 60*S
        sceneObjects.chainRect.y = 320*S
        sceneObjects.chainRect.rotation = 0
        sceneObjects.chainRect.xScale = 1
        sceneObjects.chainRect.yScale = 1
    end
    if sceneObjects.chainText then
        sceneObjects.chainText.text = "Step 1"
    end
    if sceneObjects.timerRect then
        sceneObjects.timerRect.rotation = 0
        sceneObjects.timerRect:setFillColor(0.2, 0.8, 0.6)
    end
    if sceneObjects.timerText then
        sceneObjects.timerCounter = 0
        sceneObjects.timerText.text = "Count: 0"
    end
    -- Reset stagger rects
    if sceneObjects.staggerRects then
        for i, rect in ipairs(sceneObjects.staggerRects) do
            rect.y = 465*S
        end
    end
    
    -- Section 1: Basic transitions (move, rotate, scale, fade)
    
    -- Move animation
    local function animateMove()
        local t = transition.to(sceneObjects.moveRect, {
            x = 100*S,
            time = 1000,
            transition = easing.inOutQuad,
            onComplete = function()
                local t2 = transition.to(sceneObjects.moveRect, {
                    x = 40*S,
                    time = 1000,
                    transition = easing.inOutQuad,
                    onComplete = animateMove
                })
                table.insert(activeTransitions, t2)
            end
        })
        table.insert(activeTransitions, t)
    end
    animateMove()
    
    -- Rotate animation
    local function animateRotate()
        local t = transition.to(sceneObjects.rotateRect, {
            rotation = 360,
            time = 2000,
            transition = easing.inOutQuad,
            onComplete = function()
                sceneObjects.rotateRect.rotation = 0
                animateRotate()
            end
        })
        table.insert(activeTransitions, t)
    end
    animateRotate()
    
    -- Scale animation
    local function animateScale()
        local t = transition.to(sceneObjects.scaleRect, {
            xScale = 1.5,
            yScale = 1.5,
            time = 1000,
            transition = easing.inOutQuad,
            onComplete = function()
                local t2 = transition.to(sceneObjects.scaleRect, {
                    xScale = 1.0,
                    yScale = 1.0,
                    time = 1000,
                    transition = easing.inOutQuad,
                    onComplete = animateScale
                })
                table.insert(activeTransitions, t2)
            end
        })
        table.insert(activeTransitions, t)
    end
    animateScale()
    
    -- Fade animation
    local function animateFade()
        local t = transition.to(sceneObjects.fadeRect, {
            alpha = 0.2,
            time = 1000,
            transition = easing.inOutQuad,
            onComplete = function()
                local t2 = transition.to(sceneObjects.fadeRect, {
                    alpha = 1.0,
                    time = 1000,
                    transition = easing.inOutQuad,
                    onComplete = animateFade
                })
                table.insert(activeTransitions, t2)
            end
        })
        table.insert(activeTransitions, t)
    end
    animateFade()
    
    -- Section 2: Multiple simultaneous animations
    local function animateMulti()
        local t = transition.to(sceneObjects.multiRect, {
            x = 240*S,
            rotation = 180,
            xScale = 1.5,
            yScale = 0.7,
            alpha = 0.5,
            time = 1500,
            transition = easing.inOutQuad,
            onComplete = function()
                local t2 = transition.to(sceneObjects.multiRect, {
                    x = 160*S,
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
    local function chainAnimation1()
        sceneObjects.chainText.text = "Move →"
        local t = transition.to(sceneObjects.chainRect, {
            x = 160*S,
            time = 500,
            onComplete = function()
                local t2 = transition.to(sceneObjects.chainRect, {
                    rotation = 90,
                    time = 500,
                    onComplete = function()
                        sceneObjects.chainText.text = "Rotate ↓"
                        local t3 = transition.to(sceneObjects.chainRect, {
                            y = 360*S,
                            time = 500,
                            onComplete = function()
                                local t4 = transition.to(sceneObjects.chainRect, {
                                    xScale = 1.5,
                                    yScale = 1.5,
                                    time = 500,
                                    onComplete = function()
                                        sceneObjects.chainText.text = "Scale ←"
                                        local t5 = transition.to(sceneObjects.chainRect, {
                                            x = 60*S,
                                            y = 320*S,
                                            rotation = 0,
                                            xScale = 1,
                                            yScale = 1,
                                            time = 800,
                                            onComplete = function()
                                                sceneObjects.chainText.text = "Reset ↻"
                                                local tm = timer.performWithDelay(200, chainAnimation1)
                                                table.insert(activeTimers, tm)
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
    local tm = timer.performWithDelay(500, function()
        sceneObjects.timerCounter = sceneObjects.timerCounter + 1
        sceneObjects.timerText.text = "Count: " .. sceneObjects.timerCounter
        sceneObjects.timerRect.rotation = sceneObjects.timerRect.rotation + 30
        sceneObjects.timerRect:setFillColor(
            0.2 + (sceneObjects.timerCounter % 3) * 0.3,
            0.6 + (sceneObjects.timerCounter % 2) * 0.2,
            0.4 + (sceneObjects.timerCounter % 4) * 0.15
        )
    end, 0)
    table.insert(activeTimers, tm)
    
    -- Section 5: Multiple objects with staggered timing
    for i = 1, 5 do
        local rect = sceneObjects.staggerRects[i]
        local function staggerAnim()
            local t = transition.to(rect, {
                y = 505*S,
                time = 600,
                delay = (i - 1) * 100,
                transition = easing.outBounce,
                onComplete = function()
                    local t2 = transition.to(rect, {
                        y = 465*S,
                        time = 400,
                        transition = easing.inQuad,
                        onComplete = function()
                            local tm2 = timer.performWithDelay(500, staggerAnim)
                            table.insert(activeTimers, tm2)
                        end
                    })
                    table.insert(activeTransitions, t2)
                end
            })
            table.insert(activeTransitions, t)
        end
        local tm3 = timer.performWithDelay(i * 200, staggerAnim)
        table.insert(activeTimers, tm3)
    end
    
    print("[Scene 6: Animation] Animations started")
end

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
        x = 20*S,
        y = 20*S,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Section 1: Basic transitions (move, rotate, scale, fade)
    print("[Scene 6: Animation] Setting up basic transitions...")
    local basicLabel = display.newText({
        parent = sceneGroup,
        text = "Basic transitions:",
        x = 20*S,
        y = 55*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    basicLabel.anchorX = 0
    basicLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Move animation
    sceneObjects.moveRect = display.newRect(sceneGroup, 40*S, 90*S, 30*S, 30*S)
    sceneObjects.moveRect:setFillColor(0.9, 0.4, 0.4)
    local moveLabel = display.newText({
        parent = sceneGroup,
        text = "Move",
        x = 40*S,
        y = 120*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    moveLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Rotate animation
    sceneObjects.rotateRect = display.newRect(sceneGroup, 140*S, 90*S, 30*S, 30*S)
    sceneObjects.rotateRect:setFillColor(0.4, 0.9, 0.4)
    local rotateLabel = display.newText({
        parent = sceneGroup,
        text = "Rotate",
        x = 140*S,
        y = 120*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    rotateLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Scale animation
    sceneObjects.scaleRect = display.newRect(sceneGroup, 220*S, 90*S, 30*S, 30*S)
    sceneObjects.scaleRect:setFillColor(0.4, 0.4, 0.9)
    local scaleLabel = display.newText({
        parent = sceneGroup,
        text = "Scale",
        x = 220*S,
        y = 120*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    scaleLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Fade animation
    sceneObjects.fadeRect = display.newRect(sceneGroup, 290*S, 90*S, 30*S, 30*S)
    sceneObjects.fadeRect:setFillColor(0.9, 0.9, 0.4)
    local fadeLabel = display.newText({
        parent = sceneGroup,
        text = "Fade",
        x = 290*S,
        y = 120*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    fadeLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 2: Multiple simultaneous animations
    print("[Scene 6: Animation] Setting up simultaneous animations...")
    local multiLabel = display.newText({
        parent = sceneGroup,
        text = "Simultaneous properties:",
        x = 20*S,
        y = 150*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    multiLabel.anchorX = 0
    multiLabel:setFillColor(0.7, 0.7, 0.7)
    
    sceneObjects.multiRect = display.newRect(sceneGroup, 160*S, 195*S, 40*S, 40*S)
    sceneObjects.multiRect:setFillColor(0.9, 0.5, 0.2)
    local multiSublabel = display.newText({
        parent = sceneGroup,
        text = "Move+Rotate+Scale+Fade",
        x = 160*S,
        y = 240*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    multiSublabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 3: Chained animations (onComplete)
    print("[Scene 6: Animation] Setting up chained animations...")
    local chainLabel = display.newText({
        parent = sceneGroup,
        text = "Chained animation sequence:",
        x = 20*S,
        y = 270*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    chainLabel.anchorX = 0
    chainLabel:setFillColor(0.7, 0.7, 0.7)
    
    sceneObjects.chainRect = display.newRect(sceneGroup, 60*S, 320*S, 35*S, 35*S)
    sceneObjects.chainRect:setFillColor(0.6, 0.3, 0.9)
    
    sceneObjects.chainText = display.newText({
        parent = sceneGroup,
        text = "Step 1",
        x = 60*S,
        y = 365*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    sceneObjects.chainText:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 4: Timer-based animations
    print("[Scene 6: Animation] Setting up timer animations...")
    local timerLabel = display.newText({
        parent = sceneGroup,
        text = "Timer.performWithDelay:",
        x = 20*S,
        y = 400*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    timerLabel.anchorX = 0
    timerLabel:setFillColor(0.7, 0.7, 0.7)
    
    sceneObjects.timerRect = display.newRect(sceneGroup, 220*S, 330*S, 30*S, 30*S)
    sceneObjects.timerRect:setFillColor(0.2, 0.8, 0.6)
    
    sceneObjects.timerCounter = 0
    sceneObjects.timerText = display.newText({
        parent = sceneGroup,
        text = "Count: 0",
        x = 220*S,
        y = 370*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    sceneObjects.timerText:setFillColor(0.6, 0.6, 0.6)
    
    -- Section 5: Multiple objects with staggered timing
    print("[Scene 6: Animation] Setting up staggered animations...")
    local staggerLabel = display.newText({
        parent = sceneGroup,
        text = "Staggered:",
        x = 20*S,
        y = 430*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    staggerLabel.anchorX = 0
    staggerLabel:setFillColor(0.7, 0.7, 0.7)
    
    sceneObjects.staggerRects = {}
    for i = 1, 5 do
        local rect = display.newRect(sceneGroup, (120 + i * 30)*S, 465*S, 20*S, 20*S)
        rect:setFillColor(0.5 + i * 0.1, 0.4, 0.9 - i * 0.1)
        table.insert(sceneObjects.staggerRects, rect)
    end
    
    -- Start animations after all objects are created
    startAnimations(sceneGroup)
    
    print("[Scene 6: Animation] Creation complete")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 6: Animation] Show will")
        _G.bgfxDemoCurrentScene = 6
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 6: Animation] Show did")
        -- Restart animations when scene is shown (for re-entry)
        startAnimations(self.view)
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
