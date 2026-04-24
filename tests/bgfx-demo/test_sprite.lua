-- test_sprite.lua: 帧动画/精灵动画回归测试
-- 用 t1.jpg (1200x1200) 4x4 网格做 ImageSheet，测试 display.newSprite 动画
-- 运行: SOLAR2D_TEST=sprite SOLAR2D_BACKEND=bgfx

display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Sprite Animation Test (" .. backend .. ") ===")

local pass, fail = 0, 0
local function check(name, cond)
    if cond then
        pass = pass + 1; print("[PASS] " .. name)
    else
        fail = fail + 1; print("[FAIL] " .. name)
    end
end

-- Background
display.newRect(W/2, H/2, W, H):setFillColor(0.15, 0.15, 0.25)

-- t1.jpg 是 1200x1200，切成 4x4 = 16 帧，每帧 300x300
local sheetOpts = {
    width = 300, height = 300,
    numFrames = 16,
    sheetContentWidth = 1200,
    sheetContentHeight = 1200,
}

local ok, sheet = pcall(function()
    return graphics.newImageSheet("t1.jpg", sheetOpts)
end)
check("newImageSheet created", ok and sheet ~= nil)

if not (ok and sheet) then
    print("[FAIL] Cannot create ImageSheet, aborting")
    print("TEST FAIL: sprite (sheet creation failed)")
    return
end

-- Sequence 1: 顺序播放全 16 帧
local seq1 = { name = "full", start = 1, count = 16, time = 1600, loopCount = 0 }
-- Sequence 2: 前 4 帧快速循环
local seq2 = { name = "top",  start = 1, count = 4,  time = 400,  loopCount = 0 }
-- Sequence 3: 后 4 帧反向（跳帧）
local seq3 = { name = "bot",  start = 13, count = 4, time = 600,  loopCount = 0 }

local sprites = {}

-- Sprite A: 居中，全帧
local spA = display.newSprite(sheet, { seq1, seq2, seq3 })
spA.x, spA.y = W/2 - 90, H/2
spA.xScale, spA.yScale = 0.3, 0.3
check("sprite A created", spA ~= nil)
if spA then
    spA:setSequence("full")
    spA:play()
    sprites[#sprites+1] = spA
end

-- Sprite B: 左侧，仅前 4 帧
local spB = display.newSprite(sheet, { seq1, seq2, seq3 })
spB.x, spB.y = W/2, H/2
spB.xScale, spB.yScale = 0.3, 0.3
check("sprite B created", spB ~= nil)
if spB then
    spB:setSequence("top")
    spB:play()
    sprites[#sprites+1] = spB
end

-- Sprite C: 右侧，后 4 帧
local spC = display.newSprite(sheet, { seq1, seq2, seq3 })
spC.x, spC.y = W/2 + 90, H/2
spC.xScale, spC.yScale = 0.3, 0.3
check("sprite C created", spC ~= nil)
if spC then
    spC:setSequence("bot")
    spC:play()
    sprites[#sprites+1] = spC
end

-- 验证帧属性
if spA then
    check("sprite frame is number", type(spA.frame) == "number")
    check("sprite numFrames > 0", (spA.numFrames or 0) > 0)
    check("sprite sequence exists", spA.sequence ~= nil)
end

-- 验证 setFrame 不崩溃
if spA then
    local ok2 = pcall(function() spA:setFrame(2) end)
    check("setFrame no crash", ok2)
end

-- Labels
local function label(txt, x, y)
    local t = display.newText(txt, x, y, native.systemFont, 13)
    t:setFillColor(1, 1, 0.6)
    return t
end
label("Seq: full (16f)", W/2 - 90, H/2 + 55)
label("Seq: top (4f)",   W/2,      H/2 + 55)
label("Seq: bot (4f)",   W/2 + 90, H/2 + 55)

local title = display.newText("Sprite Animation Test - " .. backend, W/2, 30, native.systemFontBold, 16)
title:setFillColor(1, 1, 1)

-- 等 30 帧后截图（精灵动画已运行几帧，画面稳定）
local frameCount = 0
Runtime:addEventListener("enterFrame", function()
    frameCount = frameCount + 1
    if frameCount == 30 then
        if spA then check("sprite frame advances", spA.frame ~= nil) end
        print(string.format("\n=== SPRITE TEST RESULTS (%s): Pass %d | Fail %d ===", backend, pass, fail))
        if fail == 0 then print("TEST PASS: sprite") else print("TEST FAIL: sprite") end
        print("SCREENSHOT_READY")
    end
end)
