-- test_byteorder.lua
-- 测试纹理字节序：验证 bswap32 对文件加载纹理是否正确
-- 用法：SOLAR2D_TEST=byteorder SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...
--
-- 测试方法：
-- 加载已知颜色的 PNG 文件，显示到屏幕，对比 GL vs bgfx
-- 如果 R↔B 互换了，bgfx 下红色会变蓝色

local W = display.contentWidth
local H = display.contentHeight

display.setDefault("background", 0.9, 0.9, 0.9)

local y = 30
local function label(text, x, yy)
    local t = display.newText(text, x, yy, native.systemFont, 12)
    t:setFillColor(0)
    return t
end

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
label("Backend: " .. backend, W/2, y)
y = y + 25

-- ============================================================
-- 测试 1：纯色填充矩形（不走 bitmap 路径，作为参考基准）
-- ============================================================
label("=== Reference: setFillColor ===", W/2, y)
y = y + 18

local refColors = {
    { 1, 0, 0, "Red" },
    { 0, 1, 0, "Green" },
    { 0, 0, 1, "Blue" },
    { 0, 1, 1, "Cyan" },
    { 1, 0, 1, "Magenta" },
    { 1, 1, 0, "Yellow" },
}

local startX = 30
for i, c in ipairs(refColors) do
    local x = startX + (i-1) * 50
    local r = display.newRect(x, y + 20, 40, 30)
    r:setFillColor(c[1], c[2], c[3])
    label(c[4], x, y + 42)
end
y = y + 60

-- ============================================================
-- 测试 2：文件 PNG 纹理（走 CGBitmap → kBGRA → bswap32 路径）
-- ============================================================
label("=== File PNG textures (kBGRA path) ===", W/2, y)
y = y + 18

local fileImages = {
    { "test_red.png", "Red" },
    { "test_green.png", "Green" },
    { "test_blue.png", "Blue" },
    { "test_cyan.png", "Cyan" },
    { "test_magenta.png", "Magenta" },
    { "test_yellow.png", "Yellow" },
}

for i, img in ipairs(fileImages) do
    local x = startX + (i-1) * 50
    local obj = display.newImageRect(img[1], 40, 30)
    if obj then
        obj.x, obj.y = x, y + 20
        label(img[2], x, y + 42)
    else
        label("FAIL", x, y + 20)
    end
end
y = y + 60

-- ============================================================
-- 测试 3：Composite paint（坦克迷彩的技术）
-- ============================================================
label("=== Composite paint ===", W/2, y)
y = y + 18

local composites = {
    { "test_red.png", "test_green.png", "R+G" },
    { "test_blue.png", "test_red.png", "B+R" },
    { "test_cyan.png", "test_magenta.png", "C+M" },
}

for i, comp in ipairs(composites) do
    local x = startX + (i-1) * 80
    local r = display.newRect(x, y + 20, 60, 40)
    r.fill = {
        type = "composite",
        paint1 = { type = "image", filename = comp[1] },
        paint2 = { type = "image", filename = comp[2] },
    }
    label(comp[3], x, y + 48)
end
y = y + 70

-- ============================================================
-- 测试 4：display.save 截图验证
-- ============================================================
label("=== Capture verification ===", W/2, y)
y = y + 18

timer.performWithDelay(2000, function()
    -- 截取整个屏幕
    local capture = display.captureScreen(true)
    if capture then
        -- 缩小显示
        capture.x = W/2
        capture.y = y + 50
        capture.xScale = 0.25
        capture.yScale = 0.25

        -- 保存供人工对比
        display.save(capture, {
            filename = "byteorder_" .. backend .. ".png",
            baseDir = system.DocumentsDirectory,
        })
        label("Capture saved", W/2, y)
        print("BYTEORDER_TEST: Capture saved as byteorder_" .. backend .. ".png")
    end
end)

print("BYTEORDER_TEST: Running backend=" .. backend)
print("BYTEORDER_TEST: Compare file texture row with reference row")
print("BYTEORDER_TEST: If Red<->Blue swapped = byte order bug")
