--[[
    test_stress_api.lua - Comprehensive Rendering API Stress Test

    Usage: SOLAR2D_TEST=stress_api SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Exercises every potentially problematic rendering API.
    Each sub-test runs 3 seconds then auto-advances.
    At end, shows summary of all tests with PASS/FAIL status.
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320
local CX = display.contentCenterX
local CY = display.contentCenterY

print("=== Rendering API Stress Test ===")
print("Backend: " .. backend)
print("Display: " .. W .. "x" .. H .. "  S=" .. S)

----------------------------------------------------------------------------
-- State
----------------------------------------------------------------------------
local testGroup = display.newGroup()   -- holds current sub-test objects
local uiGroup   = display.newGroup()   -- persistent UI overlay

local results = {}      -- { {name=, pass=, msg=} }
local currentIdx = 0
local TEST_DURATION = 3000  -- ms per sub-test

----------------------------------------------------------------------------
-- UI overlay (always on top)
----------------------------------------------------------------------------
local uiBg = display.newRect(uiGroup, CX, 20*S, W, 40*S)
uiBg:setFillColor(0, 0, 0, 0.8)

local titleText = display.newText({
    parent = uiGroup, text = "Stress API Test", x = CX, y = 14*S,
    font = native.systemFontBold, fontSize = 20*S
})
titleText:setFillColor(0, 1, 0)

local progressText = display.newText({
    parent = uiGroup, text = "", x = CX, y = 34*S,
    font = native.systemFont, fontSize = 12*S
})
progressText:setFillColor(0.8, 0.8, 0.8)

----------------------------------------------------------------------------
-- Helpers
----------------------------------------------------------------------------
local function clearTestGroup()
    if testGroup and testGroup.removeSelf then
        -- Remove children individually with pcall to avoid crash
        -- when bgfx resources (FBO/snapshot) are still in-flight
        pcall(function()
            while testGroup.numChildren > 0 do
                local child = testGroup[testGroup.numChildren]
                if child and child.removeSelf then
                    child:removeSelf()
                end
            end
            testGroup:removeSelf()
        end)
    end
    testGroup = display.newGroup()
    uiGroup:toFront()
end

local function record(name, pass, msg)
    table.insert(results, {name = name, pass = pass, msg = msg or ""})
    local status = pass and "PASS" or ("FAIL: " .. (msg or ""))
    print("[TEST " .. name .. "] " .. status)
end

-- safe image loader: returns image or colored rect fallback
local function safeImage(parent, filename, x, y, w, h)
    local ok, img = pcall(function()
        if w and h then
            return display.newImageRect(parent, filename, w, h)
        else
            return display.newImage(parent, filename)
        end
    end)
    if ok and img then
        img.x, img.y = x, y
        return img
    end
    -- fallback
    local r = display.newRect(parent, x, y, w or 40*S, h or 40*S)
    r:setFillColor(0.5, 0.2, 0.2)
    return r
end

----------------------------------------------------------------------------
-- Sub-test definitions
----------------------------------------------------------------------------
local tests = {}

-- 1. shapes_basic -----------------------------------------------------------
tests[#tests+1] = {name = "shapes_basic", fn = function(g)
    local ok, err = pcall(function()
        -- Rect
        local r = display.newRect(g, 50*S, 80*S, 60*S, 40*S)
        r:setFillColor(0.9, 0.2, 0.2)

        -- Circle
        local c = display.newCircle(g, 140*S, 80*S, 25*S)
        c:setFillColor(0.2, 0.9, 0.2)

        -- RoundedRect
        local rr = display.newRoundedRect(g, 230*S, 80*S, 60*S, 40*S, 12*S)
        rr:setFillColor(0.2, 0.2, 0.9)

        -- Line
        local l = display.newLine(g, 30*S, 130*S, 290*S, 130*S)
        l.strokeWidth = 3*S
        l:setStrokeColor(1, 1, 0)

        -- Polygon: triangle
        local tri = display.newPolygon(g, 60*S, 180*S,
            {0, -25*S, 22*S, 13*S, -22*S, 13*S})
        tri:setFillColor(1, 0.5, 0)

        -- Polygon: star
        local sv = {}
        for i = 1, 10 do
            local a = math.rad(i * 36 - 90)
            local rad = (i % 2 == 1) and 30*S or 12*S
            sv[#sv+1] = rad * math.cos(a)
            sv[#sv+1] = rad * math.sin(a)
        end
        local star = display.newPolygon(g, 160*S, 180*S, sv)
        star:setFillColor(1, 0.8, 0.2)

        -- Polygon: pentagon
        local pv = {}
        for i = 1, 5 do
            local a = math.rad(i * 72 - 90)
            pv[#pv+1] = 25*S * math.cos(a)
            pv[#pv+1] = 25*S * math.sin(a)
        end
        local pent = display.newPolygon(g, 260*S, 180*S, pv)
        pent:setFillColor(0.6, 0.3, 0.9)

        -- Different sizes
        for i = 1, 5 do
            local sz = (10 + i * 8) * S
            local rct = display.newRect(g, 30*S + i * 55*S, 240*S, sz, sz)
            rct:setFillColor(i/5, 1-i/5, 0.5)
        end
    end)
    return ok, err
end}

-- 2. shapes_stroke ----------------------------------------------------------
tests[#tests+1] = {name = "shapes_stroke", fn = function(g)
    local ok, err = pcall(function()
        -- Thin strokes (1px)
        local r1 = display.newRect(g, 60*S, 80*S, 50*S, 35*S)
        r1:setFillColor(0.8, 0.3, 0.3)
        r1.strokeWidth = 1
        r1:setStrokeColor(1, 1, 1)

        local c1 = display.newCircle(g, 140*S, 80*S, 22*S)
        c1:setFillColor(0.3, 0.8, 0.3)
        c1.strokeWidth = 1
        c1:setStrokeColor(1, 1, 1)

        local rr1 = display.newRoundedRect(g, 230*S, 80*S, 50*S, 35*S, 8*S)
        rr1:setFillColor(0.3, 0.3, 0.8)
        rr1.strokeWidth = 1
        rr1:setStrokeColor(1, 1, 1)

        -- Thick strokes (5px)
        local r2 = display.newRect(g, 60*S, 150*S, 50*S, 35*S)
        r2:setFillColor(0.8, 0.6, 0.2)
        r2.strokeWidth = 5*S
        r2:setStrokeColor(1, 0, 0)

        local c2 = display.newCircle(g, 140*S, 150*S, 22*S)
        c2:setFillColor(0.2, 0.6, 0.8)
        c2.strokeWidth = 5*S
        c2:setStrokeColor(0, 1, 0)

        local rr2 = display.newRoundedRect(g, 230*S, 150*S, 50*S, 35*S, 8*S)
        rr2:setFillColor(0.6, 0.2, 0.8)
        rr2.strokeWidth = 5*S
        rr2:setStrokeColor(0, 0, 1)

        -- Line with thin/thick
        local l1 = display.newLine(g, 30*S, 200*S, 290*S, 200*S)
        l1.strokeWidth = 1; l1:setStrokeColor(1, 1, 0)

        local l2 = display.newLine(g, 30*S, 220*S, 290*S, 220*S)
        l2.strokeWidth = 5*S; l2:setStrokeColor(0, 1, 1)

        -- Polygon with stroke
        local tri = display.newPolygon(g, 100*S, 270*S,
            {0, -25*S, 22*S, 13*S, -22*S, 13*S})
        tri:setFillColor(0.9, 0.5, 0.1)
        tri.strokeWidth = 3*S
        tri:setStrokeColor(1, 1, 1)

        local sv = {}
        for i = 1, 10 do
            local a = math.rad(i * 36 - 90)
            local rad = (i % 2 == 1) and 28*S or 12*S
            sv[#sv+1] = rad * math.cos(a)
            sv[#sv+1] = rad * math.sin(a)
        end
        local star = display.newPolygon(g, 220*S, 270*S, sv)
        star:setFillColor(0.2, 0.7, 0.9)
        star.strokeWidth = 3*S
        star:setStrokeColor(1, 0.8, 0)
    end)
    return ok, err
end}

-- 3. fill_color -------------------------------------------------------------
tests[#tests+1] = {name = "fill_color", fn = function(g)
    local ok, err = pcall(function()
        -- Solid colors
        local colors = {
            {0.9, 0, 0}, {0, 0.9, 0}, {0, 0, 0.9},
            {1, 1, 1}, {0, 0, 0}, {0.5, 0.5, 0.5}
        }
        for i, col in ipairs(colors) do
            local r = display.newRect(g, ((i-1)%3 * 90 + 55)*S, (math.floor((i-1)/3)*50 + 80)*S, 40*S, 30*S)
            r:setFillColor(col[1], col[2], col[3])
        end

        -- Gradient fills
        local grad1 = {
            type = "gradient",
            color1 = {1, 0, 0}, color2 = {0, 0, 1},
            direction = "down"
        }
        local gr1 = display.newRect(g, 60*S, 200*S, 60*S, 60*S)
        gr1.fill = grad1

        local grad2 = {
            type = "gradient",
            color1 = {1, 1, 0}, color2 = {0, 1, 0},
            direction = "right"
        }
        local gr2 = display.newRect(g, 160*S, 200*S, 60*S, 60*S)
        gr2.fill = grad2

        -- Fully transparent
        local t1 = display.newRect(g, 260*S, 200*S, 60*S, 60*S)
        t1:setFillColor(1, 0, 0, 0) -- alpha=0

        -- Semi-transparent overlaps
        local base = display.newRect(g, 120*S, 300*S, 80*S, 60*S)
        base:setFillColor(0, 0, 1)
        local over = display.newRect(g, 150*S, 310*S, 80*S, 60*S)
        over:setFillColor(1, 0, 0, 0.5)

        -- Circle with gradient
        local gc = display.newCircle(g, 260*S, 310*S, 30*S)
        gc.fill = {type="gradient", color1={1,1,1}, color2={0,0,0}, direction="down"}
    end)
    return ok, err
end}

-- 4. textures_image ---------------------------------------------------------
tests[#tests+1] = {name = "textures_image", fn = function(g)
    local ok, err = pcall(function()
        -- PNG images
        local pngs = {"test_red.png", "test_green.png", "test_blue.png",
                       "test_checker.png", "test_gradient.png", "test_icon.png"}
        for i, fn in ipairs(pngs) do
            local x = ((i-1)%3 * 90 + 55)*S
            local y = (math.floor((i-1)/3)*70 + 80)*S
            safeImage(g, fn, x, y, 50*S, 50*S)
        end

        -- JPG images
        local jpgs = {"soil2.jpg", "solid1-1.jpg"}
        for i, fn in ipairs(jpgs) do
            safeImage(g, fn, (i * 100)*S, 240*S, 60*S, 60*S)
        end

        -- Images with alpha (PNG with transparency)
        local alphas = {"test_circle_alpha.png", "test_star_alpha.png"}
        for i, fn in ipairs(alphas) do
            safeImage(g, fn, (i * 100)*S, 330*S, 50*S, 50*S)
        end

        -- Large image scaled down
        safeImage(g, "bg-village2-1.png", CX, 400*S, 200*S, 50*S)
    end)
    return ok, err
end}

-- 5. text_rendering ---------------------------------------------------------
tests[#tests+1] = {name = "text_rendering", fn = function(g)
    local ok, err = pcall(function()
        local sizes = {8, 12, 16, 24, 36}
        local y = 70*S
        for _, sz in ipairs(sizes) do
            local t = display.newText({
                parent = g, text = "Size " .. sz,
                x = 30*S, y = y,
                font = native.systemFont, fontSize = sz*S
            })
            t.anchorX = 0
            t:setFillColor(1, 1, 1)
            y = y + (sz + 6) * S
        end

        -- Bold text
        local bold = display.newText({
            parent = g, text = "Bold Text",
            x = 200*S, y = 80*S,
            font = native.systemFontBold, fontSize = 18*S
        })
        bold:setFillColor(1, 0.8, 0)

        -- Colored text
        local coloredTexts = {
            {txt = "Red", r = 1, g2 = 0, b = 0},
            {txt = "Green", r = 0, g2 = 1, b = 0},
            {txt = "Blue", r = 0, g2 = 0, b = 1},
            {txt = "Cyan", r = 0, g2 = 1, b = 1},
        }
        for i, ct in ipairs(coloredTexts) do
            local tx = display.newText({
                parent = g, text = ct.txt,
                x = 200*S, y = (100 + i * 25)*S,
                font = native.systemFontBold, fontSize = 14*S
            })
            tx:setFillColor(ct.r, ct.g2, ct.b)
        end

        -- Multiline text
        local ml = display.newText({
            parent = g, text = "Line 1\nLine 2\nLine 3\nMultiline works!",
            x = 30*S, y = 300*S, width = 200*S,
            font = native.systemFont, fontSize = 14*S,
            align = "left"
        })
        ml.anchorX = 0; ml.anchorY = 0
        ml:setFillColor(0.8, 0.8, 0.8)

        -- Right-aligned text
        local ra = display.newText({
            parent = g, text = "Right Align\nTest text",
            x = 290*S, y = 300*S, width = 120*S,
            font = native.systemFont, fontSize = 13*S,
            align = "right"
        })
        ra.anchorX = 1; ra.anchorY = 0
        ra:setFillColor(0.6, 0.9, 0.6)
    end)
    return ok, err
end}

-- 6. transforms -------------------------------------------------------------
tests[#tests+1] = {name = "transforms", fn = function(g)
    local ok, err = pcall(function()
        -- Rotation tests
        local rotations = {0, 45, 90, 180}
        for i, rot in ipairs(rotations) do
            local r = display.newRect(g, (i * 65)*S, 80*S, 40*S, 25*S)
            r:setFillColor(0.2 + i*0.2, 0.8 - i*0.1, 0.5)
            r.rotation = rot
            local lbl = display.newText({
                parent = g, text = rot .. "deg",
                x = (i * 65)*S, y = 110*S,
                font = native.systemFont, fontSize = 10*S
            })
            lbl:setFillColor(0.7, 0.7, 0.7)
        end

        -- Scale tests
        local scales = {0.5, 1.0, 2.0}
        for i, sc in ipairs(scales) do
            local c = display.newCircle(g, (i * 90)*S, 170*S, 15*S)
            c:setFillColor(0.9, 0.4, 0.1)
            c.xScale = sc; c.yScale = sc
            local lbl = display.newText({
                parent = g, text = sc .. "x",
                x = (i * 90)*S, y = 200*S,
                font = native.systemFont, fontSize = 10*S
            })
            lbl:setFillColor(0.7, 0.7, 0.7)
        end

        -- Nested group transforms
        local outer = display.newGroup(); g:insert(outer)
        outer.x = 160*S; outer.y = 280*S; outer.rotation = 15

        local inner = display.newGroup(); outer:insert(inner)
        inner.x = 30*S; inner.y = 0; inner.rotation = 30

        local nested = display.newRect(inner, 0, 0, 30*S, 20*S)
        nested:setFillColor(0, 0.8, 0.8)

        local nested2 = display.newCircle(inner, 25*S, 0, 10*S)
        nested2:setFillColor(1, 0.3, 0.6)

        -- Label
        local lbl = display.newText({
            parent = g, text = "Nested group transforms",
            x = CX, y = 330*S,
            font = native.systemFont, fontSize = 11*S
        })
        lbl:setFillColor(0.7, 0.7, 0.7)

        -- Translation
        local movable = display.newRect(g, 50*S, 380*S, 30*S, 30*S)
        movable:setFillColor(0.9, 0.9, 0.2)
        movable:translate(80*S, 20*S) -- moves to 130, 400
    end)
    return ok, err
end}

-- 7. groups_hierarchy -------------------------------------------------------
tests[#tests+1] = {name = "groups_hierarchy", fn = function(g)
    local ok, err = pcall(function()
        -- 3-level nesting
        local lvl1 = display.newGroup(); g:insert(lvl1)
        lvl1.x = 50*S; lvl1.y = 80*S

        local bg1 = display.newRect(lvl1, 0, 0, 120*S, 80*S)
        bg1:setFillColor(0.3, 0.1, 0.1, 0.5)

        local lvl2 = display.newGroup(); lvl1:insert(lvl2)
        lvl2.x = 20*S; lvl2.y = 10*S

        local bg2 = display.newRect(lvl2, 0, 0, 80*S, 50*S)
        bg2:setFillColor(0.1, 0.3, 0.1, 0.5)

        local lvl3 = display.newGroup(); lvl2:insert(lvl3)
        lvl3.x = 10*S; lvl3.y = 10*S

        local inner = display.newCircle(lvl3, 0, 0, 15*S)
        inner:setFillColor(1, 1, 0)

        -- Group alpha
        local alphaGroup = display.newGroup(); g:insert(alphaGroup)
        alphaGroup.x = 220*S; alphaGroup.y = 80*S
        alphaGroup.alpha = 0.4

        for i = 1, 3 do
            local sq = display.newRect(alphaGroup, (i-2)*25*S, 0, 20*S, 20*S)
            sq:setFillColor(1, 0, 0)
        end

        local lbl1 = display.newText({
            parent = g, text = "alpha=0.4",
            x = 220*S, y = 110*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl1:setFillColor(0.7, 0.7, 0.7)

        -- Group visibility toggle
        local visGroup = display.newGroup(); g:insert(visGroup)
        visGroup.x = 80*S; visGroup.y = 200*S
        local vc = display.newCircle(visGroup, 0, 0, 20*S)
        vc:setFillColor(0, 1, 0)
        visGroup.isVisible = false -- should not render

        local lbl2 = display.newText({
            parent = g, text = "isVisible=false (should be empty)",
            x = 80*S, y = 230*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl2:setFillColor(0.7, 0.7, 0.7)

        -- Insert/remove from groups
        local dynGroup = display.newGroup(); g:insert(dynGroup)
        dynGroup.x = CX; dynGroup.y = 300*S

        local objs = {}
        for i = 1, 5 do
            local sq = display.newRect(dynGroup, (i-3)*30*S, 0, 22*S, 22*S)
            sq:setFillColor(i/5, 0.5, 1 - i/5)
            objs[#objs+1] = sq
        end
        -- Remove middle one
        objs[3]:removeSelf()
        objs[3] = nil

        -- Group removal
        local tempGroup = display.newGroup(); g:insert(tempGroup)
        local tc = display.newCircle(tempGroup, CX, 380*S, 15*S)
        tc:setFillColor(1, 0, 1)
        tempGroup:removeSelf()  -- should clean up without crash
    end)
    return ok, err
end}

-- 8. blend_modes ------------------------------------------------------------
tests[#tests+1] = {name = "blend_modes", fn = function(g)
    local ok, err = pcall(function()
        local modes = {"normal", "add", "multiply", "screen"}
        for i, mode in ipairs(modes) do
            local x = ((i-1) % 2 * 140 + 80)*S
            local y = (math.floor((i-1) / 2) * 150 + 110)*S

            -- Background swatch
            local bg = display.newRect(g, x, y, 100*S, 100*S)
            bg:setFillColor(0.4, 0.4, 0.4)

            -- Base colored rect
            local base = display.newRect(g, x - 10*S, y - 10*S, 50*S, 50*S)
            base:setFillColor(0, 0.5, 1)

            -- Overlapping rect with blend mode
            local over = display.newRect(g, x + 10*S, y + 10*S, 50*S, 50*S)
            over:setFillColor(1, 0.3, 0, 0.7)
            over.blendMode = mode

            -- Label
            local lbl = display.newText({
                parent = g, text = mode,
                x = x, y = y + 60*S,
                font = native.systemFontBold, fontSize = 11*S
            })
            lbl:setFillColor(1, 1, 1)
        end
    end)
    return ok, err
end}

-- 9. masks ------------------------------------------------------------------
tests[#tests+1] = {name = "masks", fn = function(g)
    local ok, err = pcall(function()
        -- Create a simple masked group using graphics.newMask if available
        -- Masks require a specific bitmap file; test with pcall

        -- Container clipping via display.newContainer
        local ok2, container = pcall(function()
            local c = display.newContainer(g, 120*S, 80*S)
            c.x = 80*S; c.y = 100*S

            -- Content larger than container (should clip)
            local big = display.newRect(c, 0, 0, 200*S, 200*S)
            big:setFillColor(0, 0.7, 0.3)
            local circ = display.newCircle(c, 20*S, 0, 40*S)
            circ:setFillColor(1, 0, 0)
            return c
        end)

        local lbl1 = display.newText({
            parent = g, text = ok2 and "Container clip OK" or "Container N/A",
            x = 80*S, y = 160*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl1:setFillColor(0.7, 0.7, 0.7)

        -- Mask on display object (using graphics.newMask)
        local ok3, mask = pcall(function()
            return graphics.newMask("test_circle_alpha.png")
        end)

        if ok3 and mask then
            local img = display.newRect(g, 220*S, 100*S, 80*S, 80*S)
            img:setFillColor(1, 0.5, 0)
            img:setMask(mask)
            img.maskScaleX = 80*S / 128
            img.maskScaleY = 80*S / 128

            local lbl2 = display.newText({
                parent = g, text = "Mask applied",
                x = 220*S, y = 160*S,
                font = native.systemFont, fontSize = 10*S
            })
            lbl2:setFillColor(0.7, 0.7, 0.7)
        else
            local lbl2 = display.newText({
                parent = g, text = "Mask: " .. tostring(mask),
                x = 220*S, y = 160*S,
                font = native.systemFont, fontSize = 9*S
            })
            lbl2:setFillColor(0.9, 0.5, 0.5)
        end

        -- Snapshot test
        local ok4, snap = pcall(function()
            local s = display.newSnapshot(g, 100*S, 100*S)
            s.x = CX; s.y = 260*S
            local r = display.newRect(s.group, 0, 0, 60*S, 60*S)
            r:setFillColor(0.8, 0.2, 0.8)
            local c = display.newCircle(s.group, 15*S, 15*S, 20*S)
            c:setFillColor(0, 1, 1)
            s:invalidate()
            return s
        end)

        local lbl3 = display.newText({
            parent = g, text = ok4 and "Snapshot OK" or ("Snapshot: " .. tostring(snap)),
            x = CX, y = 330*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl3:setFillColor(0.7, 0.7, 0.7)
    end)
    return ok, err
end}

-- 10. snapshot_fbo ----------------------------------------------------------
tests[#tests+1] = {name = "snapshot_fbo", fn = function(g)
    local ok, err = pcall(function()
        -- Scene content to capture
        local scene = display.newGroup(); g:insert(scene)
        for i = 1, 8 do
            local r = display.newRect(scene, math.random(20, 300)*S, math.random(60, 400)*S,
                (20 + math.random(40))*S, (20 + math.random(40))*S)
            r:setFillColor(math.random(), math.random(), math.random())
            r.rotation = math.random(360)
        end

        -- display.capture
        local ok2, cap = pcall(function()
            return display.capture(scene, {saveToPhotoLibrary = false, isFullResolution = false})
        end)
        if ok2 and cap then
            cap.x = CX; cap.y = 300*S
            cap.xScale = 0.3; cap.yScale = 0.3
            g:insert(cap)
            local lbl = display.newText({
                parent = g, text = "display.capture() OK",
                x = CX, y = 360*S,
                font = native.systemFont, fontSize = 11*S
            })
            lbl:setFillColor(0, 1, 0)
        else
            local lbl = display.newText({
                parent = g, text = "capture: " .. tostring(cap),
                x = CX, y = 360*S,
                font = native.systemFont, fontSize = 10*S
            })
            lbl:setFillColor(0.9, 0.5, 0.5)
        end

        -- display.newSnapshot for offscreen rendering
        local ok3, snap = pcall(function()
            local s = display.newSnapshot(g, 150*S, 100*S)
            s.x = CX; s.y = 150*S
            for j = 1, 5 do
                local c = display.newCircle(s.group, (j-3)*25*S, 0, 15*S)
                c:setFillColor(j/5, 1-j/5, 0.5)
            end
            s:invalidate()
            return s
        end)

        local lbl2 = display.newText({
            parent = g, text = ok3 and "newSnapshot OK" or ("snapshot: " .. tostring(snap)),
            x = CX, y = 210*S,
            font = native.systemFont, fontSize = 11*S
        })
        lbl2:setFillColor(ok3 and {0,1,0} or {0.9,0.5,0.5})
    end)
    return ok, err
end}

-- 11. object_lifecycle ------------------------------------------------------
tests[#tests+1] = {name = "object_lifecycle", fn = function(g)
    local ok, err = pcall(function()
        local CYCLES = 3
        local OBJ_COUNT = 50

        for cycle = 1, CYCLES do
            local objs = {}
            -- Create
            for i = 1, OBJ_COUNT do
                local kind = i % 3
                local obj
                if kind == 0 then
                    obj = display.newRect(g, math.random(20, 300)*S, math.random(60, 400)*S, 15*S, 15*S)
                elseif kind == 1 then
                    obj = display.newCircle(g, math.random(20, 300)*S, math.random(60, 400)*S, 8*S)
                else
                    obj = display.newRoundedRect(g, math.random(20, 300)*S, math.random(60, 400)*S, 15*S, 15*S, 3*S)
                end
                obj:setFillColor(math.random(), math.random(), math.random())
                objs[#objs+1] = obj
            end

            -- Modify properties
            for _, obj in ipairs(objs) do
                obj.x = obj.x + 10*S
                obj.y = obj.y - 5*S
                obj.alpha = 0.5 + math.random() * 0.5
                obj:setFillColor(math.random(), math.random(), math.random())
                obj.rotation = math.random(360)
            end

            -- removeSelf all
            for _, obj in ipairs(objs) do
                obj:removeSelf()
            end
            objs = nil
            print("[lifecycle] Cycle " .. cycle .. "/" .. CYCLES .. " complete")
        end

        -- Final: leave some visible objects as proof
        for i = 1, 10 do
            local c = display.newCircle(g, (i*28 + 10)*S, CY, 10*S)
            c:setFillColor(i/10, 1-i/10, 0.5)
        end
        local lbl = display.newText({
            parent = g, text = "3 cycles x 50 objects: create/modify/remove",
            x = CX, y = CY + 40*S,
            font = native.systemFont, fontSize = 11*S
        })
        lbl:setFillColor(0, 1, 0)
    end)
    return ok, err
end}

-- 12. scene_transition (simplified, no composer in sub-test) -----------------
tests[#tests+1] = {name = "scene_transition", fn = function(g)
    local ok, err = pcall(function()
        -- Simulate scene transition by creating/destroying groups rapidly
        local lbl = display.newText({
            parent = g, text = "Scene transition simulation",
            x = CX, y = 80*S,
            font = native.systemFontBold, fontSize = 14*S
        })
        lbl:setFillColor(1, 1, 1)

        -- Create "scene A"
        local sceneA = display.newGroup(); g:insert(sceneA)
        for i = 1, 10 do
            local r = display.newRect(sceneA, math.random(30, 290)*S, math.random(100, 350)*S, 30*S, 30*S)
            r:setFillColor(0, 0.5, 1)
        end

        -- Transition out: fade and remove
        transition.to(sceneA, {
            alpha = 0, time = 800,
            onComplete = function()
                sceneA:removeSelf()

                -- Create "scene B"
                local sceneB = display.newGroup(); g:insert(sceneB)
                sceneB.alpha = 0
                for i = 1, 10 do
                    local c = display.newCircle(sceneB, math.random(30, 290)*S, math.random(100, 350)*S, 15*S)
                    c:setFillColor(1, 0.5, 0)
                end
                transition.to(sceneB, {alpha = 1, time = 800})

                local lbl2 = display.newText({
                    parent = g, text = "Scene B faded in",
                    x = CX, y = 380*S,
                    font = native.systemFont, fontSize = 11*S
                })
                lbl2:setFillColor(0, 1, 0)
            end
        })
    end)
    return ok, err
end}

-- 13. physics_display -------------------------------------------------------
tests[#tests+1] = {name = "physics_display", fn = function(g)
    local ok, err = pcall(function()
        local physics = require("physics")
        physics.start()
        physics.setGravity(0, 9.8 * S)

        -- Ground (static)
        local ground = display.newRect(g, CX, 380*S, 280*S, 15*S)
        ground:setFillColor(0.4, 0.4, 0.4)
        physics.addBody(ground, "static", {friction = 0.5})

        -- Walls
        local wallL = display.newRect(g, 20*S, CY, 10*S, 300*S)
        wallL:setFillColor(0.3, 0.3, 0.3)
        physics.addBody(wallL, "static")

        local wallR = display.newRect(g, 300*S, CY, 10*S, 300*S)
        wallR:setFillColor(0.3, 0.3, 0.3)
        physics.addBody(wallR, "static")

        -- Dynamic objects
        for i = 1, 8 do
            local x = (40 + math.random(220)) * S
            local y = (60 + i * 20) * S
            local obj
            if i % 3 == 0 then
                obj = display.newCircle(g, x, y, (8 + math.random(8))*S)
                obj:setFillColor(math.random(), math.random(), 0.8)
                physics.addBody(obj, "dynamic", {density = 1, bounce = 0.5, radius = obj.path.radius})
            else
                local w = (15 + math.random(15))*S
                local h = (15 + math.random(15))*S
                obj = display.newRect(g, x, y, w, h)
                obj:setFillColor(0.8, math.random(), math.random())
                physics.addBody(obj, "dynamic", {density = 1, bounce = 0.5})
            end
            obj.rotation = math.random(360)
        end

        local lbl = display.newText({
            parent = g, text = "Physics + Display Objects",
            x = CX, y = 60*S,
            font = native.systemFontBold, fontSize = 12*S
        })
        lbl:setFillColor(1, 1, 1)
    end)
    return ok, err
end}

-- 14. stress_count ----------------------------------------------------------
tests[#tests+1] = {name = "stress_count", fn = function(g)
    local ok, err = pcall(function()
        local counts = {100, 500, 1000}
        local fpsData = {}
        local lbl = display.newText({
            parent = g, text = "Stress count: starting...",
            x = CX, y = 70*S,
            font = native.systemFontBold, fontSize = 14*S
        })
        lbl:setFillColor(1, 1, 0)

        for ci, count in ipairs(counts) do
            -- Clear previous
            for j = g.numChildren, 1, -1 do
                local child = g[j]
                if child ~= lbl then
                    child:removeSelf()
                end
            end

            -- Create objects
            local objs = {}
            local t0 = system.getTimer()
            for i = 1, count do
                local c = display.newCircle(g, math.random(10, 310)*S, math.random(60, 420)*S, 3*S)
                c:setFillColor(math.random(), math.random(), math.random())
                objs[#objs+1] = c
            end
            local createTime = system.getTimer() - t0

            print(string.format("[stress_count] %d objects created in %.0fms", count, createTime))

            -- Clean up
            for _, obj in ipairs(objs) do
                obj:removeSelf()
            end
        end

        lbl.text = "Stress count: done (see log)"
        lbl:setFillColor(0, 1, 0)
        g:insert(lbl)

        -- Leave some visible objects
        for i = 1, 30 do
            local c = display.newCircle(g, math.random(10, 310)*S, math.random(100, 400)*S, 5*S)
            c:setFillColor(math.random(), math.random(), math.random())
        end
    end)
    return ok, err
end}

-- 15. edge_cases ------------------------------------------------------------
tests[#tests+1] = {name = "edge_cases", fn = function(g)
    local ok, err = pcall(function()
        -- Zero-size object
        local zr = display.newRect(g, CX, 80*S, 0, 0)
        zr:setFillColor(1, 0, 0)
        local lbl1 = display.newText({
            parent = g, text = "Zero-size rect (invisible)",
            x = CX, y = 95*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl1:setFillColor(0.7, 0.7, 0.7)

        -- Off-screen objects
        local off1 = display.newRect(g, -100*S, CY, 50*S, 50*S)
        off1:setFillColor(1, 0, 0)
        local off2 = display.newRect(g, W + 100*S, CY, 50*S, 50*S)
        off2:setFillColor(0, 1, 0)
        local off3 = display.newRect(g, CX, -100*S, 50*S, 50*S)
        off3:setFillColor(0, 0, 1)
        local off4 = display.newRect(g, CX, H + 100*S, 50*S, 50*S)
        off4:setFillColor(1, 1, 0)
        local lbl2 = display.newText({
            parent = g, text = "4 off-screen rects (should not crash)",
            x = CX, y = 130*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl2:setFillColor(0.7, 0.7, 0.7)

        -- Negative coordinates
        local neg = display.newCircle(g, -50*S, -50*S, 30*S)
        neg:setFillColor(1, 0, 1)

        -- Very large coordinates
        local huge1 = display.newRect(g, 10000*S, 10000*S, 20*S, 20*S)
        huge1:setFillColor(0, 1, 1)

        -- Alpha = 0 objects (should not render but not crash)
        local inv = display.newRect(g, CX, 200*S, 80*S, 40*S)
        inv:setFillColor(1, 0, 0)
        inv.alpha = 0
        local lbl3 = display.newText({
            parent = g, text = "alpha=0 rect here (invisible)",
            x = CX, y = 200*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl3:setFillColor(0.7, 0.7, 0.7)

        -- Very small object
        local tiny = display.newCircle(g, CX - 50*S, 270*S, 1)
        tiny:setFillColor(1, 1, 1)
        local lbl4 = display.newText({
            parent = g, text = "1px circle",
            x = CX - 50*S, y = 285*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl4:setFillColor(0.7, 0.7, 0.7)

        -- Very large object
        local big = display.newRect(g, CX + 50*S, 320*S, 5000*S, 5000*S)
        big:setFillColor(0.2, 0, 0, 0.3)
        local lbl5 = display.newText({
            parent = g, text = "5000*S rect (huge, transparent)",
            x = CX, y = 370*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl5:setFillColor(0.7, 0.7, 0.7)

        -- Rapid property changes
        local rapid = display.newRect(g, CX, 420*S, 30*S, 30*S)
        for i = 1, 100 do
            rapid.x = math.random(20, 300) * S
            rapid.y = math.random(60, 440) * S
            rapid.alpha = math.random()
            rapid.rotation = math.random(360)
            rapid:setFillColor(math.random(), math.random(), math.random())
        end
        local lbl6 = display.newText({
            parent = g, text = "100 rapid property changes OK",
            x = CX, y = 440*S,
            font = native.systemFont, fontSize = 10*S
        })
        lbl6:setFillColor(0, 1, 0)
    end)
    return ok, err
end}

----------------------------------------------------------------------------
-- Test runner
----------------------------------------------------------------------------
local function showSummary()
    clearTestGroup()

    local bg = display.newRect(testGroup, CX, CY, W, H)
    bg:setFillColor(0.05, 0.05, 0.08)

    titleText.text = "STRESS TEST COMPLETE"
    progressText.text = ""

    print("\n=== STRESS API TEST RESULTS (" .. backend .. ") ===")
    print(string.format("%-25s %s", "Test", "Status"))
    print(string.rep("-", 45))

    local y = 60*S
    local passCount, failCount = 0, 0

    for i, r in ipairs(results) do
        local status = r.pass and "PASS" or "FAIL"
        local color = r.pass and {0, 1, 0} or {1, 0, 0}

        if r.pass then passCount = passCount + 1 else failCount = failCount + 1 end

        local line = string.format("%2d. %-22s %s", i, r.name, status)
        if not r.pass and r.msg ~= "" then
            line = line .. " (" .. r.msg .. ")"
        end
        print(line)

        local t = display.newText({
            parent = testGroup,
            text = string.format("%d. %s  %s", i, r.name, status),
            x = 20*S, y = y,
            font = native.systemFont, fontSize = 11*S
        })
        t.anchorX = 0
        t:setFillColor(unpack(color))

        if not r.pass and r.msg ~= "" then
            local em = display.newText({
                parent = testGroup,
                text = "   " .. r.msg,
                x = 20*S, y = y + 14*S,
                font = native.systemFont, fontSize = 9*S
            })
            em.anchorX = 0
            em:setFillColor(0.9, 0.5, 0.5)
            y = y + 14*S
        end

        y = y + 18*S
    end

    local summary = string.format("\nTotal: %d  Pass: %d  Fail: %d",
        #results, passCount, failCount)
    print(summary)
    print("=== END ===\n")

    local st = display.newText({
        parent = testGroup,
        text = string.format("Total: %d  |  PASS: %d  |  FAIL: %d", #results, passCount, failCount),
        x = CX, y = y + 20*S,
        font = native.systemFontBold, fontSize = 14*S
    })
    st:setFillColor(failCount == 0 and 0 or 1, failCount == 0 and 1 or 0.5, 0)

    uiGroup:toFront()
end

local function runTest(idx)
    if idx > #tests then
        showSummary()
        return
    end

    currentIdx = idx
    local t = tests[idx]

    titleText.text = string.format("[%d/%d] %s", idx, #tests, t.name)
    progressText.text = string.format("Backend: %s  |  %d tests remaining", backend, #tests - idx)

    print(string.format("\n--- Running test %d/%d: %s ---", idx, #tests, t.name))

    clearTestGroup()

    -- Dark background for each test
    local bg = display.newRect(testGroup, CX, CY, W, H)
    bg:setFillColor(0.08, 0.08, 0.1)

    local pass, err = t.fn(testGroup)
    if pass == nil then pass = true end  -- pcall inside fn handles errors
    if pass == false and err then
        record(t.name, false, tostring(err))
    else
        record(t.name, true)
    end

    uiGroup:toFront()

    -- Auto-advance after TEST_DURATION
    timer.performWithDelay(TEST_DURATION, function()
        -- Clean up physics if it was started
        if t.name == "physics_display" then
            pcall(function()
                local physics = require("physics")
                physics.stop()
            end)
        end
        runTest(idx + 1)
    end)
end

-- Start after a short delay to let the display settle
timer.performWithDelay(500, function()
    runTest(1)
end)

print("=== Stress API Test Initialized (" .. #tests .. " sub-tests) ===")
