-- Polygon SDF test (#68)
-- Covers convex, concave (winding-number), and >16-vert tessellation fallback.
-- SOLAR2D_TEST=sdf_polygon
local W = display.contentWidth
local H = display.contentHeight

display.setDefault("background", 0.15, 0.15, 0.18)

local title = display.newText("Polygon SDF Test (#68)", W/2, 20, native.systemFontBold, 16)
title:setFillColor(1, 1, 1)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local st = display.newText("Backend: " .. backend, W/2, 42, native.systemFont, 13)
st:setFillColor(0.7, 1, 0.5)

graphics.setSDF(true)

-- Regular convex polygon: n verts, radius r
local function regular(n, r)
    local v = {}
    for i = 1, n do
        local a = (i - 1) / n * 2 * math.pi - math.pi / 2
        v[#v + 1] = r * math.cos(a)
        v[#v + 1] = r * math.sin(a)
    end
    return v
end

-- Star (concave): p points, outer ro / inner ri
local function star(p, ro, ri)
    local v = {}
    for i = 0, 2 * p - 1 do
        local a = i / (2 * p) * 2 * math.pi - math.pi / 2
        local r = (i % 2 == 0) and ro or ri
        v[#v + 1] = r * math.cos(a)
        v[#v + 1] = r * math.sin(a)
    end
    return v
end

local y1, y2, y3 = 95, 205, 320
local xs = { 55, 145, 235, 325 }

-- Row 1: convex (SDF path) — triangle / pentagon / hexagon / octagon
local convex = { regular(3, 28), regular(5, 28), regular(6, 28), regular(8, 28) }
for i, v in ipairs(convex) do
    local p = display.newPolygon(xs[i], y1, v)
    p:setFillColor(0.9, 0.7, 0.2)
end

-- Row 2: concave (SDF winding-number) — stars + arrow (all <=16 verts)
local concave = {
    star(5, 30, 13), -- 10 verts
    star(4, 30, 12), -- 8 verts
    star(6, 30, 14), -- 12 verts
    { 0, -30, 10, -10, 4, -10, 4, 30, -4, 30, -4, -10, -10, -10 }, -- arrow, 7 verts
}
for i, v in ipairs(concave) do
    local p = display.newPolygon(xs[i], y2, v)
    p:setFillColor(0.3, 0.8, 0.9)
end

-- Row 3: boundary — 16 verts (SDF) vs 20/24 verts (>16 -> tessellation fallback, must not crash)
local p16 = display.newPolygon(xs[1], y3, regular(16, 28))
p16:setFillColor(0.5, 0.9, 0.4)
local p20 = display.newPolygon(xs[2], y3, regular(20, 28))
p20:setFillColor(0.9, 0.4, 0.5)
local p24 = display.newPolygon(xs[3], y3, regular(24, 28))
p24:setFillColor(0.9, 0.6, 0.4)

local note = display.newText("R1 convex / R2 concave / R3: 16=SDF, 20+24=fallback", W/2, H - 28, native.systemFont, 11)
note:setFillColor(0.6, 0.6, 0.6)
