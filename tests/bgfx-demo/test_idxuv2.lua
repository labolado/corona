-- test_idxuv2.lua: indexed mesh + fillWithTexture（模拟 terrain ground）
display.setDefault("background", 0.8, 0.8, 0.6)

-- indexed mesh
local mesh = display.newMesh{
    x = display.contentCenterX,
    y = display.contentCenterY,
    mode = "indexed",
    vertices = {
        -150, -100,
         150, -100,
         150,  100,
        -150,  100,
    },
    uvs = {
        0, 0,
        1, 0,
        1, 1,
        0, 1,
    },
    indices = { 1, 2, 3, 1, 3, 4 },
}

-- 用 fillWithTexture（和 terrain 一样）
local tex = graphics.newTexture{ type="image", filename="Icon.png" }
if tex then
    mesh.fill = { type="image", filename="Icon.png" }
end

local text = display.newText("IDX MESH + TEXTURE", display.contentCenterX, 50, native.systemFont, 28)
text:setFillColor(0, 0, 0)

local text2 = display.newText("BELOW MESH", display.contentCenterX, display.contentHeight - 50, native.systemFont, 28)
text2:setFillColor(1, 0, 0)
