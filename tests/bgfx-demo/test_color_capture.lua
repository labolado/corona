-- test_color_capture.lua
-- 内部截图对比：用 display.captureScreen 读取渲染结果，绕过窗口合成器
-- SOLAR2D_TEST=color_capture

local W = display.contentWidth
local H = display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"

display.setDefault("background", 0, 0, 0)

-- 纯色色块
local colors = {
    { name="Red",   r=1, g=0, b=0 },
    { name="Green", r=0, g=1, b=0 },
    { name="Blue",  r=0, g=0, b=1 },
    { name="Cyan",  r=0, g=1, b=1 },
    { name="White", r=1, g=1, b=1 },
    { name="Gray",  r=0.5, g=0.5, b=0.5 },
}

local bw, bh = 100, 100
local startY = 20
for i, c in ipairs(colors) do
    local y = startY + (i-1) * (bh + 10)
    local rect = display.newRect(bw/2 + 10, y + bh/2, bw, bh)
    rect:setFillColor(c.r, c.g, c.b)
    rect.strokeWidth = 0

    local lbl = display.newText(
        string.format("%s (%d,%d,%d)", c.name, c.r*255, c.g*255, c.b*255),
        bw + 30, y + bh/2, native.systemFont, 12)
    lbl:setFillColor(1)
    lbl.anchorX = 0
end

-- 延迟 1 秒后用 display.save 截图
timer.performWithDelay(1000, function()
    local filename = "color_internal_" .. backend .. ".png"
    display.save(display.currentStage, {
        filename = filename,
        baseDir = system.TemporaryDirectory,
        isFullResolution = true,
    })
    local path = system.pathForFile(filename, system.TemporaryDirectory)
    print("COLOR_CAPTURE: saved to " .. tostring(path))
end)
