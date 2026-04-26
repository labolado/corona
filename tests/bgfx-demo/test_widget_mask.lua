--[[
    test_widget_mask.lua - Widget + Mask Heavy Stress Test

    Usage:
      SOLAR2D_TEST=widget_mask SOLAR2D_BACKEND=bgfx ./Corona\ Simulator -no-console YES tests/bgfx-demo

    Provides a correct baseline for measuring widget mask batch rate.
    Default bgfx-demo barely uses widget/mask, so prior "mask-opt 0 gain"
    measurement (issue 007) was on wrong baseline.

    Scene composition:
      - 5 widget.newScrollView (each with 20+ children) using maskFile
      - 3 widget.newTabBar (multi-button)
      - 2 widget.newButton groups (10 buttons total, scattered)
      - manual setMask matrices: 8x8 grid of independently-masked rects
        + nested setMask groups (3 levels deep) to exercise MaskCount 1/2/3
      - low-velocity rotation/translation to make mask matrices vary frame-to-frame

    BatchStats output is automatically dumped to stderr every 300 frames
    by Rtt_BgfxCommandBuffer::DumpBatchStats(). We additionally sample
    graphics.getDirtyStats() each frame and print a summary at 30s.

    Environment:
      SOLAR2D_BATCH=0       - disable batching (sanity baseline)
      SOLAR2D_MASK_BATCH=0  - disable mask-opt algorithm (only honored on
                              worker/mask-opt branch, no-op on bgfx-solar2d)
--]]

display.setStatusBar(display.HiddenStatusBar)

local backend = os.getenv("SOLAR2D_BACKEND") or "bgfx"
local batchEnv = os.getenv("SOLAR2D_BATCH")
local maskBatchEnv = os.getenv("SOLAR2D_MASK_BATCH")

print("=== Widget Mask Stress Test ===")
print("Backend: " .. backend)
print("SOLAR2D_BATCH: " .. (batchEnv or "(default ON)"))
print("SOLAR2D_MASK_BATCH: " .. (maskBatchEnv or "(default ON, no-op on bgfx-solar2d)"))

local widget = require("widget")

local W = display.contentWidth
local H = display.contentHeight

-- Background
local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.04, 0.05, 0.08)

-- Top status bar
local status = display.newText({
    text = "Widget Mask Stress | " .. backend ..
           " | BATCH=" .. (batchEnv == "0" and "OFF" or "ON") ..
           " MASK_BATCH=" .. (maskBatchEnv == "0" and "OFF" or "ON"),
    x = W/2, y = 14,
    font = native.systemFontBold, fontSize = 11,
})
status:setFillColor(0.95, 0.95, 0.95)

-- Live stats text (bottom)
local liveStats = display.newText({
    text = "[stats]",
    x = W/2, y = H - 14,
    font = native.systemFont, fontSize = 9,
})
liveStats:setFillColor(0.4, 1, 0.6)

-- ============================================================
-- Reusable mask file (built into bgfx-demo)
-- ============================================================
local MASK_FILE = "test_mask_circle.png"  -- 128x128 RGBA white-circle mask

-- ============================================================
-- Block 1: 5 widget.newScrollView, each with 20+ children
-- ScrollView with maskFile triggers scrollView:setMask(...) internally.
-- All children render under that mask -> MaskCount==1 draws.
-- ============================================================
local scrollViews = {}
local scrollChildrenAll = {}

local function buildScrollView(idx, x, y, w, h)
    local sv = widget.newScrollView{
        left = x, top = y,
        width = w, height = h,
        scrollWidth = w * 2,
        scrollHeight = h * 3,
        maskFile = MASK_FILE,
        bgColor = { 0.10 + idx*0.02, 0.12, 0.18, 1 },
        hideBackground = false,
        hideScrollBar = true,
    }

    -- Pack 20+ children inside (mix of rects, text, image rects, rounded)
    for i = 1, 22 do
        local cx = (i % 4) * (w / 4) + 14
        local cy = math.floor((i - 1) / 4) * 24 + 12
        local kind = i % 5
        local child
        if kind == 0 then
            child = display.newRect(cx, cy, w/4 - 6, 18)
            child:setFillColor(0.4 + (idx*0.05) % 0.5, 0.5, 0.8 - (i*0.02) % 0.4)
        elseif kind == 1 then
            child = display.newRoundedRect(cx, cy, w/4 - 6, 18, 4)
            child:setFillColor(0.7, 0.4 + (i*0.04) % 0.5, 0.4)
        elseif kind == 2 then
            child = display.newImageRect("test_cyan.png", w/4 - 6, 18)
            child.x, child.y = cx, cy
        elseif kind == 3 then
            child = display.newImageRect("test_magenta.png", w/4 - 6, 18)
            child.x, child.y = cx, cy
        else
            child = display.newText{ text = "row" .. i,
                x = cx, y = cy, font = native.systemFont, fontSize = 9 }
            child:setFillColor(0.9, 0.9, 0.7)
        end
        sv:insert(child)
        table.insert(scrollChildrenAll, child)
    end

    return sv
end

-- 5 ScrollViews laid out 2-column-ish, but we DON'T need them not overlapping
-- (overlap is fine — bg is small contained area).
local svPositions = {
    {  6,  40,  90, 80 },
    { 110, 40,  90, 80 },
    { 214, 40,  92, 80 },
    {  20, 130, 130, 80 },
    { 168, 130, 130, 80 },
}
for i, p in ipairs(svPositions) do
    scrollViews[i] = buildScrollView(i, p[1], p[2], p[3], p[4])
end

-- ============================================================
-- Block 2: 3 widget.newTabBar, scattered
-- ============================================================
local tabBars = {}
for i = 1, 3 do
    local tb = widget.newTabBar{
        left = 0,
        top = 220 + (i - 1) * 26,
        width = W,
        height = 22,
        buttons = {
            { id="t"..i.."a", label="A"..i, onPress=function() end, selected=true },
            { id="t"..i.."b", label="B"..i, onPress=function() end },
            { id="t"..i.."c", label="C"..i, onPress=function() end },
            { id="t"..i.."d", label="D"..i, onPress=function() end },
        },
    }
    tabBars[i] = tb
end

-- ============================================================
-- Block 3: 2 widget.newButton groups (10 buttons total)
-- Note: widget.newButton itself does NOT call setMask, but each newButton is
-- a display.newGroup containing rect + label, so it adds program-version
-- transitions when text rendering interleaves with rect rendering.
-- ============================================================
local buttons = {}
for grp = 1, 2 do
    for i = 1, 5 do
        local btn = widget.newButton{
            left = 4 + (i - 1) * (W / 5),
            top = 300 + (grp - 1) * 22,
            width = math.floor(W / 5) - 4,
            height = 18,
            label = "B" .. grp .. "-" .. i,
            cornerRadius = 4,
            defaultColor = { 0.20 + grp * 0.15, 0.35, 0.55 },
            overColor = { 0.35, 0.55, 0.75 },
            labelColor = { default = { 0.9 }, over = { 1.0 } },
            onPress = function() end,
        }
        table.insert(buttons, btn)
    end
end

-- ============================================================
-- Block 4: Manual setMask matrices
-- 8 x 8 grid of independently-masked rects: each rect is its own group with
-- its own setMask, so every rect has a unique mask matrix snapshot.
-- These are the canonical "MaskCount==1, same level" pairs that the GL
-- backend rejects unconditionally and that the mask-opt algorithm tries
-- to merge when matrix snapshots happen to coincide.
-- ============================================================
local maskHandle = graphics.newMask(MASK_FILE)
local maskGrid = display.newGroup()
maskGrid.x = 4
maskGrid.y = 340
for r = 1, 8 do
    for c = 1, 8 do
        local cell = display.newGroup()
        cell.x = (c - 1) * 16 + 8
        cell.y = (r - 1) * 14 + 8
        local rect = display.newRect(cell, 0, 0, 14, 12)
        rect:setFillColor(
            0.4 + ((r * 31 + c * 7) % 60) / 100,
            0.5 + ((r * 13 + c * 17) % 50) / 100,
            0.4 + ((r * 7 + c * 11) % 50) / 100
        )
        cell:setMask(maskHandle)
        cell.maskScaleX = 0.11
        cell.maskScaleY = 0.10
        maskGrid:insert(cell)
    end
end

-- ============================================================
-- Block 5: Nested mask groups (MaskCount 1, 2, 3)
-- Each level wraps a setMask, so children at depth N render with MaskCount=N.
-- Pairs at the same depth with different matrices are exactly what the
-- mask-opt snapshot-compare algorithm targets.
-- ============================================================
local function makeNested(parentX, parentY, depth)
    if depth <= 0 then return end
    local g = display.newGroup()
    g.x = parentX
    g.y = parentY
    -- Some inner content
    for k = 1, 6 do
        local rect = display.newRect(g, k * 8 - 24, 0, 6, 6 + depth)
        rect:setFillColor(0.6, 0.4 + depth * 0.15, 0.35 + depth * 0.10)
    end
    g:setMask(maskHandle)
    g.maskScaleX = 0.5
    g.maskScaleY = 0.4
    -- Recurse
    if depth > 1 then
        local inner = makeNested(0, 4, depth - 1)
        if inner then g:insert(inner) end
    end
    return g
end

local nestedGroups = {}
for i = 1, 6 do
    local outer = makeNested((i - 1) * 50 + 24, 470, 3)
    table.insert(nestedGroups, outer)
end

-- ============================================================
-- Animation: rotate / translate the masked content so mask matrices vary
-- ============================================================
local frameCount = 0
local startTimeMs = system.getTimer()

local function animate(event)
    frameCount = frameCount + 1
    local t = (event.time - startTimeMs) / 1000.0

    -- Slowly rotate the maskGrid
    if maskGrid.rotation ~= nil then
        maskGrid.rotation = (t * 6) % 360
    end

    -- Drift nested groups
    for i, g in ipairs(nestedGroups) do
        if g and g.x then
            g.x = (i - 1) * 50 + 24 + math.sin(t + i * 0.5) * 12
            g.rotation = math.sin(t * 0.3 + i) * 30
        end
    end

    -- Drift scroll children to make their mask intersection vary
    for i, child in ipairs(scrollChildrenAll) do
        if child and child.x then
            child.rotation = (child.rotation or 0) + 0.3
        end
    end

    -- Live stats every 30 frames
    if frameCount % 30 == 0 then
        local s = graphics.getDirtyStats()
        if s and s.batchTotalDraws then
            local saved = 0
            if s.batchTotalDraws > 0 then
                saved = (1 - s.batchActualSubmits / s.batchTotalDraws) * 100
            end
            liveStats.text = string.format(
                "f=%d D=%d->S=%d (%.1f%% saved) batches=%d max=%d",
                frameCount, s.batchTotalDraws, s.batchActualSubmits,
                saved, s.batchCount, s.batchMaxSize
            )
        end
    end
end

Runtime:addEventListener("enterFrame", animate)

-- Trigger getDirtyStats() once on first frame so engine starts collecting
-- batch stats permanently from the next frame.
graphics.getDirtyStats()

-- Auto-exit after 35s so external scripts can capture the run cleanly
-- (BatchStats stderr dump is emitted every 300 frames; at 60fps 35s
-- yields ~7 dump cycles)
local TEST_DURATION_MS = 35000
timer.performWithDelay(TEST_DURATION_MS, function()
    print("[widget_mask] === Test complete after " .. (TEST_DURATION_MS/1000) .. "s ===")
    local s = graphics.getDirtyStats()
    if s and s.batchTotalDraws then
        local saved = 0
        if s.batchTotalDraws > 0 then
            saved = (1 - s.batchActualSubmits / s.batchTotalDraws) * 100
        end
        print(string.format("[widget_mask] FINAL frame stats: D=%d S=%d saved=%.2f%% batches=%d max=%d",
            s.batchTotalDraws, s.batchActualSubmits, saved, s.batchCount, s.batchMaxSize))
    end
    print("[widget_mask] === EXITING ===")
    os.exit(0)
end)

print("[widget_mask] scene built; running for " .. (TEST_DURATION_MS/1000) .. "s")
print(string.format("[widget_mask] objects: %d ScrollViews + %d TabBars + %d Buttons + 64-cell mask grid + 6 nested-3 groups",
    #scrollViews, #tabBars, #buttons))
