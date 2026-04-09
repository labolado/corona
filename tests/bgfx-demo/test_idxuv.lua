-- test_idxuv.lua: indexed mesh WITH uvs — 复现 terrain mesh 不渲染的问题
display.setDefault("background", 0.2, 0.2, 0.4)

local textBefore = display.newText("BEFORE MESH", display.contentCenterX, 50, native.systemFont, 28)
textBefore:setFillColor(0, 1, 0)

-- indexed mesh WITH uvs（和 terrain 一样的创建方式）
local mesh = display.newMesh{
    x = display.contentCenterX,
    y = display.contentCenterY,
    mode = "indexed",
    vertices = {
        -100, -100,
         100, -100,
         100,  100,
        -100,  100,
    },
    uvs = {
        0, 0,
        1, 0,
        1, 1,
        0, 1,
    },
    indices = { 1, 2, 3, 1, 3, 4 },
}
mesh:setFillColor(1, 0, 0)

-- indexed mesh WITHOUT uvs（对照组）
local mesh2 = display.newMesh{
    x = display.contentCenterX,
    y = display.contentCenterY + 250,
    mode = "indexed",
    vertices = {
        -80, -40,
         80, -40,
         80,  40,
        -80,  40,
    },
    indices = { 1, 2, 3, 1, 3, 4 },
}
mesh2:setFillColor(0, 0, 1)

local textAfter = display.newText("AFTER MESH", display.contentCenterX, display.contentHeight - 50, native.systemFont, 28)
textAfter:setFillColor(1, 1, 0)
