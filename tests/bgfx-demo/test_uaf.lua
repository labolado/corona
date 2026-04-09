-- test_uaf.lua
-- Reproduce: heap-use-after-free in BgfxCommandBuffer::ExecuteDraw
--
-- Root cause (confirmed by ASAN):
--   Scene::Render() calls Collect() AFTER Swap(), which frees textures
--   that the new front buffer (next frame's Execute target) still references.
--
--   The crash path:
--     Frame N: back buffer references TextureA
--     Frame N: Swap() → back becomes front
--     Frame N: Render() → executes old front (ok)
--     Frame N: Collect() → frees TextureA (orphaned by scene transition)
--     Frame N+1: Render() → executes front (has TextureA) → CRASH
--
-- Reproduction strategy:
--   Composer scene transitions move objects to orphanage.
--   Scene::Collect() frees orphanage every 3 frames.
--   We simulate this by using composer.gotoScene() with many textured objects.
--   The key is: objects must be VISIBLE (in command buffer) then immediately
--   removed via scene transition.

local composer = require("composer")

display.setStatusBar(display.HiddenStatusBar)

-- Scene A: lots of unique textured objects
composer.recycleOnSceneChange = false  -- force full cleanup

local sceneA = composer.newScene("sceneA")
local sceneB = composer.newScene("sceneB")

local images = {
    "test_checker.png", "test_gradient.png", "test_circle_alpha.png",
    "grass1.png", "soil2.jpg", "solid1-1.jpg", "grass_track1.png",
}

local W, H = display.contentWidth, display.contentHeight
local cycle = 0
local maxCycles = 500
local currentScene = "A"

function sceneA:create(event)
    local g = self.view
    -- Create textured objects
    for i = 1, 40 do
        local fname = images[((i-1) % #images) + 1]
        local sz = 20 + (i % 60)
        local img = display.newImageRect(g, fname, sz, sz + (i % 30))
        if img then
            img.x = math.random(0, W)
            img.y = math.random(0, H)
            img.alpha = 0.5 + math.random() * 0.5
        end
    end
    -- COMPOSITE PAINT objects (this is what tank app uses heavily!)
    -- Composite paint creates intermediate FBOs + extra textures
    for i = 1, 20 do
        local rect = display.newRect(g, math.random(0, W), math.random(0, H), 50 + i*3, 50 + i*2)
        local fname1 = images[((i-1) % #images) + 1]
        local fname2 = images[((i+2) % #images) + 1]
        local ok, err = pcall(function()
            rect.fill = {
                type = "composite",
                paint1 = { type="image", filename=fname1 },
                paint2 = { type="image", filename=fname2 },
            }
        end)
    end
    -- Canvas textures (unique, not cached)
    for i = 1, 10 do
        local canvas = graphics.newTexture({type="canvas", width=32+i*16, height=32+i*16})
        if canvas then
            local r = display.newRect(0, 0, 20, 20)
            r:setFillColor(math.random(), math.random(), 0)
            canvas:draw(r)
            canvas:invalidate()
            r:removeSelf()
            local ci = display.newImageRect(g, canvas.filename, canvas.baseDir, 40, 40)
            if ci then ci.x = math.random(0, W); ci.y = math.random(0, H) end
            canvas:releaseSelf()
        end
    end
end

function sceneA:show(event)
    if event.phase == "did" then
        -- Dynamically mutate fill on visible objects (this orphans old paint textures)
        -- Then immediately switch scene
        local g = self.view
        if g and g.numChildren then
            for i = 1, math.min(g.numChildren, 15) do
                local obj = g[i]
                if obj and obj.setFillColor then
                    pcall(function()
                        -- Replace fill with new composite → old texture orphaned
                        local fname1 = images[math.random(1, #images)]
                        local fname2 = images[math.random(1, #images)]
                        obj.fill = {
                            type = "composite",
                            paint1 = { type="image", filename=fname1 },
                            paint2 = { type="image", filename=fname2 },
                        }
                    end)
                end
            end
        end
        -- Force GC to pressure orphanage collection
        collectgarbage("collect")

        timer.performWithDelay(1, function()
            cycle = cycle + 1
            if cycle < maxCycles then
                composer.gotoScene("sceneB", {time=0})
            else
                print("=== UAF TEST PASSED: " .. cycle .. " cycles ===")
            end
        end)
    end
end

function sceneA:destroy(event)
    -- Scene destroyed, objects go to orphanage → Collect() will free them
end

sceneA:addEventListener("create", sceneA)
sceneA:addEventListener("show", sceneA)
sceneA:addEventListener("destroy", sceneA)

function sceneB:create(event)
    local g = self.view
    for i = 1, 40 do
        local fname = images[((i + 3) % #images) + 1]
        local sz = 25 + (i % 55)
        local img = display.newImageRect(g, fname, sz, sz + (i % 25))
        if img then
            img.x = math.random(0, W)
            img.y = math.random(0, H)
        end
    end
    -- Composite paints
    for i = 1, 20 do
        local rect = display.newRect(g, math.random(0, W), math.random(0, H), 45 + i*3, 45 + i*2)
        local fname1 = images[((i+1) % #images) + 1]
        local fname2 = images[((i+4) % #images) + 1]
        pcall(function()
            rect.fill = {
                type = "composite",
                paint1 = { type="image", filename=fname1 },
                paint2 = { type="image", filename=fname2 },
            }
        end)
    end
    -- Canvas
    for i = 1, 10 do
        local canvas = graphics.newTexture({type="canvas", width=48+i*12, height=48+i*12})
        if canvas then
            local r = display.newRect(0, 0, 25, 25)
            r:setFillColor(0, math.random(), math.random())
            canvas:draw(r)
            canvas:invalidate()
            r:removeSelf()
            local ci = display.newImageRect(g, canvas.filename, canvas.baseDir, 45, 45)
            if ci then ci.x = math.random(0, W); ci.y = math.random(0, H) end
            canvas:releaseSelf()
        end
    end
end

function sceneB:show(event)
    if event.phase == "did" then
        local g = self.view
        if g and g.numChildren then
            for i = 1, math.min(g.numChildren, 15) do
                local obj = g[i]
                if obj and obj.setFillColor then
                    pcall(function()
                        local fname1 = images[math.random(1, #images)]
                        local fname2 = images[math.random(1, #images)]
                        obj.fill = {
                            type = "composite",
                            paint1 = { type="image", filename=fname1 },
                            paint2 = { type="image", filename=fname2 },
                        }
                    end)
                end
            end
        end
        collectgarbage("collect")

        timer.performWithDelay(1, function()
            cycle = cycle + 1
            if cycle % 50 == 0 then
                print("UAF test: " .. cycle .. "/" .. maxCycles .. " cycles")
            end
            if cycle < maxCycles then
                composer.gotoScene("sceneA", {time=0})
            else
                print("=== UAF TEST PASSED: " .. cycle .. " cycles ===")
            end
        end)
    end
end

function sceneB:destroy(event) end

sceneB:addEventListener("create", sceneB)
sceneB:addEventListener("show", sceneB)
sceneB:addEventListener("destroy", sceneB)

-- Start
print("=== UAF CRASH TEST: " .. maxCycles .. " scene transitions ===")
print("=== Using composer.gotoScene to trigger orphanage + Collect ===")
composer.gotoScene("sceneA", {time=0})
