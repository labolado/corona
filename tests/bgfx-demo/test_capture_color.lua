-- test_capture_color.lua
-- 精确测试 capture 颜色通道映射
-- 用纯色背景 → captureScreen → display.save → 验证保存的图片颜色
-- 用法: SOLAR2D_TEST=capture_color SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

local W = display.contentWidth
local H = display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local docDir = system.DocumentsDirectory

-- 测试序列：每种纯色背景各截一张
local tests = {
    { r=1, g=0, b=0, name="red" },
    { r=0, g=1, b=0, name="green" },
    { r=0, g=0, b=1, name="blue" },
    { r=1, g=1, b=1, name="white" },
    { r=0.5, g=0.5, b=0.5, name="gray" },
    { r=1, g=1, b=0, name="yellow" },
    { r=0, g=1, b=1, name="cyan" },
    { r=1, g=0, b=1, name="magenta" },
}

local idx = 0

local function runNext()
    idx = idx + 1
    if idx > #tests then
        print("CAPTURE_COLOR_TEST: All " .. #tests .. " tests done for backend=" .. backend)

        -- 显示所有保存的截图（缩小）
        local y = 40
        local t = display.newText("Saved captures (" .. backend .. "):", W/2, y, native.systemFont, 14)
        t:setFillColor(0)
        y = y + 20

        for i, test in ipairs(tests) do
            local fname = "cap_" .. backend .. "_" .. test.name .. ".png"
            local img = display.newImage(fname, docDir)
            if img then
                img.width = 60
                img.height = 40
                img.x = 40 + ((i-1) % 4) * 80
                img.y = y + math.floor((i-1) / 4) * 55
                local label = display.newText(test.name, img.x, img.y + 28, native.systemFont, 10)
                label:setFillColor(0)
            end
        end
        return
    end

    local test = tests[idx]
    display.setDefault("background", test.r, test.g, test.b)

    -- 同时在中间画一个对角色块作为参考
    local rect = display.newRect(W/2, H/2, 100, 100)
    rect:setFillColor(1 - test.r, 1 - test.g, 1 - test.b)  -- 反色

    local label = display.newText(test.name .. " bg", W/2, H/2 - 70, native.systemFont, 20)
    label:setFillColor(1 - test.r, 1 - test.g, 1 - test.b)

    -- 等 2 帧确保渲染完毕，然后截图
    timer.performWithDelay(200, function()
        local capture = display.captureScreen(true)
        if capture then
            local fname = "cap_" .. backend .. "_" .. test.name .. ".png"
            display.save(capture, {
                filename = fname,
                baseDir = docDir,
            })
            capture:removeSelf()
            print("CAPTURE_COLOR_TEST: Saved " .. fname .. " (bg=" .. test.name .. ")")
        else
            print("CAPTURE_COLOR_TEST: FAILED to capture " .. test.name)
        end

        -- 清理并下一个
        rect:removeSelf()
        label:removeSelf()

        timer.performWithDelay(100, runNext)
    end)
end

print("CAPTURE_COLOR_TEST: Starting " .. #tests .. " color tests with backend=" .. backend)
timer.performWithDelay(500, runNext)
