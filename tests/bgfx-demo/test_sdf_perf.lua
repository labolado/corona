-- SDF performance benchmark — FTL scenario 6
-- Measures FPS + draw calls + triangle counts for SDF OFF vs SDF ON
-- with identical 50-shape workload (circles + roundedRects).
--
-- Output markers (parsed by ftl_sdf_perf_collect.sh):
--   SDF_PERF|OFF|fps=XX.X|draws=N|tris=N
--   SDF_PERF|ON|fps=XX.X|draws=N|tris=N
--   SDF_PERF|SPEEDUP|X.XXx
local W, H = display.contentWidth, display.contentHeight
local cx = display.contentCenterX

display.setDefault("background", 0.1, 0.1, 0.1)
display.enableStatistics(true)

-- Try to detect bgfx vs GL from renderer info
local backend = os.getenv("SOLAR2D_BACKEND") or system.getInfo("androidDisplayApproximateDpi") and "android" or "gl"
local renderer = system.getInfo("gpuRenderer") or "?"
print(string.format("SDF_PERF|DEVICE|backend_env=%s|gpu=%s", os.getenv("SOLAR2D_BACKEND") or "nil", renderer))
local label = display.newText("SDF Perf — " .. backend, cx, 14, native.systemFont, 11)
label:setFillColor(1, 1, 1)

-- ── helpers ───────────────────────────────────────────────────────────────

local function get_stats()
    local t = {}; display.getStatistics(t); return t
end

-- Shape count per phase. Keep display list stable so frame cost is pure render.
local SHAPE_N = 50
local WARMUP_MS  = 1500   -- drop frames; GPU warmup
local MEASURE_MS = 8000   -- count frames here

-- ── create shapes ─────────────────────────────────────────────────────────

local function make_shapes(sdf_on)
    graphics.setSDF(sdf_on)
    local actual = graphics.getSDF()
    print(string.format("SDF_PERF|STATE|requested=%s|actual=%s", tostring(sdf_on), tostring(actual)))
    local g = display.newGroup()
    local cols = 10
    local sw = (W - 20) / cols
    local sh = (H - 80) / math.ceil(SHAPE_N / cols)
    for i = 1, SHAPE_N do
        local col = (i - 1) % cols
        local row = math.floor((i - 1) / cols)
        local x = 10 + sw * (col + 0.5)
        local y = 40 + sh * (row + 0.5)
        local r = math.min(sw, sh) * 0.4
        if i % 2 == 0 then
            local c = display.newCircle(g, x, y, r)
            c:setFillColor((i/SHAPE_N), 0.5, 1 - i/SHAPE_N)
        else
            local rr = display.newRoundedRect(g, x, y, sw * 0.8, sh * 0.7, r * 0.3)
            rr:setFillColor(1 - i/SHAPE_N, 0.8, (i/SHAPE_N) * 0.6)
        end
    end
    return g
end

-- ── phase runner ──────────────────────────────────────────────────────────

local results = {}

local function run_phase(phase_name, sdf_on, on_done)
    local g = make_shapes(sdf_on)
    label.text = "Phase: " .. phase_name .. " | backend=" .. backend

    -- warmup
    timer.performWithDelay(WARMUP_MS, function()
        local frame_count = 0
        local sum_draws = 0
        local sum_tris  = 0
        local t0 = system.getTimer()

        local function on_frame()
            frame_count = frame_count + 1
            local s = get_stats()
            sum_draws = sum_draws + (s.drawCallCount or 0)
            sum_tris  = sum_tris  + (s.triangleCount  or 0)
        end
        Runtime:addEventListener("enterFrame", on_frame)

        timer.performWithDelay(MEASURE_MS, function()
            Runtime:removeEventListener("enterFrame", on_frame)
            local elapsed = (system.getTimer() - t0) / 1000
            local fps   = frame_count / elapsed
            local draws = math.floor(sum_draws / math.max(frame_count, 1) + 0.5)
            local tris  = math.floor(sum_tris  / math.max(frame_count, 1) + 0.5)

            local marker = string.format(
                "SDF_PERF|%s|fps=%.1f|draws=%d|tris=%d|frames=%d|elapsed=%.1f",
                phase_name, fps, draws, tris, frame_count, elapsed)
            print(marker)

            results[phase_name] = { fps = fps, draws = draws, tris = tris }
            g:removeSelf()
            on_done()
        end)
    end)
end

-- ── sequence: OFF → ON → summary ─────────────────────────────────────────

run_phase("OFF", false, function()
    run_phase("ON", true, function()
        local off = results["OFF"]
        local on  = results["ON"]
        if off and on and off.fps > 0 then
            local speedup  = on.fps  / off.fps
            local tri_ratio = off.tris / math.max(on.tris, 1)
            print(string.format("SDF_PERF|SPEEDUP|%.2fx|tri_reduction=%.1fx", speedup, tri_ratio))
            print(string.format("SDF_PERF|SUMMARY|backend=%s|off_fps=%.1f|on_fps=%.1f|off_tris=%d|on_tris=%d",
                backend, off.fps, on.fps, off.tris, on.tris))
        end

        local done_label = display.newText(
            string.format("OFF %.1f fps  →  ON %.1f fps", (off or {fps=0}).fps, (on or {fps=0}).fps),
            cx, H - 20, native.systemFont, 12)
        done_label:setFillColor(0.5, 1, 0.5)

        print("=== SDF_PERF COMPLETE ===")
        Runtime:dispatchEvent({ name = "testComplete", id = "sdf_perf", result = "PASS" })
    end)
end)
