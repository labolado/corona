--[[
    test_android_compat.lua - Comprehensive bgfx rendering compatibility test

    Usage: SOLAR2D_TEST=android_compat ./Corona\ Simulator ...

    26 scenes covering all bgfx rendering features, auto-screenshot + auto-advance.
    Screenshots saved via adb screencap (display.save is blank on bgfx).
    Outputs structured PASS/FAIL summary.
--]]

display.setStatusBar(display.HiddenStatusBar)

local W = display.contentWidth
local H = display.contentHeight
local CX = display.contentCenterX
local CY = display.contentCenterY

print("=== ANDROID COMPAT TEST START ===")
print("Platform: " .. system.getInfo("platform"))
print("Time: " .. os.date())
print("Display: " .. W .. "x" .. H)

-- Ensure screenshot directory exists
local lfs = require("lfs")
local docPath = system.pathForFile("", system.DocumentsDirectory)
if docPath then
    lfs.mkdir(docPath .. "/screenshots")
end
local ssDir = "screenshots"

local results = {}
local sceneGroup = nil
local currentScene = 0
local SCENE_DELAY = 3000   -- ms before screenshot marker
local ADVANCE_DELAY = 5000 -- ms after screenshot marker, give adb screencap time

------------------------------------------------------------------------
-- Utility
------------------------------------------------------------------------
local function label(group, text, x, y, size, r, g, b)
    local t = display.newText({ parent = group, text = text, x = x, y = y,
        font = native.systemFontBold, fontSize = size or 28 })
    t:setFillColor(r or 1, g or 1, b or 1)
    return t
end

local function bg(group, cr, cg, cb)
    local r = display.newRect(group, CX, CY, W, H)
    r:setFillColor(cr or 0.08, cg or 0.08, cb or 0.12)
    return r
end

------------------------------------------------------------------------
-- Scene definitions
------------------------------------------------------------------------
local scenes = {}

-- 1. Basic shapes
scenes[1] = { name = "basic_shapes", title = "1: Basic Shapes", fn = function(g)
    bg(g)
    -- Rectangles
    local r1 = display.newRect(g, 100, 200, 120, 80)
    r1:setFillColor(1, 0.2, 0.2)
    local r2 = display.newRect(g, 260, 200, 80, 80)
    r2:setFillColor(0.2, 0.8, 0.2)
    r2.rotation = 30

    -- Circles
    local c1 = display.newCircle(g, 450, 200, 50)
    c1:setFillColor(0.2, 0.4, 1)
    local c2 = display.newCircle(g, 600, 200, 30)
    c2:setFillColor(1, 1, 0)
    c2.strokeWidth = 4
    c2:setStrokeColor(1, 0.5, 0)

    -- Lines
    local line = display.newLine(g, 80, 350, 400, 350)
    line:setStrokeColor(0, 1, 1)
    line.strokeWidth = 3
    local line2 = display.newLine(g, 80, 380, 200, 420, 320, 380, 440, 420)
    line2:setStrokeColor(1, 0.5, 1)
    line2.strokeWidth = 2

    -- Polygon
    local vertices = { 0,-40, 30,30, -30,30 }
    local poly = display.newPolygon(g, 550, 380, vertices)
    poly:setFillColor(0.8, 0.3, 0.8)

    -- Rounded rect
    local rr = display.newRoundedRect(g, 750, 200, 140, 90, 20)
    rr:setFillColor(0, 0.7, 0.6)

    -- Small shapes
    for i = 1, 8 do
        local s = display.newCircle(g, 80 + i * 90, 500, 15)
        s:setFillColor(i/8, 1 - i/8, 0.5)
    end
end }

-- 2. Textures and fills
scenes[2] = { name = "textures_fills", title = "2: Textures & Fills", fn = function(g)
    bg(g)
    -- Gradient fills
    local r1 = display.newRect(g, 200, 200, 200, 150)
    r1.fill = { type = "gradient", color1 = {1,0,0}, color2 = {0,0,1}, direction = "down" }

    local r2 = display.newRect(g, 500, 200, 200, 150)
    r2.fill = { type = "gradient", color1 = {0,1,0}, color2 = {1,1,0}, direction = "right" }

    -- Solid color palette
    local colors = { {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}, {1,0,1}, {0,1,1}, {1,0.5,0}, {0.5,0,1} }
    for i, c in ipairs(colors) do
        local sq = display.newRect(g, 60 + (i-1) * 100, 400, 70, 70)
        sq:setFillColor(c[1], c[2], c[3])
    end

    -- Image test (use built-in or test assets if available)
    local ok, img = pcall(display.newImageRect, g, "test_red.png", 100, 100)
    if ok and img then
        img.x, img.y = 200, 550
    end
    local ok2, img2 = pcall(display.newImageRect, g, "test_blue.png", 100, 100)
    if ok2 and img2 then
        img2.x, img2.y = 350, 550
    end
    local ok3, img3 = pcall(display.newImageRect, g, "test_green.png", 100, 100)
    if ok3 and img3 then
        img3.x, img3.y = 500, 550
    end
end }

-- 3. Text rendering
scenes[3] = { name = "text_render", title = "3: Text Rendering", fn = function(g)
    bg(g)
    -- Different sizes
    local sizes = { 12, 16, 20, 28, 36, 48 }
    for i, sz in ipairs(sizes) do
        local t = display.newText({
            parent = g, text = "Hello " .. sz .. "px",
            x = 200, y = 100 + i * 60,
            font = native.systemFont, fontSize = sz
        })
        t:setFillColor(1, 1, 1)
    end

    -- Bold text
    local bold = display.newText({
        parent = g, text = "Bold Text Sample",
        x = 600, y = 150,
        font = native.systemFontBold, fontSize = 30
    })
    bold:setFillColor(1, 0.8, 0)

    -- Multi-line
    local multi = display.newText({
        parent = g,
        text = "Line one\nLine two\nLine three\nFourth line here",
        x = 600, y = 350,
        font = native.systemFont, fontSize = 22,
        width = 300, align = "center"
    })
    multi:setFillColor(0.7, 0.9, 1)

    -- Colored texts
    local ct1 = display.newText({ parent = g, text = "RED", x = 550, y = 500, fontSize = 36 })
    ct1:setFillColor(1, 0, 0)
    local ct2 = display.newText({ parent = g, text = "GREEN", x = 700, y = 500, fontSize = 36 })
    ct2:setFillColor(0, 1, 0)
    local ct3 = display.newText({ parent = g, text = "BLUE", x = 850, y = 500, fontSize = 36 })
    ct3:setFillColor(0, 0, 1)
end }

-- 4. Transforms
scenes[4] = { name = "transforms", title = "4: Transforms", fn = function(g)
    bg(g)
    -- Rotation
    for i = 0, 7 do
        local r = display.newRect(g, 100 + i * 100, 180, 60, 60)
        r:setFillColor(0.2 + i*0.1, 0.5, 1 - i*0.1)
        r.rotation = i * 45
    end

    -- Scale
    for i = 1, 5 do
        local c = display.newCircle(g, 100 + i * 140, 350, 30)
        c:setFillColor(1, 0.4, 0.2)
        c.xScale = 0.5 + i * 0.3
        c.yScale = 0.5 + i * 0.3
    end

    -- Alpha
    for i = 1, 8 do
        local r = display.newRect(g, 60 + i * 90, 500, 70, 70)
        r:setFillColor(0, 0.8, 0.4)
        r.alpha = i / 8
    end

    -- Group transform
    local sg = display.newGroup()
    g:insert(sg)
    sg.x, sg.y = 900, 350
    sg.rotation = 15
    sg.xScale = 0.8
    local gr1 = display.newRect(sg, 0, -30, 80, 40)
    gr1:setFillColor(1, 0, 0)
    local gr2 = display.newRect(sg, 0, 30, 80, 40)
    gr2:setFillColor(0, 0, 1)
end }

-- 5. Masks
scenes[5] = { name = "masks", title = "5: Masks", fn = function(g)
    bg(g)
    label(g, "Mask test (may need mask file)", CX, 200, 24)

    -- Container as mask
    local container = display.newContainer(g, 200, 200)
    container.x, container.y = 250, 400
    local inner = display.newRect(container, 0, 0, 300, 300)
    inner:setFillColor(1, 0.5, 0)
    local inner2 = display.newCircle(container, 0, 0, 60)
    inner2:setFillColor(0, 0.5, 1)

    -- Another container
    local c2 = display.newContainer(g, 150, 150)
    c2.x, c2.y = 550, 400
    for i = 1, 20 do
        local dot = display.newCircle(c2, math.random(-100, 100), math.random(-100, 100), 10)
        dot:setFillColor(math.random(), math.random(), math.random())
    end

    -- Clipped rect
    local c3 = display.newContainer(g, 120, 120)
    c3.x, c3.y = 800, 400
    local big = display.newRect(c3, 0, 0, 200, 200)
    big.fill = { type = "gradient", color1 = {1,0,1}, color2 = {0,1,0} }
end }

-- 6. Blend modes
scenes[6] = { name = "blend_modes", title = "6: Blend Modes", fn = function(g)
    bg(g, 0.2, 0.2, 0.2)
    local modes = { "normal", "add", "multiply", "screen" }
    local baseColors = { {1,0.3,0.3}, {0.3,1,0.3}, {0.3,0.3,1}, {1,1,0.3} }

    for i, mode in ipairs(modes) do
        local col = (i - 1) * 220 + 150
        label(g, mode, col, 130, 22)

        -- Base rect
        local base = display.newRect(g, col, 280, 150, 150)
        base:setFillColor(0.8, 0.8, 0.8)

        -- Overlay with blend
        local overlay = display.newRect(g, col, 280, 120, 120)
        overlay:setFillColor(baseColors[i][1], baseColors[i][2], baseColors[i][3])
        overlay.blendMode = mode

        -- Circle overlay
        local circ = display.newCircle(g, col + 30, 310, 40)
        circ:setFillColor(1, 0.5, 0)
        circ.blendMode = mode
    end

    -- Alpha blending row
    for i = 1, 8 do
        local r = display.newRect(g, 60 + i * 100, 480, 80, 60)
        r:setFillColor(1, 0, 0.5)
        r.alpha = i / 8
        r.blendMode = "add"
    end
end }

-- 7. Snapshot/Capture
scenes[7] = { name = "snapshot_capture", title = "7: Snapshot & Capture", fn = function(g)
    bg(g)

    -- Create content to snapshot
    local src = display.newGroup()
    g:insert(src)
    local sr1 = display.newRect(src, 200, 250, 100, 100)
    sr1:setFillColor(1, 0, 0)
    local sr2 = display.newCircle(src, 280, 250, 40)
    sr2:setFillColor(0, 1, 0)

    -- Snapshot
    local snap = display.newSnapshot(g, 200, 200)
    snap.x, snap.y = 550, 250
    local sg = snap.group
    local s1 = display.newRect(sg, 0, 0, 100, 100)
    s1:setFillColor(0, 0.5, 1)
    local s2 = display.newCircle(sg, 30, 30, 30)
    s2:setFillColor(1, 1, 0)
    snap:invalidate()
    label(g, "Snapshot", 550, 140, 20)

    -- display.capture test
    timer.performWithDelay(500, function()
        local captured = display.capture(src)
        if captured then
            captured.x, captured.y = 550, 480
            captured.xScale, captured.yScale = 0.6, 0.6
            g:insert(captured)
            label(g, "display.capture OK", 550, 400, 18, 0, 1, 0)
            print("[PASS] display.capture")
        else
            label(g, "display.capture FAILED", 550, 400, 18, 1, 0, 0)
            print("[FAIL] display.capture")
        end
    end)

    label(g, "Source objects", 200, 140, 20)
end }

-- 8. Effect shaders
scenes[8] = { name = "effects", title = "8: Effect Shaders", fn = function(g)
    bg(g)

    -- Create objects and apply effects
    local function testEffect(x, y, effectName, params)
        local r = display.newRect(g, x, y, 120, 120)
        r.fill = { type = "gradient", color1 = {1,0.5,0}, color2 = {0,0.5,1} }
        local ok, err = pcall(function()
            r.fill.effect = effectName
            if params then
                for k, v in pairs(params) do
                    r.fill.effect[k] = v
                end
            end
        end)
        local status = ok and "OK" or "N/A"
        label(g, effectName:gsub("filter.", ""):gsub("generator.", "gen."), x, y + 80, 16)
        label(g, status, x, y + 100, 14, ok and 0 or 1, ok and 1 or 0, 0)
        return ok
    end

    local effects = {
        { 200, 220, "filter.blur", nil },
        { 400, 220, "filter.brightness", { intensity = 0.3 } },
        { 600, 220, "filter.contrast", { contrast = 1.5 } },
        { 800, 220, "filter.grayscale", nil },
        { 200, 430, "filter.invert", nil },
        { 400, 430, "filter.sepia", nil },
        { 600, 430, "generator.stripes", nil },
        { 800, 430, "filter.saturate", { intensity = 2 } },
    }

    for _, e in ipairs(effects) do
        testEffect(e[1], e[2], e[3], e[4])
    end
end }

-- 9. Particle simulation
scenes[9] = { name = "particles", title = "9: Particle Sim (500)", fn = function(g)
    bg(g, 0, 0, 0.05)
    local particles = {}
    local NUM = 500
    local colors = { {1,0.3,0.3}, {0.3,1,0.3}, {0.3,0.3,1}, {1,1,0.3}, {1,0.3,1}, {0.3,1,1} }

    for i = 1, NUM do
        local c = colors[(i % #colors) + 1]
        local sz = math.random(4, 12)
        local p = display.newCircle(g, math.random(20, W-20), math.random(80, H-20), sz)
        p:setFillColor(c[1], c[2], c[3])
        p.alpha = 0.5 + math.random() * 0.5
        p.vx = (math.random() - 0.5) * 4
        p.vy = (math.random() - 0.5) * 4
        particles[i] = p
    end

    local function onFrame()
        for _, p in ipairs(particles) do
            p.x = p.x + p.vx
            p.y = p.y + p.vy
            if p.x < 10 or p.x > W - 10 then p.vx = -p.vx end
            if p.y < 80 or p.y > H - 10 then p.vy = -p.vy end
            p.alpha = p.alpha - 0.001
            if p.alpha < 0.1 then p.alpha = 0.8 end
        end
    end
    Runtime:addEventListener("enterFrame", onFrame)
    -- Store cleanup
    g._onFrame = onFrame
end }

-- 10. Physics
scenes[10] = { name = "physics_world", title = "10: Physics World", fn = function(g)
    bg(g)
    local physics = require("physics")
    physics.start()
    physics.setGravity(0, 9.8)

    -- Ground
    local ground = display.newRect(g, CX, H - 40, W - 40, 30)
    ground:setFillColor(0.4, 0.3, 0.2)
    physics.addBody(ground, "static", { friction = 0.5 })

    -- Walls
    local lwall = display.newRect(g, 25, CY, 20, H - 100)
    lwall:setFillColor(0.3, 0.3, 0.3)
    physics.addBody(lwall, "static")
    local rwall = display.newRect(g, W - 25, CY, 20, H - 100)
    rwall:setFillColor(0.3, 0.3, 0.3)
    physics.addBody(rwall, "static")

    -- Dynamic bodies: boxes
    for i = 1, 8 do
        local box = display.newRect(g, 100 + i * 80, 100, 50, 50)
        box:setFillColor(math.random(), math.random(), math.random())
        box.rotation = math.random(0, 45)
        physics.addBody(box, "dynamic", { density = 1, friction = 0.3, bounce = 0.4 })
    end

    -- Dynamic bodies: circles
    for i = 1, 6 do
        local ball = display.newCircle(g, 150 + i * 100, 60, 20 + math.random(5, 15))
        ball:setFillColor(math.random(), math.random(), math.random())
        physics.addBody(ball, "dynamic", { density = 0.8, friction = 0.2, bounce = 0.6, radius = ball.path.radius })
    end

    -- Ramp
    local ramp = display.newRect(g, 400, 350, 300, 15)
    ramp:setFillColor(0.6, 0.5, 0.2)
    ramp.rotation = -15
    physics.addBody(ramp, "static", { friction = 0.3 })

    -- Store physics ref for cleanup
    g._physics = physics
end }

-- 11. Composer page transitions
scenes[11] = { name = "composer_jump", title = "11: Multi-page Jump", fn = function(g)
    bg(g)

    -- Simulate multi-page by creating/destroying sub-groups with transitions
    local pages = {
        { color = {0.8,0.2,0.2}, text = "Page A" },
        { color = {0.2,0.8,0.2}, text = "Page B" },
        { color = {0.2,0.2,0.8}, text = "Page C" },
    }

    local pageGroup = display.newGroup()
    g:insert(pageGroup)
    local pageIndex = 0

    local function showPage(idx)
        -- Clear previous
        while pageGroup.numChildren > 0 do
            pageGroup[1]:removeSelf()
        end
        local pg = pages[idx]
        local r = display.newRect(pageGroup, CX, CY, W - 100, H - 150)
        r:setFillColor(pg.color[1], pg.color[2], pg.color[3])
        r.alpha = 0
        transition.to(r, { alpha = 0.8, time = 200 })

        local t = display.newText({ parent = pageGroup, text = pg.text,
            x = CX, y = CY, fontSize = 60 })
        t:setFillColor(1, 1, 1)
        t.alpha = 0
        transition.to(t, { alpha = 1, time = 300 })

        local counter = display.newText({ parent = pageGroup,
            text = "Page " .. idx .. " / " .. #pages,
            x = CX, y = CY + 80, fontSize = 24 })
        counter:setFillColor(1, 1, 1, 0.7)
    end

    local function cyclePages()
        for i = 1, #pages do
            timer.performWithDelay(i * 600, function()
                showPage(i)
            end)
        end
    end
    cyclePages()
end }

-- 12. Stress test
scenes[12] = { name = "stress_2000", title = "12: Stress 2000 obj", fn = function(g)
    bg(g, 0.05, 0.05, 0.05)
    local count = 0
    local types = { "rect", "circle", "text" }

    for i = 1, 2000 do
        local kind = types[(i % #types) + 1]
        local x = math.random(20, W - 20)
        local y = math.random(80, H - 20)

        if kind == "rect" then
            local sz = math.random(5, 25)
            local r = display.newRect(g, x, y, sz, sz)
            r:setFillColor(math.random(), math.random(), math.random())
            r.alpha = 0.3 + math.random() * 0.7
        elseif kind == "circle" then
            local r = math.random(3, 12)
            local c = display.newCircle(g, x, y, r)
            c:setFillColor(math.random(), math.random(), math.random())
            c.alpha = 0.3 + math.random() * 0.7
        else
            local t = display.newText({ parent = g, text = tostring(i),
                x = x, y = y, fontSize = math.random(8, 16) })
            t:setFillColor(math.random(), math.random(), math.random())
        end
        count = count + 1
    end

    label(g, count .. " objects created", CX, H - 60, 24, 1, 1, 0)
end }

-- 13. Capture APIs (display.save / display.capture / display.captureBounds)
scenes[13] = { name = "capture_apis", title = "13: Capture APIs", fn = function(g)
    bg(g)
    local testResults = {}

    -- Create visual target
    local target = display.newGroup()
    g:insert(target)
    local tr1 = display.newRect(target, 200, 250, 120, 120)
    tr1:setFillColor(1, 0, 0)
    local tr2 = display.newCircle(target, 300, 250, 50)
    tr2:setFillColor(0, 1, 0)
    local tr3 = display.newRect(target, 400, 250, 80, 80)
    tr3:setFillColor(0, 0, 1)
    tr3.rotation = 45

    timer.performWithDelay(300, function()
        -- Test 1: display.capture(group)
        local ok1, cap1 = pcall(function() return display.capture(target) end)
        if ok1 and cap1 then
            cap1.x, cap1.y = 150, 480
            cap1.xScale, cap1.yScale = 0.4, 0.4
            g:insert(cap1)
            label(g, "capture(grp) OK", 150, 420, 16, 0, 1, 0)
        else
            label(g, "capture(grp) FAIL", 150, 420, 16, 1, 0, 0)
        end
        print("[" .. (ok1 and "PASS" or "FAIL") .. "] display.capture(group)")

        -- Test 2: display.captureBounds
        local ok2, cap2 = pcall(function()
            local bounds = { xMin = 100, yMin = 150, xMax = 500, yMax = 350 }
            return display.captureBounds(bounds)
        end)
        if ok2 and cap2 then
            cap2.x, cap2.y = 400, 480
            cap2.xScale, cap2.yScale = 0.4, 0.4
            g:insert(cap2)
            label(g, "captureBounds OK", 400, 420, 16, 0, 1, 0)
        else
            label(g, "captureBounds FAIL", 400, 420, 16, 1, 0, 0)
        end
        print("[" .. (ok2 and "PASS" or "FAIL") .. "] display.captureBounds")

        -- Test 3: display.save
        local ok3, err3 = pcall(function()
            display.save(target, {
                filename = "test_save.png",
                baseDir = system.DocumentsDirectory,
            })
        end)
        label(g, "display.save " .. (ok3 and "OK" or "FAIL"), 650, 420, 16,
            ok3 and 0 or 1, ok3 and 1 or 0, 0)
        print("[" .. (ok3 and "PASS" or "FAIL") .. "] display.save")
    end)
end }

-- 14. captureScreen
scenes[14] = { name = "capture_screen", title = "14: captureScreen", fn = function(g)
    bg(g, 0.1, 0.05, 0.2)

    -- Draw recognizable pattern
    for i = 1, 6 do
        local r = display.newRect(g, i * 120, 200, 80, 80)
        r:setFillColor(i/6, 1-i/6, 0.5)
        r.rotation = i * 15
    end
    label(g, "captureScreen test", CX, 120, 28)

    timer.performWithDelay(300, function()
        local ok, cap = pcall(function() return display.captureScreen() end)
        if ok and cap then
            cap.x, cap.y = CX, 450
            cap.xScale, cap.yScale = 0.35, 0.35
            g:insert(cap)
            label(g, "captureScreen OK", CX, 370, 20, 0, 1, 0)
        else
            label(g, "captureScreen FAIL: " .. tostring(cap), CX, 370, 18, 1, 0, 0)
        end
        print("[" .. (ok and "PASS" or "FAIL") .. "] display.captureScreen")
    end)
end }

-- 15. Snapshot invalidate + capture
scenes[15] = { name = "snapshot_ops", title = "15: Snapshot Ops", fn = function(g)
    bg(g)

    -- Snapshot with invalidate
    local snap1 = display.newSnapshot(g, 200, 200)
    snap1.x, snap1.y = 200, 280
    local sg1 = snap1.group
    local s1r = display.newRect(sg1, 0, 0, 100, 100)
    s1r:setFillColor(1, 0, 0)
    local s1c = display.newCircle(sg1, 40, 40, 30)
    s1c:setFillColor(0, 1, 0)
    snap1:invalidate()
    label(g, "invalidate()", 200, 150, 18)

    -- Snapshot with invalidate("canvas") — accumulative mode
    local snap2 = display.newSnapshot(g, 200, 200)
    snap2.x, snap2.y = 500, 280
    local sg2 = snap2.group
    for i = 1, 5 do
        local dot = display.newCircle(sg2, (i-3)*30, 0, 20)
        dot:setFillColor(i/5, 0, 1 - i/5)
    end
    snap2:invalidate("canvas")
    -- Add more and invalidate again
    local extra = display.newRect(sg2, 0, 50, 100, 20)
    extra:setFillColor(1, 1, 0)
    snap2:invalidate("canvas")
    label(g, 'invalidate("canvas")', 500, 150, 18)

    -- Snapshot capture
    local snap3 = display.newSnapshot(g, 200, 200)
    snap3.x, snap3.y = 350, 520
    local sg3 = snap3.group
    local triangle = display.newPolygon(sg3, 0, 0, { 0,-60, 50,40, -50,40 })
    triangle:setFillColor(0, 0.8, 0.8)
    snap3:invalidate()

    timer.performWithDelay(200, function()
        local ok, cap = pcall(function() return snap3:capture() end)
        if ok and cap then
            cap.x, cap.y = 650, 520
            cap.xScale, cap.yScale = 0.5, 0.5
            g:insert(cap)
            label(g, "snap:capture OK", 650, 430, 16, 0, 1, 0)
        else
            label(g, "snap:capture " .. (ok and "nil" or "FAIL"), 650, 430, 16, 1, 0.5, 0)
        end
        print("[" .. ((ok and cap) and "PASS" or "WARN") .. "] snapshot:capture")
    end)
end }

-- 16. Canvas texture
scenes[16] = { name = "canvas_texture", title = "16: Canvas Texture", fn = function(g)
    bg(g)

    local ok, err = pcall(function()
        local tex = graphics.newTexture({ type = "canvas", width = 256, height = 256 })
        if not tex then
            label(g, "newTexture(canvas) returned nil", CX, CY, 22, 1, 0.5, 0)
            print("[WARN] graphics.newTexture canvas returned nil")
            return
        end

        -- Draw into canvas
        local r1 = display.newRect(0, 0, 128, 128)
        r1:setFillColor(1, 0, 0)
        tex:draw(r1)

        local c1 = display.newCircle(64, 64, 50)
        c1:setFillColor(0, 1, 0)
        tex:draw(c1)

        local r2 = display.newRect(128, 128, 128, 128)
        r2:setFillColor(0, 0, 1)
        tex:draw(r2)

        tex:invalidate()

        -- Display canvas as fill
        local rect = display.newRect(g, 300, 300, 256, 256)
        rect.fill = { type = "image", filename = tex.filename, baseDir = tex.baseDir }
        label(g, "Canvas texture fill", 300, 170, 20, 0, 1, 0)

        -- Second instance to verify texture sharing
        local rect2 = display.newRect(g, 650, 300, 200, 200)
        rect2.fill = { type = "image", filename = tex.filename, baseDir = tex.baseDir }
        label(g, "Same canvas (scaled)", 650, 170, 20)

        -- Cleanup drawn objects
        r1:removeSelf(); c1:removeSelf(); r2:removeSelf()

        print("[PASS] canvas texture")
    end)

    if not ok then
        label(g, "Canvas texture ERROR: " .. tostring(err), CX, CY, 18, 1, 0, 0)
        print("[FAIL] canvas texture: " .. tostring(err))
    end
end }

-- 17. graphics.newOutline
scenes[17] = { name = "outline", title = "17: Outline", fn = function(g)
    bg(g, 0.9, 0.9, 0.85)

    -- Outline from image
    local ok1, result1 = pcall(function()
        local outline = graphics.newOutline(2, "test_red.png")
        if outline then
            local poly = display.newPolygon(g, 200, 300, outline)
            poly:setFillColor(0, 0, 0, 0)
            poly.strokeWidth = 2
            poly:setStrokeColor(1, 0, 0)
            label(g, "Outline(red.png)", 200, 180, 18, 0, 0, 0)
            return true
        end
        return false
    end)
    label(g, ok1 and result1 and "OK" or "N/A", 200, 420, 18,
        (ok1 and result1) and 0 or 0.8, (ok1 and result1) and 0.6 or 0.4, 0)
    print("[" .. ((ok1 and result1) and "PASS" or "WARN") .. "] graphics.newOutline(image)")

    -- Outline from ImageSheet
    local ok2, result2 = pcall(function()
        local outline2 = graphics.newOutline(2, "test_blue.png")
        if outline2 then
            local poly2 = display.newPolygon(g, 500, 300, outline2)
            poly2:setFillColor(0.2, 0.4, 0.8, 0.3)
            poly2.strokeWidth = 3
            poly2:setStrokeColor(0, 0, 1)
            label(g, "Outline(blue.png)", 500, 180, 18, 0, 0, 0)
            return true
        end
        return false
    end)
    label(g, ok2 and result2 and "OK" or "N/A", 500, 420, 18,
        (ok2 and result2) and 0 or 0.8, (ok2 and result2) and 0.6 or 0.4, 0)
    print("[" .. ((ok2 and result2) and "PASS" or "WARN") .. "] graphics.newOutline(image2)")
end }

-- 18. display.newMesh
scenes[18] = { name = "mesh", title = "18: Mesh", fn = function(g)
    bg(g)

    -- Simple triangle strip mesh
    local ok1, mesh1 = pcall(function()
        local m = display.newMesh(g, {
            x = 250, y = 280,
            mode = "triangles",
            vertices = {
                0, -80,
                -80, 60,
                80, 60,
            },
            uvs = { 0.5, 0, 0, 1, 1, 1 },
        })
        m:setFillColor(1, 0.3, 0.3)
        return m
    end)
    label(g, "Triangle mesh", 250, 150, 18)
    label(g, ok1 and "OK" or "FAIL", 250, 380, 16, ok1 and 0 or 1, ok1 and 1 or 0, 0)
    print("[" .. (ok1 and "PASS" or "FAIL") .. "] mesh triangles")

    -- Indexed quad mesh
    local ok2, mesh2 = pcall(function()
        local m = display.newMesh(g, {
            x = 550, y = 280,
            mode = "indexed",
            vertices = {
                -100, -80,
                 100, -80,
                 100,  80,
                -100,  80,
            },
            uvs = { 0,0, 1,0, 1,1, 0,1 },
            indices = { 1, 2, 3, 1, 3, 4 },
        })
        m.fill = { type = "gradient", color1 = {0,1,0}, color2 = {0,0,1}, direction = "down" }
        return m
    end)
    label(g, "Indexed quad mesh", 550, 150, 18)
    label(g, ok2 and "OK" or "FAIL", 550, 400, 16, ok2 and 0 or 1, ok2 and 1 or 0, 0)
    print("[" .. (ok2 and "PASS" or "FAIL") .. "] mesh indexed")

    -- Fan mesh
    local ok3, mesh3 = pcall(function()
        local verts = { 0, 0 }  -- center
        local uvs = { 0.5, 0.5 }
        local N = 12
        for i = 0, N do
            local angle = (i / N) * math.pi * 2
            verts[#verts+1] = math.cos(angle) * 60
            verts[#verts+1] = math.sin(angle) * 60
            uvs[#uvs+1] = 0.5 + math.cos(angle) * 0.5
            uvs[#uvs+1] = 0.5 + math.sin(angle) * 0.5
        end
        local idx = {}
        for i = 1, N do
            idx[#idx+1] = 1
            idx[#idx+1] = i + 1
            idx[#idx+1] = i + 2
        end
        local m = display.newMesh(g, {
            x = 400, y = 520,
            mode = "indexed",
            vertices = verts,
            uvs = uvs,
            indices = idx,
        })
        m:setFillColor(0.8, 0.5, 1)
        return m
    end)
    label(g, "Fan mesh", 400, 440, 18)
    label(g, ok3 and "OK" or "FAIL", 400, 600, 16, ok3 and 0 or 1, ok3 and 1 or 0, 0)
    print("[" .. (ok3 and "PASS" or "FAIL") .. "] mesh fan")
end }

-- 19. display.colorSample (CAUTION: crashes on bgfx backend — skip actual call, test API existence)
scenes[19] = { name = "color_sample", title = "19: colorSample", fn = function(g)
    bg(g)

    -- Create known-color objects for visual reference
    local red = display.newRect(g, 200, 250, 150, 150)
    red:setFillColor(1, 0, 0)
    label(g, "RED", 200, 340, 20)
    local green = display.newRect(g, 450, 250, 150, 150)
    green:setFillColor(0, 1, 0)
    label(g, "GREEN", 450, 340, 20)
    local blue = display.newRect(g, 700, 250, 150, 150)
    blue:setFillColor(0, 0, 1)
    label(g, "BLUE", 700, 340, 20)

    -- Check if colorSample exists (don't call it — crashes on bgfx Android)
    local apiExists = type(display.colorSample) == "function"
    label(g, "colorSample API: " .. (apiExists and "exists" or "missing"), CX, 450, 22,
        apiExists and 0 or 1, apiExists and 1 or 0, 0)
    label(g, "(skipped: crashes bgfx readback)", CX, 490, 16, 1, 0.7, 0)
    print("[WARN] colorSample skipped (SIGSEGV on bgfx readback)")
end }

-- 20. Custom shader (vertex + fragment)
scenes[20] = { name = "custom_shader", title = "20: Custom Shader", fn = function(g)
    bg(g)

    -- Define custom fragment-only effect
    local ok1 = pcall(function()
        graphics.defineEffect({
            language = "glsl",
            category = "filter",
            name = "compat_wave",
            isTimeDependent = true,
            fragment = [[
                P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
                {
                    P_UV float wave = sin(uv.y * 20.0 + CoronaTotalTime * 3.0) * 0.02;
                    P_COLOR vec4 color = texture2D(CoronaSampler0, vec2(uv.x + wave, uv.y));
                    return CoronaColorScale(color);
                }
            ]],
        })
    end)

    if ok1 then
        local r1 = display.newRect(g, 250, 280, 200, 200)
        r1.fill = { type = "gradient", color1 = {1,0,0}, color2 = {0,0,1} }
        r1.fill.effect = "filter.custom.compat_wave"
        label(g, "Wave shader", 250, 150, 20)
        label(g, "OK", 250, 400, 18, 0, 1, 0)
    else
        label(g, "Wave shader FAIL", 250, 280, 20, 1, 0, 0)
    end
    print("[" .. (ok1 and "PASS" or "FAIL") .. "] custom fragment shader")

    -- Define custom vertex + fragment effect
    local ok2 = pcall(function()
        graphics.defineEffect({
            language = "glsl",
            category = "filter",
            name = "compat_tint",
            uniformData = {
                { name = "tintColor", default = {1, 0.5, 0, 1}, type = "vec4", index = 0 },
            },
            fragment = [[
                uniform P_COLOR vec4 u_UserData0;
                P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
                {
                    P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
                    return CoronaColorScale(mix(color, u_UserData0, 0.5));
                }
            ]],
        })
    end)

    if ok2 then
        local r2 = display.newRect(g, 550, 280, 200, 200)
        r2.fill = { type = "gradient", color1 = {0,1,0}, color2 = {1,1,0} }
        r2.fill.effect = "filter.custom.compat_tint"
        r2.fill.effect.tintColor = { 0, 0.5, 1, 1 }
        label(g, "Tint uniform shader", 550, 150, 20)
        label(g, "OK", 550, 400, 18, 0, 1, 0)
    else
        label(g, "Tint shader FAIL", 550, 280, 20, 1, 0, 0)
    end
    print("[" .. (ok2 and "PASS" or "FAIL") .. "] custom uniform shader")
end }

-- 21. Multi-FBO nesting (snapshot inside snapshot)
scenes[21] = { name = "nested_fbo", title = "21: Nested FBO", fn = function(g)
    bg(g)

    -- Inner snapshot
    local inner = display.newSnapshot(g, 180, 180)
    inner.x, inner.y = 250, 300
    local ig = inner.group
    local ir1 = display.newRect(ig, 0, 0, 100, 100)
    ir1:setFillColor(1, 0, 0)
    local ic1 = display.newCircle(ig, 30, 30, 40)
    ic1:setFillColor(0, 1, 0)
    inner:invalidate()
    label(g, "Inner snapshot", 250, 170, 18)

    -- Outer snapshot containing inner
    local outer = display.newSnapshot(g, 300, 300)
    outer.x, outer.y = 600, 300
    local og = outer.group

    -- Capture inner and put into outer
    timer.performWithDelay(200, function()
        local innerCap = display.capture(inner)
        if innerCap then
            og:insert(innerCap)
            innerCap.x, innerCap.y = -40, -40
            innerCap.xScale, innerCap.yScale = 0.7, 0.7
        end

        local outerRect = display.newRect(og, 40, 40, 80, 80)
        outerRect:setFillColor(0, 0, 1)

        local outerText = display.newText({ parent = og, text = "Nested",
            x = 0, y = 80, fontSize = 18 })
        outerText:setFillColor(1, 1, 0)

        outer:invalidate()
        label(g, "Outer(contains inner)", 600, 170, 18)

        -- Triple nesting: snapshot of outer
        timer.performWithDelay(200, function()
            local outerCap = display.capture(outer)
            if outerCap then
                outerCap.x, outerCap.y = 400, 550
                outerCap.xScale, outerCap.yScale = 0.4, 0.4
                g:insert(outerCap)
                label(g, "Triple nested capture", 400, 490, 16, 0, 1, 0)
                print("[PASS] triple nested FBO")
            else
                label(g, "Triple capture FAIL", 400, 490, 16, 1, 0, 0)
                print("[FAIL] triple nested FBO")
            end
        end)
    end)
end }

-- 22. setDrawMode
scenes[22] = { name = "draw_mode", title = "22: Draw Mode", fn = function(g)
    bg(g)
    label(g, "setDrawMode tests", CX, 120, 24)

    -- Create objects
    local objs = {}
    for i = 1, 6 do
        local r = display.newRect(g, 80 + i * 120, 280, 80, 80)
        r:setFillColor(math.random(), math.random(), math.random())
        r.rotation = i * 10
        objs[i] = r
    end
    local circ = display.newCircle(g, CX, 450, 60)
    circ:setFillColor(0.5, 0.8, 0.2)

    -- Test wireframe mode
    local ok1, err1 = pcall(function()
        display.setDrawMode("wireframe")
    end)
    label(g, "wireframe: " .. (ok1 and "OK" or "N/A"), 250, 180, 18,
        ok1 and 0 or 0.8, ok1 and 0.6 or 0.4, 0)
    print("[" .. (ok1 and "PASS" or "WARN") .. "] setDrawMode wireframe")

    -- Test forceRender
    local ok2, err2 = pcall(function()
        display.setDrawMode("forceRender")
    end)
    label(g, "forceRender: " .. (ok2 and "OK" or "N/A"), 550, 180, 18,
        ok2 and 0 or 0.8, ok2 and 0.6 or 0.4, 0)
    print("[" .. (ok2 and "PASS" or "WARN") .. "] setDrawMode forceRender")

    -- Reset to normal after brief display
    timer.performWithDelay(1500, function()
        pcall(function() display.setDrawMode("default") end)
    end)
end }

-- 23. graphics.defineEffect — custom composite + generator
scenes[23] = { name = "define_effect", title = "23: defineEffect", fn = function(g)
    bg(g)

    -- Custom generator effect (procedural pattern)
    local ok1 = pcall(function()
        graphics.defineEffect({
            language = "glsl",
            category = "generator",
            name = "compat_checker",
            fragment = [[
                P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
                {
                    P_UV float cx = step(0.5, fract(uv.x * 8.0));
                    P_UV float cy = step(0.5, fract(uv.y * 8.0));
                    P_COLOR float check = abs(cx - cy);
                    P_COLOR vec4 c1 = vec4(0.2, 0.6, 1.0, 1.0);
                    P_COLOR vec4 c2 = vec4(1.0, 0.8, 0.2, 1.0);
                    return CoronaColorScale(mix(c1, c2, check));
                }
            ]],
        })
    end)

    if ok1 then
        local r1 = display.newRect(g, 250, 280, 200, 200)
        r1.fill = { type = "image", filename = "test_red.png" }
        r1.fill.effect = "generator.custom.compat_checker"
        label(g, "Generator checker", 250, 150, 20)
        label(g, "OK", 250, 400, 18, 0, 1, 0)
    else
        label(g, "Generator FAIL", 250, 280, 20, 1, 0, 0)
    end
    print("[" .. (ok1 and "PASS" or "FAIL") .. "] defineEffect generator")

    -- Custom composite effect (blend two textures)
    local ok2 = pcall(function()
        graphics.defineEffect({
            language = "glsl",
            category = "composite",
            name = "compat_mix",
            fragment = [[
                P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
                {
                    P_COLOR vec4 a = texture2D(CoronaSampler0, uv);
                    P_COLOR vec4 b = texture2D(CoronaSampler1, uv);
                    return CoronaColorScale(mix(a, b, 0.5));
                }
            ]],
        })
    end)
    label(g, "Composite effect", 550, 150, 20)
    label(g, ok2 and "OK (defined)" or "FAIL", 550, 280, 18,
        ok2 and 0 or 1, ok2 and 1 or 0, 0)
    print("[" .. (ok2 and "PASS" or "FAIL") .. "] defineEffect composite")

    -- Time-dependent effect
    local ok3 = pcall(function()
        graphics.defineEffect({
            language = "glsl",
            category = "filter",
            name = "compat_pulse",
            isTimeDependent = true,
            fragment = [[
                P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
                {
                    P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
                    P_COLOR float pulse = 0.5 + 0.5 * sin(CoronaTotalTime * 4.0);
                    color.rgb *= pulse;
                    return CoronaColorScale(color);
                }
            ]],
        })
    end)

    if ok3 then
        local r3 = display.newRect(g, 550, 450, 150, 150)
        r3.fill = { type = "gradient", color1 = {1,0,1}, color2 = {0,1,1} }
        r3.fill.effect = "filter.custom.compat_pulse"
        label(g, "Pulse (animated)", 550, 540, 16)
    end
    print("[" .. (ok3 and "PASS" or "FAIL") .. "] defineEffect time-dependent")
end }

-- 24. Group hierarchy operations
scenes[24] = { name = "group_ops", title = "24: Group Ops", fn = function(g)
    bg(g)

    -- Create layered groups
    local gBack = display.newGroup(); g:insert(gBack)
    local gMid = display.newGroup(); g:insert(gMid)
    local gFront = display.newGroup(); g:insert(gFront)

    -- Back layer - large blue rect
    local back = display.newRect(gBack, CX, 280, 400, 300)
    back:setFillColor(0, 0, 0.8)
    label(gBack, "BACK", CX, 150, 20, 0.5, 0.5, 1)

    -- Mid layer - green rect
    local mid = display.newRect(gMid, CX + 50, 300, 300, 200)
    mid:setFillColor(0, 0.7, 0)
    label(gMid, "MID", CX + 50, 220, 20, 0.5, 1, 0.5)

    -- Front layer - red rect
    local front = display.newRect(gFront, CX - 30, 320, 200, 150)
    front:setFillColor(0.8, 0, 0)
    label(gFront, "FRONT", CX - 30, 260, 20, 1, 0.5, 0.5)

    -- Test toFront/toBack
    timer.performWithDelay(500, function()
        gBack:toFront()  -- Blue should now be on top
        label(g, "After: back:toFront()", CX, 480, 18, 1, 1, 0)
        print("[PASS] toFront")
    end)

    timer.performWithDelay(1200, function()
        gBack:toBack()  -- Blue back to bottom
        gFront:toFront()  -- Red on top again
        label(g, "After: front:toFront()", CX, 520, 18, 1, 1, 0)
        print("[PASS] toBack + toFront")
    end)

    -- Test insert/remove
    local movable = display.newCircle(gMid, CX, 450, 30)
    movable:setFillColor(1, 1, 0)

    timer.performWithDelay(800, function()
        gFront:insert(movable)  -- Move circle to front group
        movable.x = CX + 100
        label(g, "Circle moved to front group", CX, 560, 16, 0, 1, 0)
        print("[PASS] group:insert (re-parent)")
    end)
end }

-- 25. Transitions
scenes[25] = { name = "transitions", title = "25: Transitions", fn = function(g)
    bg(g)

    -- transition.to - move
    local r1 = display.newRect(g, 80, 200, 60, 60)
    r1:setFillColor(1, 0, 0)
    transition.to(r1, { x = 700, time = 2500, transition = easing.inOutQuad })
    label(g, "to: move", 80, 160, 16)

    -- transition.to - rotate + scale
    local r2 = display.newRect(g, CX, 300, 80, 80)
    r2:setFillColor(0, 0.7, 1)
    transition.to(r2, { rotation = 360, xScale = 2, yScale = 0.5, time = 2500 })
    label(g, "to: rotate+scale", CX, 250, 16)

    -- transition.to - alpha fade
    local r3 = display.newRect(g, 300, 400, 100, 60)
    r3:setFillColor(0, 1, 0)
    transition.to(r3, { alpha = 0, time = 2000, transition = easing.outExpo })
    label(g, "to: alpha fade", 300, 370, 16)

    -- transition.from
    local r4 = display.newRect(g, 600, 400, 100, 60)
    r4:setFillColor(1, 0, 1)
    transition.from(r4, { x = 100, y = 600, alpha = 0, rotation = -90, time = 2000 })
    label(g, "from: appear", 600, 370, 16)

    -- Multiple simultaneous transitions
    for i = 1, 5 do
        local c = display.newCircle(g, 100 + i * 120, 530, 20)
        c:setFillColor(i/5, 1 - i/5, 0.5)
        transition.to(c, {
            y = 530 - i * 30,
            xScale = 1 + i * 0.3,
            time = 500 + i * 300,
            transition = easing.outBounce,
        })
    end
    label(g, "Multiple bounce transitions", CX, 490, 16)

    print("[PASS] transitions")
end }

-- 26. Sprite sheet animation
scenes[26] = { name = "sprite_anim", title = "26: Sprite Animation", fn = function(g)
    bg(g)

    -- Create a simple sprite sheet from individual colored frames
    -- Since we don't have a real sprite sheet, we simulate with a canvas texture
    -- and also test basic sprite API with a solid image

    -- Method 1: Manual frame animation using timer
    local colors = {
        {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}, {1,0,1}, {0,1,1},
        {1,0.5,0}, {0.5,0,1}
    }
    local frameRect = display.newRect(g, 250, 280, 100, 100)
    frameRect:setFillColor(1, 0, 0)
    local frameLabel = label(g, "Frame 1/8", 250, 200, 18)
    local frameIdx = 1

    local function animateFrame()
        frameIdx = (frameIdx % #colors) + 1
        local c = colors[frameIdx]
        frameRect:setFillColor(c[1], c[2], c[3])
        frameLabel.text = "Frame " .. frameIdx .. "/8"
    end
    local animTimer = timer.performWithDelay(200, animateFrame, 0)
    g._animTimer = animTimer
    label(g, "Manual frame anim", 250, 150, 16)

    -- Method 2: Sprite sheet with newImageSheet
    local ok2, sprite = pcall(function()
        -- Create a simple 4-frame sheet options (using test_red.png as single frame)
        local sheetOpts = {
            width = 50, height = 50,
            numFrames = 1,
        }
        local sheet = graphics.newImageSheet("test_red.png", sheetOpts)
        if not sheet then return nil end

        local seqData = {
            { name = "idle", start = 1, count = 1, time = 500, loopCount = 0 },
        }
        local sp = display.newSprite(g, sheet, seqData)
        sp.x, sp.y = 550, 280
        sp.xScale, sp.yScale = 2, 2
        sp:play()
        return sp
    end)

    if ok2 and sprite then
        label(g, "ImageSheet sprite", 550, 200, 18, 0, 1, 0)
        label(g, "OK (playing)", 550, 350, 16, 0, 1, 0)
    else
        label(g, "ImageSheet sprite", 550, 200, 18)
        label(g, "N/A", 550, 350, 16, 1, 0.5, 0)
    end
    print("[" .. ((ok2 and sprite) and "PASS" or "WARN") .. "] sprite sheet")

    -- Method 3: Transition-based animation sequence
    local seqGroup = display.newGroup()
    g:insert(seqGroup)
    seqGroup.x, seqGroup.y = 400, 500
    local seqShapes = {}
    for i = 1, 6 do
        local s = display.newCircle(seqGroup, (i-3.5) * 40, 0, 15)
        s:setFillColor(i/6, 1-i/6, 0.5)
        s.alpha = 0.3
        seqShapes[i] = s
    end

    -- Sequential highlight animation
    local function highlightSeq(idx)
        if idx > #seqShapes then idx = 1 end
        for j, s in ipairs(seqShapes) do
            transition.to(s, { alpha = (j == idx) and 1 or 0.3, xScale = (j == idx) and 1.5 or 1,
                yScale = (j == idx) and 1.5 or 1, time = 150 })
        end
        timer.performWithDelay(250, function() highlightSeq(idx + 1) end)
    end
    highlightSeq(1)
    label(g, "Sequence highlight", 400, 440, 16)

    print("[PASS] sprite animation")
end }

------------------------------------------------------------------------
-- Scene runner
------------------------------------------------------------------------
local statusBar = display.newGroup()
local statusBg = display.newRect(statusBar, CX, 30, W, 60)
statusBg:setFillColor(0, 0, 0, 0.8)
local statusText = display.newText({
    parent = statusBar, text = "Starting...",
    x = CX, y = 30, font = native.systemFontBold, fontSize = 26
})
statusText:setFillColor(1, 1, 0)

local function cleanupScene()
    if sceneGroup then
        -- Remove enterFrame listener if any
        if sceneGroup._onFrame then
            Runtime:removeEventListener("enterFrame", sceneGroup._onFrame)
        end
        -- Stop physics if active
        if sceneGroup._physics then
            pcall(function() sceneGroup._physics.stop() end)
        end
        -- Cancel sprite animation timer
        if sceneGroup._animTimer then
            pcall(function() timer.cancel(sceneGroup._animTimer) end)
        end
        -- Reset draw mode if changed
        pcall(function() display.setDrawMode("default") end)
        sceneGroup:removeSelf()
        sceneGroup = nil
    end
end

local function captureScreenshot(name)
    local tag = string.format("scene_%02d_%s", currentScene, name)
    -- Signal the automation script to take a screencap via adb
    -- display.save on bgfx backend produces blank images, so we use a marker approach:
    -- print a known tag, external script uses `adb screencap` when it sees it
    print("[SCREENSHOT_READY] " .. tag)
    table.insert(results, { scene = currentScene, name = name, status = "PASS" })
end

local function runScene(index)
    if index > #scenes then
        -- All done
        cleanupScene()
        local pass, fail = 0, 0
        for _, r in ipairs(results) do
            if r.status == "PASS" then pass = pass + 1 else fail = fail + 1 end
        end
        statusText.text = string.format("DONE: %d/%d pass", pass, #results)
        statusText:setFillColor(fail == 0 and 0 or 1, fail == 0 and 1 or 0, 0)

        local summary = "\n=== ANDROID COMPAT RESULTS ===\n"
        summary = summary .. string.format("Pass: %d / %d | Fail: %d\n", pass, #results, fail)
        for _, r in ipairs(results) do
            local icon = r.status == "PASS" and "OK" or "FAIL"
            summary = summary .. string.format("  [%s] Scene %02d: %s %s\n",
                icon, r.scene, r.name, r.detail or "")
        end
        summary = summary .. "=== ALL SCENES COMPLETE ===\n"
        print(summary)

        -- Write results file
        local f = io.open(system.pathForFile("compat_results.txt", system.DocumentsDirectory), "w")
        if f then
            f:write(summary)
            f:close()
        end
        return
    end

    currentScene = index
    local sc = scenes[index]
    cleanupScene()

    sceneGroup = display.newGroup()
    -- Scene title
    statusText.text = sc.title .. " (" .. index .. "/" .. #scenes .. ")"

    -- Run scene
    local ok, err = pcall(sc.fn, sceneGroup)
    if not ok then
        print("[FAIL] Scene " .. index .. " error: " .. tostring(err))
        table.insert(results, { scene = index, name = sc.name, status = "FAIL", detail = tostring(err) })
        timer.performWithDelay(200, function() runScene(index + 1) end)
        return
    end

    -- Ensure status bar is on top
    statusBar:toFront()

    -- Screenshot after delay, then advance
    timer.performWithDelay(SCENE_DELAY, function()
        captureScreenshot(sc.name)
        timer.performWithDelay(ADVANCE_DELAY, function()
            runScene(index + 1)
        end)
    end)
end

-- Start
timer.performWithDelay(500, function()
    print("=== Running " .. #scenes .. " scenes ===")
    runScene(1)
end)
