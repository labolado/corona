-- test_capture_flash.lua
-- 复现 capture 导致的黑闪问题
-- 用法: SOLAR2D_TEST=capture_flash SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...
--
-- 测试方法：显示一个彩色背景，定时调 display.save()，观察是否闪黑

local W = display.contentWidth
local H = display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"

-- 鲜艳背景色，闪黑很容易看到
display.setDefault("background", 0, 0.8, 1)

local label = display.newText("Backend: " .. backend .. "\nWatching for black flash...", W/2, 80, native.systemFont, 16)
label:setFillColor(1)

-- 画一些内容
for i = 1, 5 do
    local r = display.newRect(W/2, 120 + i * 60, 200, 40)
    r:setFillColor(math.random(), math.random(), math.random())
end

local countLabel = display.newText("Captures: 0", W/2, H - 40, native.systemFont, 18)
countLabel:setFillColor(1)

local count = 0

-- 每 2 秒做一次 display.save，模拟坦克的 shadow/save 调用
local function doCapture()
    count = count + 1
    countLabel.text = "Captures: " .. count

    -- 这就是触发 CaptureFrameBuffer 的操作
    local group = display.newGroup()
    local rect = display.newRect(group, 0, 0, 100, 100)
    rect:setFillColor(1, 0, 0)

    display.save(group, {
        filename = "flash_test_" .. count .. ".png",
        baseDir = system.TemporaryDirectory,
    })
    group:removeSelf()

    print("FLASH_TEST: capture #" .. count .. " done")
end

-- 连续做 2 次（模拟坦克的 2 次 capture）
local function doPairCapture()
    doCapture()
    doCapture()
    print("FLASH_TEST: pair capture done, watch for black flash!")
end

-- 首次 1 秒后开始，之后每 1 秒重复
timer.performWithDelay(1000, doPairCapture)
timer.performWithDelay(2000, doPairCapture, 0)

print("FLASH_TEST: Running with backend=" .. backend)
print("FLASH_TEST: Will do paired display.save() every 4s")
print("FLASH_TEST: If screen flashes black = bug confirmed")
