--[[
    test_recorder.lua — Verify InputRecorder timestamps are non-zero

    Usage: SOLAR2D_TEST=recorder SOLAR2D_BACKEND=bgfx ./Corona\ Simulator -no-console YES tests/bgfx-demo

    Test flow:
    1. Start recording
    2. Simulate a few touch events with delays
    3. Stop recording and verify saved file has non-zero timestamps
--]]

local json = require("json")
local recorder = require("lib.input_recorder")

local PASS = true

local function log(msg)
    print("[TEST] " .. msg)
end

local function fail(msg)
    print("[TEST] FAIL: " .. msg)
    PASS = false
end

-- Start recording
log("Starting recording...")
recorder.startRecord()

-- Simulate touch events with delays
local touchCount = 0
local function simulateTouch(phase, x, y)
    touchCount = touchCount + 1
    local event = {
        name = "touch",
        phase = phase,
        x = x,
        y = y,
        id = touchCount,
    }
    Runtime:dispatchEvent(event)
end

-- First touch at 500ms
timer.performWithDelay(500, function()
    simulateTouch("began", 100, 200)
end)

-- Move at 1000ms
timer.performWithDelay(1000, function()
    simulateTouch("moved", 150, 250)
end)

-- End at 1500ms
timer.performWithDelay(1500, function()
    simulateTouch("ended", 200, 300)
end)

-- Second touch at 2000ms
timer.performWithDelay(2000, function()
    simulateTouch("began", 400, 400)
    simulateTouch("ended", 400, 400)
end)

-- Stop recording and verify at 3000ms
timer.performWithDelay(3000, function()
    log("Stopping recording...")
    local filename = recorder.stopRecord()

    if not filename then
        fail("No recording file created")
        log("Result: " .. (PASS and "PASS" or "FAIL"))
        native.requestExit()
        return
    end

    log("Recording saved: " .. filename)

    -- Read and verify the file
    local filepath = system.pathForFile("recordings/" .. filename, system.DocumentsDirectory)
    local f = io.open(filepath, "r")
    if not f then
        fail("Cannot open recording file: " .. tostring(filepath))
        log("Result: " .. (PASS and "PASS" or "FAIL"))
        native.requestExit()
        return
    end

    local data = f:read("*a")
    f:close()

    local ok, decoded = pcall(json.decode, data)
    if not ok or not decoded then
        fail("Invalid JSON in recording file")
        log("Result: " .. (PASS and "PASS" or "FAIL"))
        native.requestExit()
        return
    end

    -- Verify format: should be {meta: {}, events: []}
    if not decoded.meta then
        fail("Missing 'meta' in recording (not C++ compatible format)")
    else
        log("Meta present: platform=" .. tostring(decoded.meta.platform) ..
            " screenWidth=" .. tostring(decoded.meta.screenWidth))
    end

    if not decoded.events then
        fail("Missing 'events' in recording (not C++ compatible format)")
        log("Result: " .. (PASS and "PASS" or "FAIL"))
        native.requestExit()
        return
    end

    local events = decoded.events
    log("Event count: " .. #events)

    if #events == 0 then
        fail("No events recorded")
        log("Result: " .. (PASS and "PASS" or "FAIL"))
        native.requestExit()
        return
    end

    -- Verify timestamps
    local zeroCount = 0
    local hasTimeField = true
    local prevTime = -1

    for i, ev in ipairs(events) do
        if ev.time == nil then
            fail("Event #" .. i .. " missing 'time' field (has 't'=" .. tostring(ev.t) .. ")")
            hasTimeField = false
        elseif ev.time == 0 then
            zeroCount = zeroCount + 1
        end

        -- Verify monotonically increasing
        local t = ev.time or 0
        if t < prevTime then
            fail("Event #" .. i .. " timestamp " .. t .. " < previous " .. prevTime)
        end
        prevTime = t

        log("  Event #" .. i .. ": time=" .. tostring(ev.time) ..
            " phase=" .. tostring(ev.phase) ..
            " x=" .. tostring(ev.x) .. " y=" .. tostring(ev.y))
    end

    -- First event can be near 0 (if touch happens right after recording starts)
    -- but with 500ms delay, it should be > 0
    if zeroCount > 0 then
        fail(zeroCount .. " events have time=0 (expected non-zero with delayed touches)")
    else
        log("All " .. #events .. " events have non-zero timestamps")
    end

    -- Verify last event timestamp is reasonable (> 1000ms given our delays)
    local lastTime = events[#events].time or 0
    if lastTime < 1000 then
        fail("Last event time " .. lastTime .. "ms is too low (expected > 1000ms)")
    else
        log("Last event at " .. lastTime .. "ms (reasonable)")
    end

    -- Verify 'time' field is used (not 't')
    if events[1].t ~= nil then
        fail("Events use legacy 't' field instead of 'time'")
    end

    log("Result: " .. (PASS and "PASS" or "FAIL"))

    -- Clean up test recording
    os.remove(filepath)

    timer.performWithDelay(500, function()
        native.requestExit()
    end)
end)
