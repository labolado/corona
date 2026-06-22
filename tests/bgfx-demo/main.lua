--[[
    main.lua - Solar2D bgfx Test Demo Main Entry

    Comprehensive test demo for verifying bgfx rendering backend correctness.
    Uses composer for multi-scene navigation with bottom navigation bar.

    Test entry system:
      SOLAR2D_TEST=bench  → run test_bench.lua (performance benchmark)
      SOLAR2D_TEST=xxx    → run test_xxx.lua
      (no env var)        → normal demo with navigation

    FTL Game Loop:
      launchArgs.gameLoopScenario=N  → run test_benchmark_all and exit
--]]

-- Firebase Test Lab Game Loop support. Map a scenario number to a test entry so
-- both platforms select the same test from one source of truth.
-- Scenario arrives differently per platform:
--   Android: GameLoopActivity sets SOLAR2D_TEST (handled by the env path below).
--   iOS: launched cold via the firebase-game-loop://<n> URL scheme, which lands in
--        launchArgs.url (AppDelegate SetLaunchArgs). The bare global gameLoopScenario
--        is only set for the -com.google.games.test.loops arg / warm openURL paths,
--        which FTL cold launch does NOT hit — so launchArgs.url is the real source.
local FTL_SCENARIO_TESTS = { [1] = "benchmark_all", [2] = "bench", [3] = "all_scenes", [4] = "shapes", [5] = "perf", [6] = "sdf_perf", [7] = "realrepro079", [8] = "realrepro079" }
local launchArgs = system.getInfo("launchArgs")
local gameLoopScenario = _G.gameLoopScenario
if not gameLoopScenario and launchArgs and launchArgs.url then
    gameLoopScenario = tostring(launchArgs.url):match("firebase%-game%-loop://(%d+)")
end
if gameLoopScenario then
    local n = tonumber(gameLoopScenario)
    local testName = FTL_SCENARIO_TESTS[n]
    print("=== FTL Game Loop Mode: Scenario " .. tostring(gameLoopScenario)
        .. " -> " .. tostring(testName) .. " ===")
    if not testName then
        print("=== GAME LOOP: unknown scenario " .. tostring(gameLoopScenario) .. " ===")
        os.exit(1)
    end
    if n == 1 then
        -- benchmark_all self-terminates via the phase/waitLoop guard
        local ok = pcall(require, "test_benchmark_all")
        if not ok then os.exit(1) end
        local t0 = system.getTimer()
        local function waitLoop()
            if phase == "done" then
                print("=== GAME LOOP COMPLETE ===")
                os.exit(0)
            elseif system.getTimer() - t0 > 60000 then
                print("=== GAME LOOP TIMEOUT ===")
                os.exit(1)
            else
                timer.performWithDelay(500, waitLoop)
            end
        end
        timer.performWithDelay(1000, waitLoop)
        return
    end
    -- Other scenarios run their test entry; FTL controls run duration on device.
    print("=== Running test entry: test_" .. testName .. " ===")
    require("test_" .. testName)
    return
end

-- Check for test entry: env var, flag file, or build-time override
local testEntry = os.getenv("SOLAR2D_TEST")
if not testEntry then
    -- Check flag file (for Android where env vars don't work)
    local path = system.pathForFile("solar2d_test.txt", system.DocumentsDirectory)
    if path then
        local f = io.open(path, "r")
        if f then
            testEntry = f:read("*l")
            f:close()
            os.remove(path)
        end
    end
end
-- Build-time override: check bundled resource file (for Android perf testing)
if not testEntry then
    local path = system.pathForFile("solar2d_test.txt", system.ResourceDirectory)
    if path then
        local f = io.open(path, "r")
        if f then
            testEntry = f:read("*l")
            f:close()
        end
    end
end
if testEntry then
    local testFile = "test_" .. testEntry
    print("=== Running test entry: " .. testFile .. " ===")
    require(testFile)
    return
end

local composer = require("composer")

-- Scaling variables for high resolution displays
local W = display.contentWidth
local H = display.contentHeight
local S = W / 320  -- Scaling factor

-- Disable status bar
display.setStatusBar(display.HiddenStatusBar)

-- Detect backend
local backend = os.getenv("SOLAR2D_BACKEND") or "?"
local platform = system.getInfo("platform")
if backend == "?" then
    -- On Android/iOS, bgfx is hardcoded in C++ (no env var available)
    -- Detect by checking if bgfx-specific API exists
    if platform == "android" or platform == "ios" or platform == "tvos" then
        backend = "bgfx"
    end
end

-- Enable debugging output
print("=== Solar2D bgfx Test Demo Starting ===")
print("Display size: " .. display.contentWidth .. "x" .. display.contentHeight)
print("Platform: " .. system.getInfo("platform"))
print("Environment: " .. system.getInfo("environment"))
print("Backend: " .. backend)

-- Scene definitions
local scenes = {
    { name = "shapes",     label = "Shapes" },
    { name = "images",     label = "Images" },
    { name = "text",       label = "Text" },
    { name = "transforms", label = "Transform" },
    { name = "blend",      label = "Blend" },
    { name = "animation",  label = "Animate" },
    { name = "groups",     label = "Group" },
    { name = "physics",    label = "Physics" },
    { name = "masks",      label = "Mask" },
    { name = "stress",     label = "Stress" },
    { name = "custom_shader", label = "Shader" },
}

_G.bgfxDemoScenes = scenes
_G.bgfxDemoCurrentScene = 1

-- Create navigation bar
local function createNavigationBar()
    local navGroup = display.newGroup()

    -- Navigation bar background
    local navBg = display.newRect(navGroup, display.contentCenterX, display.contentHeight - 25*S, display.contentWidth, 50*S)
    navBg:setFillColor(0.15, 0.15, 0.15)
    navBg.strokeWidth = 1*S
    navBg:setStrokeColor(0.3, 0.3, 0.3)

    -- Scene buttons
    local buttonWidth = display.contentWidth / #scenes

    for i, sceneInfo in ipairs(scenes) do
        local btnX = (i - 0.5) * buttonWidth
        local btnY = display.contentHeight - 25

        -- Button background
        local btn = display.newRect(navGroup, btnX, btnY, buttonWidth - 2, 46*S)
        btn:setFillColor(0.25, 0.25, 0.25)
        btn.sceneIndex = i
        btn.sceneName = sceneInfo.name

        -- Button label
        local label = display.newText({
            parent = navGroup,
            text = tostring(i),
            x = btnX,
            y = btnY,
            font = native.systemFontBold,
            fontSize = 12*S
        })
        label:setFillColor(0.9, 0.9, 0.9)

        -- Touch handler
        btn:addEventListener("touch", function(event)
            if event.phase == "ended" then
                if _G.bgfxDemoCurrentScene ~= i then
                    _G.bgfxDemoCurrentScene = i
                    composer.gotoScene("scene_" .. sceneInfo.name, { effect = "slideLeft", time = 300 })
                    print("[Navigation] Switching to Scene " .. i .. ": " .. sceneInfo.name)
                end
            end
            return true
        end)

        -- Store reference for highlighting
        sceneInfo.button = btn
        sceneInfo.labelText = label
    end

    -- Highlight current scene function
    function _G.updateNavHighlight()
        for i, sceneInfo in ipairs(scenes) do
            if i == _G.bgfxDemoCurrentScene then
                sceneInfo.button:setFillColor(0.4, 0.6, 0.9)
                sceneInfo.labelText:setFillColor(1, 1, 1)
            else
                sceneInfo.button:setFillColor(0.25, 0.25, 0.25)
                sceneInfo.labelText:setFillColor(0.7, 0.7, 0.7)
            end
        end
    end

    _G.updateNavHighlight()

    return navGroup
end

-- Create navigation bar (it will stay on top)
local navGroup = createNavigationBar()

-- Insert navGroup into composer's stage and keep it on top
composer.stage:insert(navGroup)
navGroup:toFront()

-- FPS overlay (large text, top of screen)
local fpsGroup = display.newGroup()
local fpsBg = display.newRect(fpsGroup, display.contentCenterX, 20*S, display.contentWidth, 40*S)
fpsBg:setFillColor(0, 0, 0, 0.7)

local fpsText = display.newText({
    parent = fpsGroup,
    text = backend:upper() .. "  FPS: --",
    x = display.contentCenterX,
    y = 20*S,
    font = native.systemFontBold,
    fontSize = 20*S
})
fpsText:setFillColor(0, 1, 0)

local deviceInfo = system.getInfo("platform") .. " / " .. system.getInfo("architectureInfo")
local infoText = display.newText({
    parent = fpsGroup,
    text = deviceInfo,
    x = display.contentCenterX,
    y = 38*S,
    font = native.systemFont,
    fontSize = 10*S
})
infoText:setFillColor(0.7, 0.7, 0.7)

composer.stage:insert(fpsGroup)

-- FPS calculation
local frameCount = 0
local lastTime = system.getTimer()

-- Keep navGroup and fpsGroup on top after scene changes
Runtime:addEventListener("enterFrame", function()
    navGroup:toFront()
    fpsGroup:toFront()

    -- FPS counter
    frameCount = frameCount + 1
    local now = system.getTimer()
    local elapsed = now - lastTime
    if elapsed >= 1000 then
        local fps = math.floor(frameCount / (elapsed / 1000) + 0.5)
        fpsText.text = backend:upper() .. "  FPS: " .. fps
        if fps >= 55 then
            fpsText:setFillColor(0, 1, 0)  -- green
        elseif fps >= 30 then
            fpsText:setFillColor(1, 1, 0)  -- yellow
        else
            fpsText:setFillColor(1, 0, 0)  -- red
        end
        frameCount = 0
        lastTime = now
    end
end)

-- Go to first scene
print("[Main] Loading Scene 1: Shapes")
composer.gotoScene("scene_shapes")

print("=== Solar2D bgfx Test Demo Initialized ===")
