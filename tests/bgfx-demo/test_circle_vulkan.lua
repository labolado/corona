-- test_circle_vulkan.lua
-- 用于诊断 Vulkan 圆形变方形问题
-- 运行: SOLAR2D_TEST=circle_vulkan SOLAR2D_BACKEND=bgfx ./Corona\ Simulator -no-console YES tests/bgfx-demo

local w, h = display.contentWidth, display.contentHeight
local cx, cy = w / 2, h / 2

print("[CIRCLE_TEST] START w=" .. w .. " h=" .. h)

-- 背景
local bg = display.newRect(cx, cy, w, h)
bg:setFillColor(0.1, 0.1, 0.1)

local y = 60

local function label(text, yPos)
    local t = display.newText(text, cx, yPos, native.systemFont, 14)
    t:setFillColor(1, 1, 1)
    return t
end

-- ============================================================
-- TEST 1: 单独圆形（无其他对象在前）
-- 这是最简单的用例——如果这个也是方形，说明根本问题在几何体
-- ============================================================
label("T1: 单独圆形 (r=30)", y)
y = y + 40

local circle1 = display.newCircle(cx - 100, y + 30, 30)
circle1:setFillColor(1, 0, 0)  -- 红色实心圆

local circle2 = display.newCircle(cx + 100, y + 30, 30)
circle2:setFillColor(0, 1, 0)  -- 绿色实心圆

label("红/绿 = 圆 ✅  方 ❌", y + 80)
y = y + 120

-- ============================================================
-- TEST 2: 矩形之后的圆形（模拟 mech studio 渲染顺序）
-- 矩形先占用 pool 顶点偏移，圆形在 offset>0 时渲染
-- ============================================================
label("T2: 矩形之后的圆形", y)
y = y + 30

-- 先画 N 个矩形，让 pool 顶点偏移变大
local rects = {}
for i = 1, 8 do
    local r = display.newRect(30 + (i-1)*30, y + 15, 25, 25)
    r:setFillColor(0.3, 0.3, 0.8)
    rects[i] = r
end

y = y + 50

local circle3 = display.newCircle(cx - 80, y + 30, 35)
circle3:setFillColor(1, 0.5, 0)  -- 橙色

local circle4 = display.newCircle(cx + 80, y + 30, 35)
circle4:setFillColor(0, 0.8, 1)  -- 青色

label("矩形后的圆: 圆 ✅  方 ❌", y + 80)
y = y + 130

-- ============================================================
-- TEST 3: display.newRoundedRect (r = half size)
-- ============================================================
label("T3: RoundedRect (r=半宽)", y)
y = y + 35

local rr = display.newRoundedRect(cx, y + 30, 60, 60, 30)  -- r=半宽 = circle
rr:setFillColor(1, 0, 1)  -- 紫色

label("紫色 RoundedRect: 圆 ✅  方 ❌", y + 80)
y = y + 120

-- ============================================================
-- TEST 4: 大圆 + 小圆（验证不同大小）
-- ============================================================
label("T4: 不同半径", y)
y = y + 30

local sizes = {10, 20, 40, 60}
for i, r in ipairs(sizes) do
    local c = display.newCircle(60 * i, y + 50, r)
    c:setFillColor(i * 0.25, 1 - i * 0.2, 0.5)
end

label("多尺寸: 全圆 ✅  任何方形 ❌", y + 120)

print("[CIRCLE_TEST] Objects created, watching for square shapes")
print("[CIRCLE_TEST] If any circle appears as diamond/square, it's the Vulkan TRISTRIP bug")
