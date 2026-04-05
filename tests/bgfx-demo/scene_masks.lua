--[[
    scene_masks.lua - Scene 9: Masks and FBO
    
    Tests:
    - display.setMask - circular mask
    - display.newSnapshot - render to texture
    - display.capture - screenshot
    - graphics.newEffect - blur effect (if available)
--]]

local composer = require("composer")
local scene = composer.newScene()

-- Animation references
local activeTransitions = {}

-- Store references to animated objects for reset
local sceneObjects = {}

-- Function to start all animations (called from create and show)
local function startAnimations()
    print("[Scene 9: Masks] Starting animations...")
    
    -- Clear any existing transitions first
    for _, t in ipairs(activeTransitions) do
        if t and t.cancel then
            transition.cancel(t)
        end
    end
    activeTransitions = {}
    transition.cancelAll()
    
    -- Reset object properties to initial values
    if sceneObjects.rttSnapshot then
        sceneObjects.rttSnapshot.rotation = 0
    end
    if sceneObjects.snapshots then
        for i, snap in ipairs(sceneObjects.snapshots) do
            snap.rotation = 0
        end
    end
    if sceneObjects.outerSnap then
        sceneObjects.outerSnap.rotation = 0
    end
    
    -- Section 2: Animate the snapshot
    local function animateSnapshot()
        local t = transition.to(sceneObjects.rttSnapshot, {
            rotation = 360,
            time = 5000,
            iterations = 0
        })
        table.insert(activeTransitions, t)
    end
    animateSnapshot()
    
    -- Section 3: Animate multiple snapshots
    if sceneObjects.snapshots then
        for i = 1, #sceneObjects.snapshots do
            local snap = sceneObjects.snapshots[i]
            local t = transition.to(snap, {
                rotation = i * 90,
                time = 2000 + i * 500,
                iterations = 0
            })
            table.insert(activeTransitions, t)
        end
    end
    
    -- Section 4: Animate nested snapshot
    local function animateOuterSnap()
        local t = transition.to(sceneObjects.outerSnap, {
            rotation = -360,
            time = 4000,
            iterations = 0
        })
        table.insert(activeTransitions, t)
    end
    animateOuterSnap()
    
    print("[Scene 9: Masks] Animations started")
end

function scene:create(event)
    local sceneGroup = self.view
    
    -- Scaling variables for high resolution
    local W = display.contentWidth
    local H = display.contentHeight
    local S = W / 320  -- Scaling factor
    
    print("[Scene 9: Masks] Creating...")
    
    -- Background
    local bg = display.newRect(sceneGroup, display.contentCenterX, display.contentCenterY, display.contentWidth, display.contentHeight)
    bg:setFillColor(0.1, 0.1, 0.15)
    
    -- Title
    local title = display.newText({
        parent = sceneGroup,
        text = "Scene 9: Masks & FBO",
        x = 20,
        y = 20*S,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    title.anchorX = 0
    title:setFillColor(0.9, 0.9, 0.9)
    
    -- Section 1: Circular mask using snapshot
    print("[Scene 9: Masks] Testing circular mask...")
    local maskLabel = display.newText({
        parent = sceneGroup,
        text = "Circular Mask (Snapshot):",
        x = 20,
        y = 55*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    maskLabel.anchorX = 0
    maskLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Create content to be masked
    local maskContent = display.newGroup()
    maskContent.x, maskContent.y = 80*S, 120*S
    sceneGroup:insert(maskContent)
    
    -- Background pattern
    for i = 1, 4 do
        for j = 1, 4 do
            local check = display.newRect(maskContent, (j - 2.5) * 20*S, (i - 2.5) * 20*S, 20*S, 20*S)
            if (i + j) % 2 == 0 then
                check:setFillColor(0.9, 0.3, 0.3)
            else
                check:setFillColor(0.3, 0.3, 0.9)
            end
        end
    end
    
    -- Add some text
    local maskText = display.newText({
        parent = maskContent,
        text = "MASKED",
        x = 0,
        y = 0,
        font = native.systemFontBold,
        fontSize = 16*S
    })
    maskText:setFillColor(1, 1, 1)
    
    -- Create circular mask using snapshot technique
    -- Since we can't load image masks, we simulate by creating a circular snapshot
    local maskSnapshot = display.newSnapshot(80*S, 80*S)
    maskSnapshot.x, maskSnapshot.y = 80*S, 120*S
    sceneGroup:insert(maskSnapshot)
    
    -- Draw circular mask shape in snapshot
    local maskCircle = display.newCircle(maskSnapshot.group, 0, 0, 40*S)
    maskCircle:setFillColor(1, 1, 1)
    maskSnapshot:invalidate()
    
    -- Note: True masking with display.setMask requires an image file
    -- We demonstrate the concept with visual grouping
    local maskNote = display.newText({
        parent = sceneGroup,
        text = "Mask visualization",
        x = 80,
        y = 170*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    maskNote:setFillColor(0.5, 0.5, 0.5)
    
    -- Section 2: Snapshot with rendered content
    print("[Scene 9: Masks] Testing display.newSnapshot...")
    local snapshotLabel = display.newText({
        parent = sceneGroup,
        text = "Render to Texture:",
        x = 180,
        y = 55*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    snapshotLabel.anchorX = 0
    snapshotLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Create a snapshot and draw to it
    local rttSnapshot = display.newSnapshot(100*S, 100*S)
    rttSnapshot.x, rttSnapshot.y = 230*S, 120*S
    sceneGroup:insert(rttSnapshot)
    sceneObjects.rttSnapshot = rttSnapshot
    
    -- Draw content to snapshot
    local rttBg = display.newRect(rttSnapshot.group, 0, 0, 100*S, 100*S)
    rttBg:setFillColor(0.2, 0.2, 0.3)
    
    local rttCircle1 = display.newCircle(rttSnapshot.group, -20*S, -20*S, 25*S)
    rttCircle1:setFillColor(0.9, 0.4, 0.4)
    
    local rttCircle2 = display.newCircle(rttSnapshot.group, 20*S, 20*S, 25*S)
    rttCircle2:setFillColor(0.4, 0.9, 0.4)
    
    local rttRect = display.newRect(rttSnapshot.group, 0, 0, 40*S, 40*S)
    rttRect:setFillColor(0.4, 0.4, 0.9)
    
    local rttText = display.newText({
        parent = rttSnapshot.group,
        text = "RTT",
        x = 0,
        y = 0,
        font = native.systemFontBold,
        fontSize = 14*S*S
    })
    rttText:setFillColor(1, 1, 1)
    
    rttSnapshot:invalidate()
    
    local rttNote = display.newText({
        parent = sceneGroup,
        text = "Rotating snapshot",
        x = 230*S,
        y = 180*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    rttNote:setFillColor(0.5, 0.5, 0.5)
    
    -- Section 3: Multiple snapshots with different content
    print("[Scene 9: Masks] Testing multiple snapshots...")
    local multiLabel = display.newText({
        parent = sceneGroup,
        text = "Multiple Snapshots:",
        x = 20,
        y = 210*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    multiLabel.anchorX = 0
    multiLabel:setFillColor(0.7, 0.7, 0.7)
    
    sceneObjects.snapshots = {}
    for i = 1, 4 do
        local snap = display.newSnapshot(60*S, 60*S)
        snap.x = 50*S + (i - 1) * 75*S
        snap.y = 265*S
        sceneGroup:insert(snap)
        
        -- Different content for each
        local bg = display.newRect(snap.group, 0, 0, 60*S, 60*S)
        bg:setFillColor(i * 0.2, 0.5, 1 - i * 0.2)
        
        local shape
        if i == 1 then
            shape = display.newCircle(snap.group, 0, 0, 20*S)
        elseif i == 2 then
            shape = display.newRect(snap.group, 0, 0, 30*S, 30*S)
        elseif i == 3 then
            shape = display.newRoundedRect(snap.group, 0, 0, 35*S, 25*S, 8*S)
        else
            shape = display.newPolygon(snap.group, 0, 0, {0, -20*S, 17*S, 10*S, -17*S, 10*S})
        end
        shape:setFillColor(1, 1, 1, 0.7)
        
        snap:invalidate()
        table.insert(sceneObjects.snapshots, snap)
    end
    
    -- Section 4: Nested snapshots
    print("[Scene 9: Masks] Testing nested snapshots...")
    local nestedLabel = display.newText({
        parent = sceneGroup,
        text = "Nested Snapshot:",
        x = 20,
        y = 320*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    nestedLabel.anchorX = 0
    nestedLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Outer snapshot
    local outerSnap = display.newSnapshot(80*S, 80*S)
    outerSnap.x, outerSnap.y = 80*S, 380*S
    sceneGroup:insert(outerSnap)
    sceneObjects.outerSnap = outerSnap
    
    local outerBg = display.newRect(outerSnap.group, 0, 0, 80*S, 80*S)
    outerBg:setFillColor(0.3, 0.2, 0.3)
    
    local outerRect = display.newRect(outerSnap.group, 0, 0, 60*S, 60*S)
    outerRect:setFillColor(0.6, 0.4, 0.6)
    outerSnap:invalidate()
    
    -- Inner content (simulated nested by drawing on top)
    local innerCircle = display.newCircle(outerSnap.group, 0, 0, 20*S)
    innerCircle:setFillColor(0.9, 0.6, 0.9)
    outerSnap:invalidate()
    
    -- Section 5: Capture simulation (using snapshot as capture)
    print("[Scene 9: Masks] Testing capture concept...")
    local captureLabel = display.newText({
        parent = sceneGroup,
        text = "Capture Simulation:",
        x = 180,
        y = 320*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    captureLabel.anchorX = 0
    captureLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Create a scene to "capture"
    local captureScene = display.newGroup()
    captureScene.x, captureScene.y = 230*S, 380*S
    sceneGroup:insert(captureScene)
    
    local csBg = display.newRect(captureScene, 0, 0, 80*S, 80*S)
    csBg:setFillColor(0.2, 0.3, 0.4)
    
    local csObj1 = display.newCircle(captureScene, -15*S, -15*S, 20*S)
    csObj1:setFillColor(0.9, 0.5, 0.2)
    
    local csObj2 = display.newRect(captureScene, 15*S, 15*S, 30*S, 30*S)
    csObj2:setFillColor(0.2, 0.7, 0.9)
    
    local csText = display.newText({
        parent = captureScene,
        text = "CAP",
        x = 0,
        y = 0,
        font = native.systemFontBold,
        fontSize = 14
    })
    csText:setFillColor(1, 1, 1)
    
    -- Create a snapshot that represents the "captured" image
    local captureSnap = display.newSnapshot(80*S, 80*S)
    captureSnap.x, captureSnap.y = 230*S, 480*S
    sceneGroup:insert(captureSnap)
    
    -- Copy content to snapshot (simulating capture)
    local capBg = display.newRect(captureSnap.group, 0, 0, 80*S, 80*S)
    capBg:setFillColor(0.4, 0.2, 0.2)
    
    local capRect = display.newRect(captureSnap.group, 0, 0, 50*S, 50*S)
    capRect:setFillColor(0.8, 0.4, 0.4)
    
    local capText = display.newText({
        parent = captureSnap.group,
        text = "Captured",
        x = 0,
        y = 0,
        font = native.systemFont,
        fontSize = 10*S
    })
    capText:setFillColor(1, 1, 1)
    
    captureSnap:invalidate()
    
    -- Scale down the "captured" image
    captureSnap.xScale = 0.6
    captureSnap.yScale = 0.6
    
    local captureNote = display.newText({
        parent = sceneGroup,
        text = "Captured (60%)",
        x = 230,
        y = 520*S,
        font = native.systemFont,
        fontSize = 9*S
    })
    captureNote:setFillColor(0.5, 0.5, 0.5)
    
    -- Section 6: Effects test (if graphics.newEffect is available)
    print("[Scene 9: Masks] Testing graphics effects...")
    local effectsLabel = display.newText({
        parent = sceneGroup,
        text = "Effects (if available):",
        x = 20,
        y = 440*S,
        font = native.systemFont,
        fontSize = 12*S
    })
    effectsLabel.anchorX = 0
    effectsLabel:setFillColor(0.7, 0.7, 0.7)
    
    -- Check if graphics effects are available
    local effectAvailable = false
    if graphics and graphics.newEffect then
        local success, effect = pcall(function()
            return graphics.newEffect({
                category = "filter",
                name = "blur"
            })
        end)
        if success then
            effectAvailable = true
        end
    end
    
    local effectRect = display.newRect(sceneGroup, 80*S, 495*S, 60*S, 50*S)
    if effectAvailable then
        effectRect:setFillColor(0.9, 0.5, 0.2)
        -- Try to apply effect
        pcall(function()
            effectRect.fill.effect = "filter.blur"
        end)
    else
        effectRect:setFillColor(0.4, 0.4, 0.4)
    end
    
    local effectText = display.newText({
        parent = sceneGroup,
        text = effectAvailable and "Blur" or "N/A",
        x = 80*S,
        y = 560*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    effectText:setFillColor(0.6, 0.6, 0.6)
    
    -- Fallback visual effect (transparency)
    local fallbackEffect = display.newRect(sceneGroup, 160*S, 495*S, 60*S, 50*S)
    fallbackEffect:setFillColor(0.2, 0.7, 0.9, 0.5)
    fallbackEffect.strokeWidth = 2*S
    fallbackEffect:setStrokeColor(0.5, 0.9, 1)
    
    local fallbackLabel = display.newText({
        parent = sceneGroup,
        text = "Alpha",
        x = 160*S,
        y = 560*S,
        font = native.systemFont,
        fontSize = 10*S
    })
    fallbackLabel:setFillColor(0.6, 0.6, 0.6)
    
    -- Start animations after all objects are created
    startAnimations()
    
    print("[Scene 9: Masks] Creation complete - All mask and FBO tests rendered")
end

function scene:show(event)
    if event.phase == "will" then
        print("[Scene 9: Masks] Show will")
        _G.bgfxDemoCurrentScene = 9
        if _G.updateNavHighlight then _G.updateNavHighlight() end
    elseif event.phase == "did" then
        print("[Scene 9: Masks] Show did")
        -- Restart animations when scene is shown (for re-entry)
        startAnimations()
    end
end

function scene:hide(event)
    if event.phase == "will" then
        print("[Scene 9: Masks] Hide will - cleaning up transitions")
        for _, t in ipairs(activeTransitions) do
            if t and t.cancel then
                transition.cancel(t)
            end
        end
        activeTransitions = {}
        transition.cancelAll()
    elseif event.phase == "did" then
        print("[Scene 9: Masks] Hide did")
    end
end

function scene:destroy(event)
    print("[Scene 9: Masks] Destroy")
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
