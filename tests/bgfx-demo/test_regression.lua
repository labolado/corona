--[[
    test_regression.lua - Full regression test suite

    Usage: SOLAR2D_TEST=regression SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Auto-loads each scene, checks for errors, logs results.
    Outputs structured results to /tmp/regression_results.txt
--]]

display.setStatusBar(display.HiddenStatusBar)

local composer = require("composer")
local backend = os.getenv("SOLAR2D_BACKEND") or "unknown"

print("=== REGRESSION TEST START ===")
print("Backend: " .. backend)
print("Time: " .. os.date())

local scenes = {
    "shapes", "images", "text", "transforms", "blend",
    "animation", "groups", "physics", "masks", "stress"
}

local results = {}
local currentIndex = 0
local testStartTime = 0
local SCENE_DURATION = 2000  -- ms per scene

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

local function finishTests()
    -- Summary
    local pass, fail = 0, 0
    for _, r in ipairs(results) do
        if r.status == "PASS" then pass = pass + 1 else fail = fail + 1 end
    end

    local summary = string.format(
        "\n=== REGRESSION RESULTS (%s) ===\n" ..
        "Pass: %d / %d | Fail: %d\n",
        backend, pass, #results, fail
    )

    for _, r in ipairs(results) do
        local icon = r.status == "PASS" and "OK" or "FAIL"
        summary = summary .. string.format("  [%s] %-12s %s\n", icon, r.scene, r.detail)
    end
    summary = summary .. "=== END ===\n"
    print(summary)

    -- Write to file for scripted comparison
    local f = io.open("/tmp/regression_" .. backend .. ".txt", "w")
    if f then
        f:write(summary)
        f:close()
    end

    statusText.text = string.format("Done: %d pass, %d fail", pass, fail)
    statusText:setFillColor(fail == 0 and 0 or 1, fail == 0 and 1 or 0, 0)
end

local function loadNextScene()
    currentIndex = currentIndex + 1
    if currentIndex > #scenes then
        finishTests()
        return
    end

    local sceneName = scenes[currentIndex]
    statusText.text = string.format("Testing %d/%d: %s", currentIndex, #scenes, sceneName)
    testStartTime = system.getTimer()

    local ok, err = pcall(function()
        composer.gotoScene("scene_" .. sceneName)
    end)

    if not ok then
        logResult(sceneName, "FAIL", "load error: " .. tostring(err))
        timer.performWithDelay(100, loadNextScene)
        return
    end

    -- Wait for scene to render, then check and move on
    timer.performWithDelay(SCENE_DURATION, function()
        -- Scene loaded and rendered without crash = pass
        logResult(sceneName, "PASS", "rendered ok")

        -- Hide current scene before loading next
        timer.performWithDelay(100, loadNextScene)
    end)
end

-- Start after a short delay
timer.performWithDelay(500, loadNextScene)
