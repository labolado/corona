--[[
    test_regression.lua - Extended regression test suite
    Covers 10 base scenes + 8 high-risk专项 tests

    Usage: SOLAR2D_TEST=regression (via env var or solar2d_test.txt in APK)
    Outputs: /tmp/regression_<backend>.txt + <Documents>/regression_<backend>.txt
--]]

display.setStatusBar(display.HiddenStatusBar)

local composer = require("composer")
local backend = os.getenv("SOLAR2D_BACKEND") or "unknown"

-- ============================================================
-- Regression harness: intercept test-module side effects
-- (enterFrame listeners, timers, os.exit) so cleanupStage()
-- can fully reset state between tests.
-- ============================================================
local _trackedEnterFrame = {}
local _trackedTimers = {}

local _origAddEventListener = Runtime.addEventListener
function Runtime.addEventListener(self, eventName, listener)
    if eventName == "enterFrame" then
        table.insert(_trackedEnterFrame, listener)
    end
    return _origAddEventListener(self, eventName, listener)
end

local _origRemoveEventListener = Runtime.removeEventListener
function Runtime.removeEventListener(self, eventName, listener)
    if eventName == "enterFrame" and listener then
        for i = #_trackedEnterFrame, 1, -1 do
            if _trackedEnterFrame[i] == listener then
                table.remove(_trackedEnterFrame, i)
                break
            end
        end
    end
    return _origRemoveEventListener(self, eventName, listener)
end

local _origTimerPerform = timer.performWithDelay
function timer.performWithDelay(delay, listener, iterations)
    local t = _origTimerPerform(delay, listener, iterations)
    if t then
        table.insert(_trackedTimers, t)
    end
    return t
end

local _origOsExit = os.exit
function os.exit(code)
    print("[REGRESSION] os.exit(" .. tostring(code) .. ") blocked")
end

local function resetTestSideEffects()
    -- Remove all enterFrame listeners added by the previous test
    for _, listener in ipairs(_trackedEnterFrame) do
        pcall(function() _origRemoveEventListener(Runtime, "enterFrame", listener) end)
    end
    _trackedEnterFrame = {}
    -- Cancel all timers added by the previous test
    for _, t in ipairs(_trackedTimers) do
        pcall(function() timer.cancel(t) end)
    end
    _trackedTimers = {}
end

print("=== REGRESSION TEST START ===")
print("Backend: " .. backend)
print("Time: " .. os.date())

-- Scene/test definitions: name, type (composer|test), duration ms
local scenes = {
    {name="shapes",          type="composer", duration=2000},
    {name="images",          type="composer", duration=2000},
    {name="text",            type="composer", duration=2000},
    {name="transforms",      type="composer", duration=2000},
    {name="blend",           type="composer", duration=2000},
    {name="animation",       type="composer", duration=2000},
    {name="groups",          type="composer", duration=2000},
    {name="physics",         type="composer", duration=2000},
    {name="masks",           type="composer", duration=2000},
    {name="stress",          type="composer", duration=3000},
    -- High-risk 专项（全部 composer 模式，避免 stress 阻塞自动回归）
    {name="skybox",          type="composer", duration=2000},
    {name="mask_triangles",  type="composer", duration=2000},
    {name="widget_mask",     type="composer", duration=2000},
    {name="switch_a",        type="composer", duration=2000},
    {name="physics_uaf",     type="composer", duration=2500},
    {name="capture_color",   type="composer", duration=2000},
    {name="custom_shader",   type="composer", duration=2000},
    {name="define_effect_nodot", type="test", duration=2000},
    {name="batching",        type="composer", duration=2000},
    {name="texture_byteorder",type="composer", duration=2000},
    {name="effect_chain",    type="composer", duration=2000},
}

local results = {}
local currentIndex = 0
local testStartTime = 0

local statusText = display.newText({
    text = "Regression: starting...",
    x = display.contentCenterX, y = 15,
    font = native.systemFontBold, fontSize = 11
})
statusText:setFillColor(1, 1, 0)

local function logResult(sceneName, status, detail)
    local entry = {
        scene = sceneName,
        status = status,
        detail = detail or "",
        time = system.getTimer() - testStartTime
    }
    table.insert(results, entry)

    local icon = status == "PASS" and "[PASS]" or "[FAIL]"
    print(string.format("  %s %s %s (%.0fms)", icon, sceneName, detail, entry.time))
end

-- Write results to both /tmp (desktop) and Documents (Android FTL pickup)
local function writeResults(summary)
    -- Desktop path
    local f1 = io.open("/tmp/regression_" .. backend .. ".txt", "w")
    if f1 then
        f1:write(summary)
        f1:close()
    end

    -- Android/Documents path (FTL can pull this)
    local docPath = system.pathForFile("regression_" .. backend .. ".txt", system.DocumentsDirectory)
    if docPath then
        local f2 = io.open(docPath, "w")
        if f2 then
            f2:write(summary)
            f2:close()
            print("[REGRESSION] Saved to: " .. docPath)
        end
    end
end

local function finishTests()
    local pass, fail = 0, 0
    for _, r in ipairs(results) do
        if r.status == "PASS" then pass = pass + 1 else fail = fail + 1 end
    end

    local summary = string.format(
        "=== REGRESSION RESULTS (%s) ===\n" ..
        "Pass: %d / %d | Fail: %d\n",
        backend, pass, #results, fail
    )

    for _, r in ipairs(results) do
        local icon = r.status == "PASS" and "OK" or "FAIL"
        summary = summary .. string.format("  [%s] %-18s %s\n", icon, r.scene, r.detail)
    end
    summary = summary .. "=== END ===\n"
    print(summary)

    writeResults(summary)

    statusText.text = string.format("Done: %d pass, %d fail", pass, fail)
    statusText:setFillColor(fail == 0 and 0 or 1, fail == 0 and 1 or 0, 0)
    local exitCode = (fail == 0) and 0 or 1
    print("[REGRESSION] exiting with code " .. exitCode)
    timer.performWithDelay(500, function() _origOsExit(exitCode) end)
end

-- Remove leaked stage children only when previous test was type=test
-- (composer scenes manage their own children via composer.gotoScene; clearing
-- stage during composer→composer transitions breaks composer internal state
-- and causes "?:0: attempt to call method 'toFront' (a nil value)" errors)
local function cleanupStage(prevWasTest)
    -- composer→composer transitions: composer manages its own scene lifecycle,
    -- including listeners attached to scene views (auto-cancelled on hide/destroy).
    -- Do NOT touch stage children, listeners, or scene module cache here — any
    -- intervention causes Lua errors in composer's internal state machine.
    if not prevWasTest then
        collectgarbage("collect")
        return
    end

    -- prev was a "test" type — it created display objects directly on the
    -- global stage and may have added Runtime listeners. Sweep both.
    resetTestSideEffects()
    local stage = display.getCurrentStage()
    for i = stage.numChildren, 1, -1 do
        local child = stage[i]
        if child ~= statusText and child ~= composer.stage then
            display.remove(child)
        end
    end
    -- Clear test_* module cache so next test gets a fresh require.
    for name, _ in pairs(package.loaded) do
        if name:match("^test_") then
            package.loaded[name] = nil
        end
    end
    collectgarbage("collect")
end

local function loadNextScene()
    -- Capture prev test type BEFORE incrementing so cleanupStage knows whether
    -- to wipe stage (only do so transitioning out of a "test" type that may
    -- have leaked display objects onto the global stage).
    local prevWasTest = (currentIndex > 0 and scenes[currentIndex] and scenes[currentIndex].type == "test")

    currentIndex = currentIndex + 1
    if currentIndex > #scenes then
        finishTests()
        return
    end

    local sceneInfo = scenes[currentIndex]
    local sceneName = sceneInfo.name
    local duration = sceneInfo.duration or 2000

    statusText.text = string.format("Testing %d/%d: %s (%s)", currentIndex, #scenes, sceneName, sceneInfo.type)
    testStartTime = system.getTimer()

    cleanupStage(prevWasTest)

    local ok, err = pcall(function()
        if sceneInfo.type == "composer" then
            composer.gotoScene("scene_" .. sceneName)
        else
            -- test type: set up env then require
            if sceneName == "scene_switch" then
                _G.REPRO_AUTO = "1"
                _G.REPRO_CYCLES = 5
                _G.REPRO_LEVEL = 2
            end
            -- Load empty composer scene first to clear any lingering scene state
            composer.gotoScene("scene_empty")
            -- Require test module (will create its own display objects)
            require("test_" .. sceneName)
        end
    end)

    if not ok then
        logResult(sceneName, "FAIL", "load error: " .. tostring(err))
        timer.performWithDelay(100, loadNextScene)
        return
    end

    timer.performWithDelay(duration, function()
        logResult(sceneName, "PASS", "rendered ok")
        timer.performWithDelay(100, loadNextScene)
    end)
end

-- Start after a short delay
timer.performWithDelay(500, loadNextScene)
