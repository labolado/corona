--[[
    input_recorder.lua — Touch event recording & replay for Solar2D

    Usage:
        local recorder = require("lib.input_recorder")

        recorder.startRecord()          -- begin recording
        recorder.stopRecord()           -- stop & save to file
        recorder.startReplay("file")    -- replay from file
        recorder.onReplayDone = fn      -- callback when replay ends

    Auto mode (via env vars in main.lua):
        SOLAR2D_RECORD=1        → auto record, save on exit
        SOLAR2D_REPLAY=file.json → auto replay on start

    Works on: Android, iOS, macOS — pure Lua, no platform deps.
--]]

local json = require("json")
local M = {}

-- State
local recording = false
local replaying = false
local events = {}
local sceneEvents = {}
local metaInfo = {}
local startTime = 0
local replayTimers = {}

-- Config
local RECORDINGS_DIR = "recordings"

-- Ensure recordings directory exists
local function ensureDir()
    local path = system.pathForFile(RECORDINGS_DIR, system.DocumentsDirectory)
    if not path then
        -- Create directory
        local lfs = require("lfs")
        local base = system.pathForFile("", system.DocumentsDirectory)
        lfs.mkdir(base .. "/" .. RECORDINGS_DIR)
    end
end

-- Get timestamp in ms since recording started
local function elapsed()
    return math.floor((system.getTimer() - startTime))
end

-- Touch listener for recording
local function onTouch(event)
    if not recording then return false end
    events[#events + 1] = {
        time = elapsed(),
        phase = event.phase,
        x = math.floor(event.x),
        y = math.floor(event.y),
        id = event.id or 0,
    }
    return false -- don't consume, let normal handling continue
end

-- Scene change listener
local function onScene(event)
    if not recording then return end
    local name = event.sceneName or event.name or "?"
    sceneEvents[#sceneEvents + 1] = {
        time = elapsed(),
        type = "scene",
        phase = event.phase or "?",
        name = name,
    }
end

---------------------------------------------------------------
-- Public API
---------------------------------------------------------------

function M.startRecord()
    if recording then return end
    ensureDir()
    events = {}
    sceneEvents = {}
    startTime = system.getTimer()
    recording = true

    -- Record meta info (C++ compatible format)
    metaInfo = {
        version = 1,
        platform = system.getInfo("platform"),
        model = system.getInfo("model"),
        backend = os.getenv("SOLAR2D_BACKEND") or "unknown",
        screenWidth = display.contentWidth,
        screenHeight = display.contentHeight,
        timestamp = os.date("%Y%m%d_%H%M%S"),
    }

    Runtime:addEventListener("touch", onTouch)

    -- Hook composer scene events if available
    pcall(function()
        local composer = require("composer")
        composer:addEventListener("show", onScene)
        composer:addEventListener("hide", onScene)
    end)

    print("[InputRecorder] Recording started")
end

function M.stopRecord()
    if not recording then return nil end
    recording = false
    Runtime:removeEventListener("touch", onTouch)

    pcall(function()
        local composer = require("composer")
        composer:removeEventListener("show", onScene)
        composer:removeEventListener("hide", onScene)
    end)

    -- Generate filename
    local ts = os.date("%Y%m%d_%H%M%S")
    local filename = "rec_" .. ts .. ".json"
    local filepath = system.pathForFile(
        RECORDINGS_DIR .. "/" .. filename, system.DocumentsDirectory)

    if filepath then
        local f = io.open(filepath, "w")
        if f then
            -- Save in C++ compatible format: {meta: {}, events: []}
            local data = {
                meta = metaInfo,
                events = events,
            }
            f:write(json.encode(data))
            f:close()
            print("[InputRecorder] Saved " .. #events .. " events to " .. filename)
            return filename
        end
    end

    print("[InputRecorder] ERROR: Failed to save recording")
    return nil
end

function M.startReplay(filename)
    if replaying then return end

    local filepath = system.pathForFile(
        RECORDINGS_DIR .. "/" .. filename, system.DocumentsDirectory)
    if not filepath then
        print("[InputRecorder] ERROR: File not found: " .. filename)
        return false
    end

    local f = io.open(filepath, "r")
    if not f then
        print("[InputRecorder] ERROR: Cannot open: " .. filename)
        return false
    end

    local data = f:read("*a")
    f:close()

    local ok, decoded = pcall(json.decode, data)
    if not ok or not decoded then
        print("[InputRecorder] ERROR: Invalid JSON in " .. filename)
        return false
    end

    -- Handle both formats: nested {meta, events} and flat array
    local replayEvents
    local replayMeta
    if type(decoded) == "table" and decoded.events then
        replayEvents = decoded.events
        replayMeta = decoded.meta
    else
        replayEvents = decoded
    end

    replaying = true
    replayTimers = {}

    -- Print meta info if available
    if replayMeta then
        print("[InputRecorder] Original: " .. (replayMeta.platform or "?") ..
              " " .. (replayMeta.model or "?") ..
              " " .. (replayMeta.screenWidth or replayMeta.w or "?") .. "x" ..
              (replayMeta.screenHeight or replayMeta.h or "?") ..
              " backend=" .. (replayMeta.backend or "?"))
    end

    print("[InputRecorder] Replaying " .. #replayEvents .. " events from " .. filename)

    for i, ev in ipairs(replayEvents) do
        -- Support both "time" (C++ format) and "t" (legacy Lua format)
        local evTime = ev.time or ev.t or 0
        -- Skip non-touch events (meta, scene, log)
        local isTouch = ev.type == "touch" or (ev.phase and not ev.type)
        if isTouch then
            local tid = timer.performWithDelay(evTime, function()
                local touchEvent = {
                    name = "touch",
                    phase = ev.phase,
                    x = ev.x,
                    y = ev.y,
                    xStart = ev.x,
                    yStart = ev.y,
                    id = ev.id or 0,
                    time = system.getTimer(),
                }
                Runtime:dispatchEvent(touchEvent)
            end)
            replayTimers[#replayTimers + 1] = tid
        end
    end

    -- Find last event time for completion callback
    local maxTime = 0
    for _, ev in ipairs(replayEvents) do
        local t = ev.time or ev.t or 0
        if t > maxTime then maxTime = t end
    end

    timer.performWithDelay(maxTime + 500, function()
        replaying = false
        replayTimers = {}
        print("[InputRecorder] Replay complete")
        if M.onReplayDone then M.onReplayDone() end
    end)

    return true
end

function M.stopReplay()
    if not replaying then return end
    for _, tid in ipairs(replayTimers) do
        timer.cancel(tid)
    end
    replayTimers = {}
    replaying = false
    print("[InputRecorder] Replay stopped")
end

function M.isRecording() return recording end
function M.isReplaying() return replaying end
function M.getEventCount() return #events end

-- List saved recordings
function M.listRecordings()
    local lfs = require("lfs")
    local dir = system.pathForFile(RECORDINGS_DIR, system.DocumentsDirectory)
    if not dir then return {} end
    local files = {}
    for file in lfs.dir(dir) do
        if file:match("^rec_.*%.json$") then
            files[#files + 1] = file
        end
    end
    table.sort(files)
    return files
end

-- Auto-setup from environment variables (call from main.lua)
function M.autoSetup()
    local record = os.getenv("SOLAR2D_RECORD")
    local replay = os.getenv("SOLAR2D_REPLAY")

    if record == "1" or record == "yes" then
        M.startRecord()
        -- Save on app exit
        Runtime:addEventListener("system", function(event)
            if event.type == "applicationExit" or event.type == "applicationSuspend" then
                if recording then
                    M.stopRecord()
                end
            end
        end)
    elseif replay and replay ~= "" then
        -- Delay replay slightly to let app initialize
        timer.performWithDelay(1000, function()
            M.startReplay(replay)
        end)
    end
end

return M
