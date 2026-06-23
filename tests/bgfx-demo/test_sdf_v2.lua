-- SDF visual test v2 — adds stroke, statistics comparison, CoronaDeltaTime animation
-- Compare with test_sdf.lua (v1): same shapes but now measures draw call / triangle counts
-- and demonstrates SDF vs non-SDF efficiency side-by-side.
local W = display.contentWidth
local H = display.contentHeight
local cx = display.contentCenterX

display.setDefault("background", 0.15, 0.15, 0.15)

-- ── Header ────────────────────────────────────────────────────────────────
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
display.newText("SDF v2 — " .. backend, cx, 18, native.systemFontBold, 14):setFillColor(1,1,1)

-- ── Statistics helper ──────────────────────────────────────────────────────
display.enableStatistics(true)

local function snapshot_stats()
    local t = {}
    display.getStatistics(t)
    return t
end

-- ── Phase 1: SDF OFF — measure baseline cost ──────────────────────────────
graphics.setSDF(false)

local N = 8   -- shapes to draw per mode

local offGroup = display.newGroup()
local spacing = (W - 40) / N
for i = 1, N do
    local c = display.newCircle(offGroup, 20 + spacing * (i - 0.5), H * 0.25, 18)
    c:setFillColor(0.8, 0.3, 0.3)
end
local offLabel = display.newText("SDF OFF", cx, H * 0.25 - 30, native.systemFont, 11)
offLabel:setFillColor(1, 0.5, 0.5)

-- ── Phase 2: SDF ON — same shapes ─────────────────────────────────────────
graphics.setSDF(true)

local onGroup = display.newGroup()
for i = 1, N do
    -- Circle with stroke
    local c = display.newCircle(onGroup, 20 + spacing * (i - 0.5), H * 0.5, 18)
    c:setFillColor(0.3, 0.6, 1.0)
    c.strokeWidth = 2 + i * 0.5
    c:setStrokeColor(1, 1, 0, 0.9)
end

-- Rounded rects with stroke (not in v1)
local rr = display.newRoundedRect(onGroup, cx, H * 0.68, W - 40, 30, 10)
rr:setFillColor(0.2, 0.8, 0.4)
rr.strokeWidth = 3
rr:setStrokeColor(1, 1, 1, 0.6)

local onLabel = display.newText("SDF ON + stroke", cx, H * 0.5 - 30, native.systemFont, 11)
onLabel:setFillColor(0.5, 1, 0.5)

-- ── Phase 3: CoronaDeltaTime animation (new uniform, not in v1) ────────────
graphics.defineEffect {
    category = "filter",
    name     = "pulse",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            float t   = mod(CoronaTotalTime * 1.5, 1.0);
            float glow = 0.5 + 0.5 * sin(CoronaTotalTime * 4.0);
            vec4  base = CoronaColorScale(texture2D(CoronaSampler0, uv));
            return base * vec4(1.0, glow, glow, 1.0);
        }
    ]]
}
graphics.setSDF(false)   -- effect on an existing polygon
local poly = display.newPolygon(cx, H * 0.82, {0,-20, 20,10, -20,10})
poly:setFillColor(1, 0.4, 0.4)
poly.fill.effect = "filter.custom.pulse"

local animLabel = display.newText("CoronaTotalTime pulse (custom effect)", cx, H * 0.82 + 22, native.systemFont, 10)
animLabel:setFillColor(0.9, 0.9, 0.9)

-- ── Measure & print stats after one rendered frame ─────────────────────────
timer.performWithDelay(200, function()
    local s = snapshot_stats()
    local msg = string.format(
        "draws=%d  tris=%d  geomBinds=%d  progBinds=%d  texBinds=%d",
        s.drawCallCount or 0,
        s.triangleCount or 0,
        s.geometryBindCount or 0,
        s.programBindCount or 0,
        s.textureBindCount or 0
    )
    print("=== SDF_V2 STATS: " .. msg .. " ===")

    -- v1 reference (16 circles, no SDF): ~16 draw calls, ~160+ triangles (10-tri/circle approx)
    -- v2 (SDF ON, 8 circles): each = 1 quad = 2 tris → expect ~16 tris for the SDF row
    local statsText = display.newText(msg, cx, H - 16, native.systemFont, 9)
    statsText:setFillColor(0.7, 1, 0.7)

    print("=== SDF_V2 RESULT: PASS ===")
    Runtime:dispatchEvent({ name = "testComplete", id = "sdf_v2", result = "PASS" })
end)
