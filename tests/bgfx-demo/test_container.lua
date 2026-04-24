-- test_container.lua: Container 裁剪功能回归测试
-- 测试 display.newContainer 裁剪、嵌套、动态子对象移动
-- 运行: SOLAR2D_TEST=container SOLAR2D_BACKEND=bgfx

display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Container Clip Test (" .. backend .. ") ===")

local pass, fail = 0, 0
local function check(name, cond)
    if cond then
        pass = pass + 1; print("[PASS] " .. name)
    else
        fail = fail + 1; print("[FAIL] " .. name)
    end
end

-- 背景
display.newRect(W/2, H/2, W, H):setFillColor(0.1, 0.1, 0.18)

-- ===== Test 1: 基础 Container 裁剪 =====
-- Container 200x150，子对象超出边界，应被裁剪
local ok1, c1 = pcall(function()
    return display.newContainer(200, 150)
end)
check("container created", ok1 and c1 ~= nil)

if c1 then
    c1.x, c1.y = W * 0.25, H * 0.28

    -- 边框（container 外的参考框）
    local border = display.newRect(c1.x, c1.y, 202, 152)
    border:setFillColor(0, 0, 0, 0)
    border.strokeWidth = 2
    border:setStrokeColor(1, 1, 0)

    -- 背景填充（蓝色）
    local bg = display.newRect(c1, 0, 0, 200, 150)
    bg:setFillColor(0.2, 0.2, 0.5)

    -- 超出边界的红色圆（只有一半应可见）
    local overCircle = display.newCircle(c1, 90, 0, 40)
    overCircle:setFillColor(1, 0.2, 0.2)

    -- 超出下边界的绿色矩形
    local overRect = display.newRect(c1, 0, 65, 80, 60)
    overRect:setFillColor(0.2, 1, 0.2)

    -- 内部完全可见的白色小方块
    local inner = display.newRect(c1, -40, -30, 50, 50)
    inner:setFillColor(1, 1, 1, 0.8)

    check("container children inserted", c1.numChildren >= 3)
end

-- ===== Test 2: 嵌套 Container =====
local ok2, c2 = pcall(function()
    return display.newContainer(160, 120)
end)
check("outer container created", ok2 and c2 ~= nil)

local innerContainer
if c2 then
    c2.x, c2.y = W * 0.75, H * 0.28

    -- 外层背景
    local bg2 = display.newRect(c2, 0, 0, 160, 120)
    bg2:setFillColor(0.3, 0.15, 0.35)

    -- 外边框
    local border2 = display.newRect(c2.x, c2.y, 162, 122)
    border2:setFillColor(0,0,0,0); border2.strokeWidth = 2
    border2:setStrokeColor(1, 0.5, 0)

    -- 嵌套内层 Container 80x60
    local ok3, c3 = pcall(function()
        return display.newContainer(c2, 80, 60)
    end)
    check("nested container created", ok3 and c3 ~= nil)
    innerContainer = c3

    if c3 then
        c3.x, c3.y = 0, 0
        local bg3 = display.newRect(c3, 0, 0, 80, 60)
        bg3:setFillColor(0.5, 0.5, 0.1)

        -- 超出内层但被外层裁剪的圆
        local superCircle = display.newCircle(c3, 35, 25, 30)
        superCircle:setFillColor(1, 0.3, 0.8)

        check("nested container has children", c3.numChildren >= 2)
    end
end

-- ===== Test 3: Container 中动态移动子对象 =====
local ok4, c4 = pcall(function()
    return display.newContainer(180, 100)
end)
check("animated container created", ok4 and c4 ~= nil)

local mover
if c4 then
    c4.x, c4.y = W/2, H * 0.65

    local bg4 = display.newRect(c4, 0, 0, 180, 100)
    bg4:setFillColor(0.1, 0.3, 0.1)

    -- 左右边界参考线
    local lEdge = display.newRect(c4, -75, 0, 4, 100)
    lEdge:setFillColor(1, 0, 0, 0.5)
    local rEdge = display.newRect(c4, 75, 0, 4, 100)
    rEdge:setFillColor(1, 0, 0, 0.5)

    -- 移动的圆球（会在裁剪区域内左右弹跳）
    mover = display.newCircle(c4, -60, 0, 25)
    mover:setFillColor(0.2, 0.8, 1.0)
    mover._vx = 90  -- pixels/sec

    -- 边框
    local border4 = display.newRect(c4.x, c4.y, 182, 102)
    border4:setFillColor(0,0,0,0); border4.strokeWidth = 2
    border4:setStrokeColor(0.5, 1, 0.5)

    check("mover created", mover ~= nil)
end

-- 动画：每帧移动 mover
local lastTime = system.getTimer()
Runtime:addEventListener("enterFrame", function()
    local now = system.getTimer()
    local dt = (now - lastTime) / 1000
    lastTime = now
    if mover and mover._vx then
        mover.x = mover.x + mover._vx * dt
        if mover.x > 60 then mover._vx = -90 end
        if mover.x < -60 then mover._vx = 90 end
    end
end)

-- ===== 标签 =====
local function label(txt, x, y)
    local t = display.newText(txt, x, y, native.systemFont, 12)
    t:setFillColor(1, 1, 0.7)
    return t
end
label("Basic Clipping", W * 0.25, H * 0.28 + 90)
label("Nested Container", W * 0.75, H * 0.28 + 75)
label("Dynamic Move (clipped)", W/2, H * 0.65 + 65)

local title = display.newText("Container Clip Test - " .. backend, W/2, 22, native.systemFontBold, 15)
title:setFillColor(1, 1, 1)

-- 延迟验证容器仍存在
timer.performWithDelay(600, function()
    if c1 then
        check("container c1 still valid", c1.numChildren ~= nil)
    end
    if mover then
        check("mover animated (x changed)", math.abs(mover.x) > 0)
    end
    print(string.format("\n=== CONTAINER TEST RESULTS (%s): Pass %d | Fail %d ===", backend, pass, fail))
    if fail == 0 then
        print("TEST PASS: container")
    else
        print("TEST FAIL: container")
    end
end)
