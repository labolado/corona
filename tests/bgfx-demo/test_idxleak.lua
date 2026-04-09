-- test_idxleak.lua: indexed mesh state leak 最简复现
display.setDefault("background", 0.2, 0.2, 0.4)

-- 1) mesh 之前的文字（应该可见）
local textBefore = display.newText("BEFORE MESH", display.contentCenterX, 100, native.systemFont, 32)
textBefore:setFillColor(0, 1, 0)

-- 2) indexed mesh（红色方块）
local mesh = display.newMesh{
    x = display.contentCenterX,
    y = display.contentCenterY,
    mode = "indexed",
    vertices = {
        -50, -50,
         50, -50,
         50,  50,
        -50,  50,
    },
    indices = { 1, 2, 3, 1, 3, 4 },
}
mesh:setFillColor(1, 0, 0)

-- 3) mesh 之后的文字（在 bgfx 下消失 — 这是 bug）
local textAfter = display.newText("AFTER MESH", display.contentCenterX, display.contentHeight - 100, native.systemFont, 32)
textAfter:setFillColor(1, 1, 0)

-- 4) 更多对象验证
local rect = display.newRect(display.contentCenterX, display.contentHeight - 200, 100, 50)
rect:setFillColor(0, 0, 1)
