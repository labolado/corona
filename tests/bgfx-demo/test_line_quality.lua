-- test_line_quality.lua: Line 视觉质量回归测试
-- 测试水平/垂直/斜线、strokeWidth、多段折线、颜色+alpha、anchorSegments
-- 运行: SOLAR2D_TEST=line_quality SOLAR2D_BACKEND=bgfx

display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Line Quality Test (" .. backend .. ") ===")

local pass, fail = 0, 0
local function check(name, cond)
    if cond then
        pass = pass + 1; print("[PASS] " .. name)
    else
        fail = fail + 1; print("[FAIL] " .. name)
    end
end

-- 背景
display.newRect(W/2, H/2, W, H):setFillColor(0.06, 0.06, 0.12)

-- ===== Section 1: 单线不同宽度（水平线）=====
local y1 = H * 0.12
local widths = {1, 3, 8}
local colors = {{1,1,1}, {1,0.8,0.2}, {0.3,0.8,1}}

for i, sw in ipairs(widths) do
    local lineY = y1 + (i-1) * 22
    local ok = pcall(function()
        local ln = display.newLine(20, lineY, W-20, lineY)
        ln.strokeWidth = sw
        ln:setStrokeColor(colors[i][1], colors[i][2], colors[i][3], 1)
    end)
    check(string.format("horizontal line sw=%d", sw), ok)
end

-- ===== Section 2: 垂直线和斜线 =====
local y2 = H * 0.28

-- 垂直线 1px
local okV = pcall(function()
    local vl = display.newLine(50, y2, 50, y2 + 70)
    vl.strokeWidth = 1
    vl:setStrokeColor(1, 0.5, 0.5, 1)
end)
check("vertical line 1px", okV)

-- 垂直线 4px
local okV2 = pcall(function()
    local vl2 = display.newLine(90, y2, 90, y2 + 70)
    vl2.strokeWidth = 4
    vl2:setStrokeColor(0.5, 1, 0.5, 1)
end)
check("vertical line 4px", okV2)

-- 45° 斜线
local okD = pcall(function()
    local dl = display.newLine(130, y2, 200, y2 + 70)
    dl.strokeWidth = 2
    dl:setStrokeColor(1, 1, 0, 1)
end)
check("diagonal 45deg line", okD)

-- 135° 斜线（反向）
local okD2 = pcall(function()
    local dl2 = display.newLine(240, y2 + 70, 310, y2)
    dl2.strokeWidth = 3
    dl2:setStrokeColor(0.8, 0.2, 1, 1)
end)
check("diagonal 135deg line", okD2)

-- ===== Section 3: 多段折线 + append =====
local y3 = H * 0.50

local polyline, okPoly = nil, false
okPoly = pcall(function()
    polyline = display.newLine(30, y3, 80, y3 - 40)
    polyline:append(130, y3)
    polyline:append(180, y3 - 40)
    polyline:append(230, y3)
    polyline:append(280, y3 - 40)
    polyline.strokeWidth = 3
    polyline:setStrokeColor(0.2, 1, 0.8, 1)
end)
check("polyline with 5 append", okPoly)

-- 封闭折线（近似矩形）
local okRect = pcall(function()
    local rl = display.newLine(W-160, y3-50, W-40, y3-50)
    rl:append(W-40, y3+20)
    rl:append(W-160, y3+20)
    rl:append(W-160, y3-50)
    rl.strokeWidth = 4
    rl:setStrokeColor(1, 0.4, 0.4, 1)
end)
check("closed polyline rectangle", okRect)

-- ===== Section 4: alpha 渐变色测试 =====
local y4 = H * 0.70

for i = 1, 6 do
    local alpha = i / 6
    local ok = pcall(function()
        local al = display.newLine(20 + (i-1)*45, y4, 20 + (i-1)*45 + 35, y4)
        al.strokeWidth = 8
        al:setStrokeColor(1, 0.5, 0, alpha)
    end)
    check(string.format("alpha line %.2f", alpha), ok)
end

-- ===== Section 5: anchorSegments =====
local y5 = H * 0.84
local okAnchor = pcall(function()
    local anl = display.newLine(30, y5, 150, y5)
    anl:append(150, y5 - 40)
    anl.strokeWidth = 5
    anl:setStrokeColor(0.5, 1, 0.5, 1)
    anl.anchorSegments = true
    anl.x = W * 0.3
end)
check("anchorSegments = true no crash", okAnchor)

local okAnchor2 = pcall(function()
    local anl2 = display.newLine(W*0.55, y5, W*0.55+120, y5)
    anl2:append(W*0.55+120, y5 - 40)
    anl2.strokeWidth = 5
    anl2:setStrokeColor(1, 0.5, 0.5, 1)
    anl2.anchorSegments = false
end)
check("anchorSegments = false no crash", okAnchor2)

-- ===== 标签 =====
local function label(txt, x, y)
    local t = display.newText(txt, x, y, native.systemFont, 11)
    t:setFillColor(0.8, 0.8, 1)
    return t
end
label("Horizontal sw=1/3/8", W/2, y1 + 10)
label("Vertical & Diagonal", W/2, y2 - 12)
label("Polyline + append", W/2, y3 - 55)
label("Alpha 1/6 → 6/6", W/2, y4 - 14)
label("anchorSegments", W/2, y5 - 14)

local title = display.newText("Line Quality Test - " .. backend, W/2, 22, native.systemFontBold, 15)
title:setFillColor(1, 1, 1)

-- 汇总
timer.performWithDelay(400, function()
    print(string.format("\n=== LINE QUALITY TEST RESULTS (%s): Pass %d | Fail %d ===", backend, pass, fail))
    if fail == 0 then
        print("TEST PASS: line_quality")
    else
        print("TEST FAIL: line_quality")
    end
end)
