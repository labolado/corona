-- test_outline_flip.lua: 测试 outline + 旋转/翻转 是否 Y-flip
-- 复现 tank 教程炮管轮廓 Y 翻转 bug
display.setDefault("background", 0.92, 0.92, 0.88)

graphics.defineEffect{
    language = "glsl", category = "filter", name = "colored_outline",
    vertex = [[
        varying P_COLOR vec4 outlineColor;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            outlineColor = vec4(0.2, 0.8, 0.1, 0.9);
            return position;
        }
    ]],
    fragment = [[
        varying P_COLOR vec4 outlineColor;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            P_COLOR vec4 col = texture2D(CoronaSampler0, fract(uv));
            return CoronaColorScale(outlineColor * col.a);
        }
    ]],
}

local cx = display.contentCenterX
local cy = display.contentCenterY
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
display.newText(backend, cx, 40, native.systemFont, 36):setFillColor(0.8, 0, 0)

-- 用不对称的星星图片（能看出翻转）
local img = "test_star_alpha.png"
local S = 120
local olW = 10

-- Row 1: 正常（无翻转）
display.newText("normal", 100, 100, native.systemFont, 20):setFillColor(0,0,0)
local ol1 = display.newImageRect(img, S+olW, S+olW); ol1:translate(100, 180)
ol1.fill.effect = "filter.custom.colored_outline"
local o1 = display.newImageRect(img, S, S); o1:translate(100, 180)

-- Row 2: rotation = 30
display.newText("rot=30", 280, 100, native.systemFont, 20):setFillColor(0,0,0)
local g2 = display.newGroup(); g2:translate(280, 180); g2.rotation = 30
local ol2 = display.newImageRect(g2, img, S+olW, S+olW)
ol2.fill.effect = "filter.custom.colored_outline"
local o2 = display.newImageRect(g2, img, S, S)

-- Row 3: xScale = -1 (水平翻转，炮管可能用了这个)
display.newText("xScale=-1", 460, 100, native.systemFont, 20):setFillColor(0,0,0)
local g3 = display.newGroup(); g3:translate(460, 180); g3.xScale = -1
local ol3 = display.newImageRect(g3, img, S+olW, S+olW)
ol3.fill.effect = "filter.custom.colored_outline"
local o3 = display.newImageRect(g3, img, S, S)

-- Row 4: yScale = -1 (垂直翻转)
display.newText("yScale=-1", 100, 320, native.systemFont, 20):setFillColor(0,0,0)
local g4 = display.newGroup(); g4:translate(100, 400); g4.yScale = -1
local ol4 = display.newImageRect(g4, img, S+olW, S+olW)
ol4.fill.effect = "filter.custom.colored_outline"
local o4 = display.newImageRect(g4, img, S, S)

-- Row 5: rotation = -20 + xScale = -1 (组合)
display.newText("rot-20+xFlip", 280, 320, native.systemFont, 20):setFillColor(0,0,0)
local g5 = display.newGroup(); g5:translate(280, 400); g5.rotation = -20; g5.xScale = -1
local ol5 = display.newImageRect(g5, img, S+olW, S+olW)
ol5.fill.effect = "filter.custom.colored_outline"
local o5 = display.newImageRect(g5, img, S, S)

-- Row 6: 用 tank 炮管图片（如果存在）
local gunFile = "tank_shape-1.png"  -- 用积木代替，但加旋转模拟炮管
display.newText("rot-15+xFlip", 460, 320, native.systemFont, 20):setFillColor(0,0,0)
local g6 = display.newGroup(); g6:translate(460, 400); g6.rotation = -15; g6.xScale = -1
local ol6 = display.newImageRect(g6, gunFile, 200, 58)
ol6.fill.effect = "filter.custom.colored_outline"
local o6 = display.newImageRect(g6, gunFile, 192, 50)

print("=== test_outline_flip (" .. backend .. ") ===")
