-- 临时测试：第三方字体
local W = display.contentWidth
local H = display.contentHeight
local bg = display.newRect(W/2, H/2, W, H)
bg:setFillColor(0.1, 0.1, 0.15)

local y = 60
local fonts = {
    { name = "Geneva.ttf", label = "Geneva (TTF)" },
    { name = "Arial Unicode.ttf", label = "Arial Unicode (TTF)" },
    { name = native.systemFont, label = "System Default" },
    { name = native.systemFontBold, label = "System Bold" },
}

for _, f in ipairs(fonts) do
    local ok, t = pcall(function()
        return display.newText({
            text = f.label .. ": Hello bgfx 你好世界 123",
            x = W/2, y = y,
            font = f.name, fontSize = 18
        })
    end)
    if ok and t then
        t:setFillColor(1, 1, 1)
        print("[PASS] " .. f.label .. " width=" .. math.floor(t.width))
        y = y + 35
    else
        print("[FAIL] " .. f.label .. " error: " .. tostring(t))
        local err = display.newText({
            text = "FAIL: " .. f.label,
            x = W/2, y = y,
            font = native.systemFont, fontSize = 16
        })
        err:setFillColor(1, 0, 0)
        y = y + 35
    end
end

-- 更多样式：大号、小号、彩色
y = y + 20
local t1 = display.newText({
    text = "Geneva 大字 Big Text",
    x = W/2, y = y,
    font = "Geneva.ttf", fontSize = 32
})
t1:setFillColor(0, 1, 1)
print("[PASS] Geneva large size")

y = y + 50
local t2 = display.newText({
    text = "Arial Unicode 小字 Small Text 日本語テスト",
    x = W/2, y = y,
    font = "Arial Unicode.ttf", fontSize = 11
})
t2:setFillColor(1, 1, 0)
print("[PASS] Arial Unicode small size")

print("=== Custom Font Test Done ===")
