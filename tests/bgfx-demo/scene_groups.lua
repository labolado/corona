--[[
    scene_groups.lua - Scene 7: Groups and Hierarchy
    
    Tests:
    - display.newGroup - nested groups
    - Group transform (move, rotate, scale)
    - Independent transforms within groups
    - insert/remove operations
    - toFront/toBack ordering
--]]

local composer = require("composer")
local scene = composer.newScene()

local W = display.contentWidth
local H = display.contentHeight
local S = W / 320

-- Animation references
local activeTransitions = {}

-- Store references to animated objects for reset
local sceneObjects = {}

-- Function to start all animations (called from create and show)
local function startAnimations()
    print("[Scene 7: Groups] Starting animations...")
    
    -- Clear any existing transitions first
    for _, t in ipairs(activeTransitions) do
        if t and t.cancel then
            transition.cancel(t)
        end
    end
    activeTransitions = {}
    transition.cancelAll()
    
    -- Reset object properties to initial values
    if sceneObjects.basicGroup then
        sceneObjects.basicGroup.rotation = 0
    end
    if sceneObjects.outerGroup then
        sceneObjects.outerGroup.rotation = 0
    end
    if sceneObjects.innerGroup then
        sceneObjects.innerGroup.rotation = 0
    end
    if sceneObjects.transformGroup then
        sceneObjects.transformGroup.x = 100*S
        sceneObjects.transformGroup.y = 230*S
        sceneObjects.transformGroup.rotation = 0
        sceneObjects.transformGroup.xScale = 1
        sceneObjects.transformGroup.yScale = 1
    end
    if sceneObjects.insertGroup then
        sceneObjects.insertGroup.rotation = 0
    end
    if sceneObjects.indGroup then
        sceneObjects.indGroup.x = 120*S
        sceneObjects.indGroup.y = 490*S
    end
    if sceneObjects.indObj1 then
        sceneObjects.indObj1.rotation = 0
    end
    if sceneObjects.indObj2 then
        sceneObjects.indObj2.rotation = 0
    end
    if sceneObjects.indObj3 then
        sceneObjects.indObj3.rotation = 0
    end
    
    -- Section 1: Animate basic group
    local function animateBasicGroup()
        local t = transition.to(sceneObjects.basicGroup, {
            rotation = 360,
            time = 4000,
            iterations = 0
        })
        table.insert(activeTransitions, t)
    end
    animateBasicGroup()
    
    -- Section 2: Animate nested groups
    local function animateNestedGroups()
        local t1 = transition.to(sceneObjects.outerGroup, {
            rotation = 360,
            time = 6000,
            iterations = 0
        })
        table.insert(activeTransitions, t1)
        
        local t2 = transition.to(sceneObjects.innerGroup, {
            rotation = -720,
            time = 3000,
            iterations = 0
        })
        table.insert(activeTransitions, t2)
    end
    animateNestedGroups()
    
    -- Section 3: Complex group animation
    local function animateTransformGroup()
        local t = transition.to(sceneObjects.transformGroup, {
            x = 200*S,
            rotation = 180,
            xScale = 1.5,
            yScale = 1.5,
            time = 2000,
            transition = easing.inOutQuad,
            onComplete = function()
                local t2 = transition.to(sceneObjects.transformGroup, {
                    x = 100*S,
                    rotation = 0,
                    xScale = 1,
                    yScale = 1,
                    time = 2000,
                    transition = easing.inOutQuad,
                    onComplete = animateTransformGroup
                })
                table.insert(activeTransitions, t2)
            end
        })
        table.insert(activeTransitions, t)
    end
    animateTransformGroup()
    
    -- Section 4: Animate insert group
    local function animateInsertGroup()
        local t = transition.to(sceneObjects.insertGroup, {
            rotation = 360,
            time = 3000,
            iterations = 0
        })
        table.insert(activeTransitions, t)
    end
    animateInsertGroup()
    
    -- Section 6: Animate independent transforms group
    local t1 = transition.to(sceneObjects.indGroup, {
        x = 240*S,
        time = 2000,
        iterations = 0,
        transition = easing.continuousLoop
    })
    table.insert(activeTransitions, t1)
    
    -- Animate objects independently within group
    local t2 = transition.to(sceneObjects.indObj1, {
        rotation = 360,
        time = 1500,
        iterations = 0
    })
    table.insert(activeTransitions, t2)
    
    local t3 = transition.to(sceneObjects.indObj2, {
        rotation = -360,
        time = 2000,
        iterations = 0
    })
    table.insert(activeTransitions, t3)
    
    local t4 = transition.to(sceneObjects.indObj3, {
        rotation = 360,
        time = 1000,
        iterations = 0
    })
    table.insert(activeTransitions, t4)
    
    print("[Scene 7: Groups] Animations started")
end

function scene:create(event)
    local sceneGroup = self.view
    
    print("[Scene 7: Groups] Creating...")
    
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
        text = "Scene 7: Groups & Hierarchy",
        x = 20*S,
        y = 20*S,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Section 1: Basic group with multiple objects
    print("[Scene 7: Groups] Testing basic group...")
    local basicLabel = display.newText({
        parent = sceneGroup,
        text = "Basic Group:",
        x = 20*S,
        y = 55*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    basicLabel.anchorX = 0
    basicLabel:setFillColor(0.7, 0.7, 0.7)
    
    local basicGroup = display.newGroup()
    basicGroup.x, basicGroup.y = 80*S, 100*S
    sceneGroup:insert(basicGroup)
    sceneObjects.basicGroup = basicGroup
    
    -- Add objects to group
    local rect1 = display.newRect(basicGroup, 0, -20*S, 30*S, 30*S)
    rect1:setFillColor(0.9, 0.4, 0.4)
    
    local rect2 = display.newRect(basicGroup, -20*S, 20*S, 30*S, 30*S)
    rect2:setFillColor(0.4, 0.9, 0.4)
    
    local rect3 = display.newRect(basicGroup, 20*S, 20*S, 30*S, 30*S)
    rect3:setFillColor(0.4, 0.4, 0.9)
    
    -- Section 2: Nested groups
    print("[Scene 7: Groups] Testing nested groups...")
    local nestedLabel = display.newText({
        parent = sceneGroup,
        text = "Nested Groups:",
        x = 170*S,
        y = 55*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    nestedLabel.anchorX = 0
    nestedLabel:setFillColor(0.7, 0.7, 0.7)
    
    local outerGroup = display.newGroup()
    outerGroup.x, outerGroup.y = 240*S, 100*S
    sceneGroup:insert(outerGroup)
    sceneObjects.outerGroup = outerGroup
    
    local innerGroup = display.newGroup()
    outerGroup:insert(innerGroup)
    sceneObjects.innerGroup = innerGroup
    
    -- Outer group objects
    local outerRect = display.newRect(outerGroup, 0, 0, 70*S, 70*S)
    outerRect:setFillColor(0.3, 0.3, 0.3, 0.5)
    outerRect.strokeWidth = 2*S
    outerRect:setStrokeColor(0.6, 0.6, 0.6)
    
    -- Inner group objects
    local innerRect1 = display.newRect(innerGroup, -15*S, 0, 25*S, 25*S)
    innerRect1:setFillColor(0.9, 0.6, 0.2)
    
    local innerRect2 = display.newRect(innerGroup, 15*S, 0, 25*S, 25*S)
    innerRect2:setFillColor(0.2, 0.6, 0.9)
    
    -- Section 3: Group transform affecting children
    print("[Scene 7: Groups] Testing group transform inheritance...")
    local transformLabel = display.newText({
        parent = sceneGroup,
        text = "Transform Inheritance:",
        x = 20*S,
        y = 170*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    transformLabel.anchorX = 0
    transformLabel:setFillColor(0.7, 0.7, 0.7)
    
    local transformGroup = display.newGroup()
    transformGroup.x, transformGroup.y = 100*S, 230*S
    sceneGroup:insert(transformGroup)
    sceneObjects.transformGroup = transformGroup
    
    -- Add various objects
    local tRect = display.newRect(transformGroup, -30*S, 0, 25*S, 25*S)
    tRect:setFillColor(1, 0.3, 0.3)
    
    local tCircle = display.newCircle(transformGroup, 0, 0, 15*S)
    tCircle:setFillColor(0.3, 1, 0.3)
    
    local tText = display.newText({
        parent = transformGroup,
        text = "A",
        x = 30*S,
        y = 0,
        font = native.systemFontBold,
        fontSize = 20*S
    })
    tText:setFillColor(0.3, 0.3, 1)
    
    -- Section 4: Insert/Remove operations
    print("[Scene 7: Groups] Testing insert/remove operations...")
    local insertLabel = display.newText({
        parent = sceneGroup,
        text = "Insert/Remove:",
        x = 20*S,
        y = 300*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    insertLabel.anchorX = 0
    insertLabel:setFillColor(0.7, 0.7, 0.7)
    
    local insertGroup = display.newGroup()
    insertGroup.x, insertGroup.y = 90*S, 350*S
    sceneGroup:insert(insertGroup)
    sceneObjects.insertGroup = insertGroup
    
    -- Container outline
    local container = display.newRect(sceneGroup, 90*S, 350*S, 120*S, 80*S)
    container:setFillColor(0.2, 0.2, 0.25)
    container.strokeWidth = 1*S
    container:setStrokeColor(0.5, 0.5, 0.5)
    
    -- Dynamic object
    local dynamicObj = display.newCircle(0, 0, 15*S) -- Start not in group
    dynamicObj:setFillColor(0.9, 0.5, 0.2)
    sceneGroup:insert(dynamicObj)
    dynamicObj.x, dynamicObj.y = 40*S, 350*S
    
    local insertState = display.newText({
        parent = sceneGroup,
        text = "Outside",
        x = 40*S,
        y = 380*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    insertState:setFillColor(0.6, 0.6, 0.6)
    
    local isInside = false
    local function toggleInsert()
        if isInside then
            -- Remove from group
            insertGroup:remove(dynamicObj)
            sceneGroup:insert(dynamicObj)
            dynamicObj.x, dynamicObj.y = 40*S, 350*S
            insertState.text = "Outside"
            isInside = false
        else
            -- Insert into group
            insertGroup:insert(dynamicObj)
            dynamicObj.x, dynamicObj.y = 0, 0
            insertState.text = "Inside"
            isInside = true
        end
    end
    
    timer.performWithDelay(1500, toggleInsert, 0)
    
    -- Section 5: toFront/toBack ordering
    print("[Scene 7: Groups] Testing toFront/toBack ordering...")
    local orderLabel = display.newText({
        parent = sceneGroup,
        text = "toFront/toBack:",
        x = 180*S,
        y = 300*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    orderLabel.anchorX = 0
    orderLabel:setFillColor(0.7, 0.7, 0.7)
    
    local orderGroup = display.newGroup()
    orderGroup.x, orderGroup.y = 250*S, 350*S
    sceneGroup:insert(orderGroup)
    
    -- Three overlapping objects
    local backObj = display.newRect(orderGroup, 0, -15*S, 50*S, 50*S)
    backObj:setFillColor(0.9, 0.3, 0.3)
    
    local midObj = display.newRect(orderGroup, 0, 0, 50*S, 50*S)
    midObj:setFillColor(0.3, 0.9, 0.3)
    
    local frontObj = display.newRect(orderGroup, 0, 15*S, 50*S, 50*S)
    frontObj:setFillColor(0.3, 0.3, 0.9)
    
    local orderText = display.newText({
        parent = sceneGroup,
        text = "Order: R-G-B",
        x = 250*S,
        y = 410*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    orderText:setFillColor(0.6, 0.6, 0.6)
    
    -- Cycle through orderings
    local orderState = 0
    local function cycleOrder()
        orderState = (orderState + 1) % 3
        if orderState == 0 then
            frontObj:toFront()
            orderText.text = "Blue front"
        elseif orderState == 1 then
            midObj:toFront()
            orderText.text = "Green front"
        else
            backObj:toFront()
            orderText.text = "Red front"
        end
    end
    
    timer.performWithDelay(1000, cycleOrder, 0)
    
    -- Section 6: Object with independent transform in parent group
    print("[Scene 7: Groups] Testing independent transforms within group...")
    local independentLabel = display.newText({
        parent = sceneGroup,
        text = "Independent Transforms:",
        x = 20*S,
        y = 440*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    independentLabel.anchorX = 0
    independentLabel:setFillColor(0.7, 0.7, 0.7)
    
    local indGroup = display.newGroup()
    indGroup.x, indGroup.y = 120*S, 490*S
    sceneGroup:insert(indGroup)
    sceneObjects.indGroup = indGroup
    
    local indBase = display.newRect(indGroup, 0, 0, 80*S, 40*S)
    indBase:setFillColor(0.3, 0.3, 0.3, 0.5)
    indBase.strokeWidth = 1*S
    indBase:setStrokeColor(0.6, 0.6, 0.6)
    
    local indObj1 = display.newRect(indGroup, -25*S, 0, 20*S, 20*S)
    indObj1:setFillColor(0.9, 0.5, 0.2)
    sceneObjects.indObj1 = indObj1
    
    local indObj2 = display.newRect(indGroup, 0, 0, 20*S, 20*S)
    indObj2:setFillColor(0.2, 0.9, 0.5)
    sceneObjects.indObj2 = indObj2
    
    local indObj3 = display.newRect(indGroup, 25*S, 0, 20*S, 20*S)
    indObj3:setFillColor(0.2, 0.5, 0.9)
    sceneObjects.indObj3 = indObj3
    
    -- Start animations after all objects are created
    startAnimations()
    
    print("[Scene 7: Groups] Creation complete - All group tests rendered")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 7: Groups] Show will")
        _G.bgfxDemoCurrentScene = 7
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 7: Groups] Show did")
        -- Restart animations when scene is shown (for re-entry)
        startAnimations()
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 7: Groups] Hide will - cleaning up transitions")
        for _, t in ipairs(activeTransitions) do
            if t and t.cancel then
                transition.cancel(t)
            end
        end
        activeTransitions = {}
        transition.cancelAll()
    elseif event.phase == "did" then
        print("[Scene 7: Groups] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 7: Groups] Destroy")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
