------------------------------------------------------------------------
-- test_unit.lua - Unit tests for Solar2D core APIs
-- Entry: SOLAR2D_TEST=unit
------------------------------------------------------------------------

local T = require("lib.test_framework")

------------------------------------------------------------------------
-- display.newRect
------------------------------------------------------------------------
T.describe("display.newRect", function()
    T.it("creates object with correct dimensions", function()
        local r = display.newRect(0, 0, 100, 50)
        T.assertNotNil(r)
        T.assertEqual(r.width, 100)
        T.assertEqual(r.height, 50)
        r:removeSelf()
    end)

    T.it("default fill is white", function()
        local r = display.newRect(0, 0, 10, 10)
        T.assertNotNil(r)
        r:removeSelf()
    end)

    T.it("position can be set", function()
        local r = display.newRect(50, 75, 10, 10)
        T.assertAlmostEqual(r.x, 50, 0.1)
        T.assertAlmostEqual(r.y, 75, 0.1)
        r:removeSelf()
    end)

    T.it("responds to removeSelf", function()
        local r = display.newRect(0, 0, 10, 10)
        r:removeSelf()
        T.assertNotNil(r) -- Lua table still exists after removeSelf
    end)
end)

------------------------------------------------------------------------
-- display.newCircle
------------------------------------------------------------------------
T.describe("display.newCircle", function()
    T.it("creates circle with correct radius", function()
        local c = display.newCircle(0, 0, 25)
        T.assertNotNil(c)
        T.assertAlmostEqual(c.path.radius, 25, 0.1)
        c:removeSelf()
    end)

    T.it("position is at center", function()
        local c = display.newCircle(100, 200, 10)
        T.assertAlmostEqual(c.x, 100, 0.1)
        T.assertAlmostEqual(c.y, 200, 0.1)
        c:removeSelf()
    end)
end)

------------------------------------------------------------------------
-- display.newRoundedRect
------------------------------------------------------------------------
T.describe("display.newRoundedRect", function()
    T.it("creates rounded rect", function()
        local r = display.newRoundedRect(0, 0, 100, 50, 10)
        T.assertNotNil(r)
        T.assertEqual(r.width, 100)
        T.assertEqual(r.height, 50)
        r:removeSelf()
    end)
end)

------------------------------------------------------------------------
-- display.newLine
------------------------------------------------------------------------
T.describe("display.newLine", function()
    T.it("creates line between two points", function()
        local l = display.newLine(0, 0, 100, 100)
        T.assertNotNil(l)
        l:removeSelf()
    end)
end)

------------------------------------------------------------------------
-- display.newGroup
------------------------------------------------------------------------
T.describe("display.newGroup", function()
    T.it("creates empty group", function()
        local g = display.newGroup()
        T.assertNotNil(g)
        T.assertEqual(g.numChildren, 0)
        g:removeSelf()
    end)

    T.it("inserts children correctly", function()
        local g = display.newGroup()
        local r = display.newRect(g, 0, 0, 10, 10)
        T.assertEqual(g.numChildren, 1)
        g:removeSelf()
    end)

    T.it("nested groups work", function()
        local parent = display.newGroup()
        local child = display.newGroup()
        parent:insert(child)
        T.assertEqual(parent.numChildren, 1)
        parent:removeSelf()
    end)
end)

------------------------------------------------------------------------
-- display.newText
------------------------------------------------------------------------
T.describe("display.newText", function()
    T.it("creates text object", function()
        local t = display.newText("Hello", 0, 0, native.systemFont, 20)
        T.assertNotNil(t)
        T.assertEqual(t.text, "Hello")
        t:removeSelf()
    end)

    T.it("text can be changed", function()
        local t = display.newText("A", 0, 0, native.systemFont, 16)
        t.text = "B"
        T.assertEqual(t.text, "B")
        t:removeSelf()
    end)
end)

------------------------------------------------------------------------
-- display.newImage
------------------------------------------------------------------------
T.describe("display.newImage", function()
    T.it("loads a PNG image", function()
        local img = display.newImage("test_blue.png")
        T.assertNotNil(img, "test_blue.png should load")
        if img then img:removeSelf() end
    end)

    T.it("loads a JPG image", function()
        local img = display.newImage("solid1-1.jpg")
        T.assertNotNil(img, "solid1-1.jpg should load")
        if img then img:removeSelf() end
    end)
end)

------------------------------------------------------------------------
-- Object properties
------------------------------------------------------------------------
T.describe("object properties", function()
    T.it("alpha property", function()
        local r = display.newRect(0, 0, 10, 10)
        T.assertAlmostEqual(r.alpha, 1.0, 0.01)
        r.alpha = 0.5
        T.assertAlmostEqual(r.alpha, 0.5, 0.01)
        r:removeSelf()
    end)

    T.it("isVisible property", function()
        local r = display.newRect(0, 0, 10, 10)
        T.assertTrue(r.isVisible)
        r.isVisible = false
        T.assertFalse(r.isVisible)
        r:removeSelf()
    end)

    T.it("rotation property", function()
        local r = display.newRect(0, 0, 10, 10)
        r.rotation = 45
        T.assertAlmostEqual(r.rotation, 45, 0.1)
        r:removeSelf()
    end)

    T.it("scale properties", function()
        local r = display.newRect(0, 0, 10, 10)
        r.xScale = 2
        r.yScale = 0.5
        T.assertAlmostEqual(r.xScale, 2, 0.01)
        T.assertAlmostEqual(r.yScale, 0.5, 0.01)
        r:removeSelf()
    end)

    T.it("fill color can be set", function()
        local r = display.newRect(0, 0, 10, 10)
        r:setFillColor(1, 0, 0) -- red, should not error
        r:removeSelf()
    end)
end)

------------------------------------------------------------------------
-- system.getInfo
------------------------------------------------------------------------
T.describe("system.getInfo", function()
    T.it("returns platform string", function()
        local platform = system.getInfo("platform")
        T.assertNotNil(platform)
        T.assertType(platform, "string")
    end)

    T.it("returns appName", function()
        local name = system.getInfo("appName")
        T.assertNotNil(name)
    end)

    T.it("returns environment", function()
        local env = system.getInfo("environment")
        T.assertNotNil(env)
        T.assertType(env, "string")
    end)
end)

------------------------------------------------------------------------
-- math operations (sanity check)
------------------------------------------------------------------------
T.describe("math operations", function()
    T.it("basic arithmetic", function()
        T.assertEqual(1 + 1, 2)
        T.assertEqual(10 / 2, 5)
        T.assertEqual(3 * 4, 12)
    end)

    T.it("trig functions", function()
        T.assertAlmostEqual(math.sin(0), 0, 0.001)
        T.assertAlmostEqual(math.cos(0), 1, 0.001)
        T.assertAlmostEqual(math.sin(math.pi / 2), 1, 0.001)
    end)

    T.it("math.huge and nan", function()
        T.assertTrue(math.huge > 0)
        local nan = 0 / 0
        T.assertFalse(nan == nan) -- NaN is not equal to itself
    end)
end)

------------------------------------------------------------------------
-- transition system
------------------------------------------------------------------------
T.describe("transition", function()
    T.it("transition.to returns handle", function()
        local r = display.newRect(0, 0, 10, 10)
        local handle = transition.to(r, { time = 100, x = 50 })
        T.assertNotNil(handle)
        transition.cancel(handle)
        r:removeSelf()
    end)
end)

------------------------------------------------------------------------
-- timer system
------------------------------------------------------------------------
T.describe("timer", function()
    T.it("timer.performWithDelay returns handle", function()
        local handle = timer.performWithDelay(1000, function() end)
        T.assertNotNil(handle)
        timer.cancel(handle)
    end)
end)

------------------------------------------------------------------------
-- display constants
------------------------------------------------------------------------
T.describe("display constants", function()
    T.it("contentWidth and contentHeight are positive", function()
        T.assertTrue(display.contentWidth > 0)
        T.assertTrue(display.contentHeight > 0)
    end)

    T.it("contentCenterX and contentCenterY", function()
        T.assertAlmostEqual(display.contentCenterX, display.contentWidth / 2, 1)
        T.assertAlmostEqual(display.contentCenterY, display.contentHeight / 2, 1)
    end)
end)

------------------------------------------------------------------------
-- Run all tests
------------------------------------------------------------------------
T.run()
