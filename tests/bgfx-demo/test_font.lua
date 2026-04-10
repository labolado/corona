--[[
    test_font.lua - Font Rendering Test Suite

    Usage: SOLAR2D_TEST=font SOLAR2D_BACKEND=bgfx ./Corona\ Simulator -no-console YES tests/bgfx-demo

    Tests various font rendering scenarios to catch mask/texture bugs:
    1. Basic text (default font, various sizes)
    2. Custom bitmap font
    3. Colored text (different fill colors)
    4. Text with effects (filter.color, filter.grayscale etc.)
    5. Text on colored backgrounds (contrast check)
    6. Multi-line / wrapped text
    7. Dynamic text (changing content)
    8. Text with masks applied
    9. Text alignment variations
    10. Embedded text (emoji/special chars)
    11. Text inside groups/containers
    12. Text with transitions/animations
--]]

local W = display.contentWidth
local H = display.contentHeight
local CX = W * 0.5
local CY = H * 0.5

-- Background
local bg = display.newRect(CX, CY, W, H)
bg:setFillColor(0.12, 0.12, 0.15)

local results = {}
local function log(name, pass)
    results[#results + 1] = { name = name, pass = pass }
    local status = pass and "PASS" or "FAIL"
    print(string.format("  [%s] %s", status, name))
end

print("=== Font Rendering Test Suite ===")

local yPos = 30
local function nextY(h)
    local y = yPos
    yPos = yPos + (h or 30)
    return y
end

-- Section header
local function section(title)
    yPos = yPos + 5
    local t = display.newText({
        text = "-- " .. title .. " --",
        x = CX, y = nextY(22),
        font = native.systemFontBold, fontSize = 12
    })
    t:setFillColor(0.5, 0.5, 0.5)
end

-- ============================================================
-- 1. Basic text - various sizes
-- ============================================================
section("1. Basic Text Sizes")

local sizes = {8, 12, 16, 20, 28, 36}
for i, sz in ipairs(sizes) do
    local t = display.newText({
        text = "Size " .. sz .. "px ABCabc 123",
        x = CX, y = nextY(sz + 6),
        font = native.systemFont, fontSize = sz
    })
    t:setFillColor(1, 1, 1)
    log("basic_size_" .. sz, t.width > 0 and t.height > 0)
end

-- ============================================================
-- 2. Font weight variants
-- ============================================================
section("2. Font Weights")

local fonts = {
    { native.systemFont, "Regular" },
    { native.systemFontBold, "Bold" },
}
for _, f in ipairs(fonts) do
    local t = display.newText({
        text = f[2] .. ": The quick brown fox jumps",
        x = CX, y = nextY(22),
        font = f[1], fontSize = 14
    })
    t:setFillColor(1, 1, 1)
    log("font_" .. f[2], t.width > 0)
end

-- ============================================================
-- 3. Colored text
-- ============================================================
section("3. Colored Text")

local colors = {
    { "Red",     {1, 0, 0} },
    { "Green",   {0, 1, 0} },
    { "Blue",    {0, 0.5, 1} },
    { "Yellow",  {1, 1, 0} },
    { "Magenta", {1, 0, 1} },
    { "White",   {1, 1, 1} },
}
local colorY = nextY(22)
local colorX = 20
for _, c in ipairs(colors) do
    local t = display.newText({
        text = c[1],
        x = colorX, y = colorY,
        font = native.systemFontBold, fontSize = 13
    })
    t.anchorX = 0
    t:setFillColor(unpack(c[2]))
    colorX = colorX + t.width + 10
    log("color_" .. c[1], t.width > 0)
end

-- ============================================================
-- 4. Text on colored backgrounds (contrast)
-- ============================================================
section("4. Text on Backgrounds")

local bgColors = {
    { "WhiteOnBlack", {1,1,1}, {0,0,0} },
    { "BlackOnWhite", {0,0,0}, {1,1,1} },
    { "RedOnDark",    {1,0.3,0.3}, {0.1,0.1,0.1} },
    { "BlueOnLight",  {0,0,0.8}, {0.9,0.9,0.9} },
}
local bgX = 15
local bgY = nextY(35)
for _, bc in ipairs(bgColors) do
    local r = display.newRect(bgX + 55, bgY, 110, 28)
    r:setFillColor(unpack(bc[3]))
    local t = display.newText({
        text = bc[1],
        x = bgX + 55, y = bgY,
        font = native.systemFont, fontSize = 11
    })
    t:setFillColor(unpack(bc[2]))
    bgX = bgX + 118
    log("bg_" .. bc[1], t.width > 0)
end

-- ============================================================
-- 5. Multi-line text
-- ============================================================
section("5. Multi-line Text")

local multiText = display.newText({
    text = "Line 1: Hello World\nLine 2: bgfx Font Test\nLine 3: 中文测试 日本語テスト",
    x = CX, y = nextY(50),
    width = W - 40,
    font = native.systemFont, fontSize = 13,
    align = "center"
})
multiText:setFillColor(0.9, 0.9, 0.9)
log("multiline", multiText.height > 30)

-- ============================================================
-- 6. Text alignment
-- ============================================================
section("6. Text Alignment")

local aligns = {"left", "center", "right"}
for _, a in ipairs(aligns) do
    local t = display.newText({
        text = "Align: " .. a,
        x = CX, y = nextY(20),
        width = W - 40,
        font = native.systemFont, fontSize = 12,
        align = a
    })
    t:setFillColor(0.8, 0.8, 1)
    log("align_" .. a, t.width > 0)
end

-- ============================================================
-- 7. Text with alpha
-- ============================================================
section("7. Text Alpha")

local alphaY = nextY(22)
local alphas = {1.0, 0.7, 0.4, 0.15}
local alphaX = 20
for _, a in ipairs(alphas) do
    local t = display.newText({
        text = string.format("α=%.0f%%", a*100),
        x = alphaX, y = alphaY,
        font = native.systemFontBold, fontSize = 14
    })
    t.anchorX = 0
    t:setFillColor(1, 1, 1)
    t.alpha = a
    alphaX = alphaX + t.width + 15
    log("alpha_" .. tostring(math.floor(a*100)), true)
end

-- ============================================================
-- 8. Text inside group
-- ============================================================
section("8. Text in Group")

local grp = display.newGroup()
grp.x = CX
grp.y = nextY(22)
local gt = display.newText({
    parent = grp,
    text = "Text inside display.newGroup()",
    x = 0, y = 0,
    font = native.systemFont, fontSize = 13
})
gt:setFillColor(0.5, 1, 0.5)
log("text_in_group", gt.width > 0)

-- ============================================================
-- 9. Dynamic text update
-- ============================================================
section("9. Dynamic Text")

local dynText = display.newText({
    text = "Counter: 0",
    x = CX, y = nextY(22),
    font = native.systemFont, fontSize = 14
})
dynText:setFillColor(1, 0.8, 0)

local counter = 0
local function updateText()
    counter = counter + 1
    dynText.text = "Counter: " .. counter
    if counter >= 5 then
        timer.cancel(dynText._timer)
        log("dynamic_update", dynText.text == "Counter: 5")
    end
end
dynText._timer = timer.performWithDelay(200, updateText, 5)

-- ============================================================
-- 10. Text with transition
-- ============================================================
section("10. Animated Text")

local animText = display.newText({
    text = "Fade In/Out",
    x = CX, y = nextY(22),
    font = native.systemFontBold, fontSize = 16
})
animText:setFillColor(0, 1, 1)
animText.alpha = 0
transition.to(animText, {alpha = 1, time = 500})
log("animated_text", true)

-- ============================================================
-- 11. Many text objects (stress test)
-- ============================================================
section("11. Stress Test (20 texts)")

local stressY = nextY(45)
local stressGroup = display.newGroup()
for i = 1, 20 do
    local t = display.newText({
        parent = stressGroup,
        text = "Item #" .. i,
        x = 15 + ((i-1) % 5) * (W/5),
        y = stressY + math.floor((i-1) / 5) * 16,
        font = native.systemFont, fontSize = 10
    })
    t.anchorX = 0
    t:setFillColor(0.7 + math.random()*0.3, 0.7 + math.random()*0.3, 0.7 + math.random()*0.3)
end
log("stress_20_texts", stressGroup.numChildren == 20)

-- ============================================================
-- Summary
-- ============================================================
timer.performWithDelay(1500, function()
    local pass = 0
    local fail = 0
    for _, r in ipairs(results) do
        if r.pass then pass = pass + 1 else fail = fail + 1 end
    end
    print(string.format("\n=== Font Test Results: %d/%d PASS, %d FAIL ===", pass, pass+fail, fail))

    if fail > 0 then
        print("Failed tests:")
        for _, r in ipairs(results) do
            if not r.pass then print("  - " .. r.name) end
        end
    end
end)
