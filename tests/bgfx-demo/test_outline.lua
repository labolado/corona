-- test_outline.lua: Outline shader 边缘检测测试
-- 验证 bgfx 下 outline 是否跟随形状轮廓（而非矩形边界）
-- 问题：GL 下轮廓贴合形状，bgfx 下变成矩形框
display.setDefault("background", 0.85, 0.85, 0.85)

-- ============================================================
-- 注册 outline shader (fragment only, uniform 控制宽度和颜色)
-- ============================================================
graphics.defineEffect{
    language = "glsl",
    category = "filter",
    name = "outline",
    uniformData = {
        { name = "outlineWidth", default = 3, min = 0, max = 100, type = "float", index = 0 },
        { name = "color", default = { 1, 0.3, 0, 1 }, min = {0,0,0,0}, max = {1,1,1,1}, type = "vec4", index = 1 },
    },
    isTimeDependent = true,
    fragment = [[
        uniform P_COLOR float u_UserData0;
        uniform P_COLOR vec4 u_UserData1;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
            P_UV float w = u_UserData0 * CoronaTexelSize.x;
            P_UV float h = u_UserData0 * CoronaTexelSize.y;
            P_COLOR float maxa = color.a;
            P_COLOR float mina = color.a;
            P_COLOR float a;
            a = texture2D(CoronaSampler0, uv + vec2(0, -h)).a; maxa = max(a, maxa); mina = min(a, mina);
            a = texture2D(CoronaSampler0, uv + vec2(0,  h)).a; maxa = max(a, maxa); mina = min(a, mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w, 0)).a; maxa = max(a, maxa); mina = min(a, mina);
            a = texture2D(CoronaSampler0, uv + vec2( w, 0)).a; maxa = max(a, maxa); mina = min(a, mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w, h)).a; maxa = max(a, maxa); mina = min(a, mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w,-h)).a; maxa = max(a, maxa); mina = min(a, mina);
            a = texture2D(CoronaSampler0, uv + vec2( w,-h)).a; maxa = max(a, maxa); mina = min(a, mina);
            a = texture2D(CoronaSampler0, uv + vec2( w, h)).a; maxa = max(a, maxa); mina = min(a, mina);
            color = mix(vec4(0.0), u_UserData1, maxa - mina);
            return CoronaColorScale(color);
        }
    ]],
}

-- ============================================================
-- 注册 colored_outline shader (vertex + varying)
-- ============================================================
graphics.defineEffect{
    language = "glsl",
    category = "filter",
    name = "colored_outline",
    isTimeDependent = true,
    vertex = [[
        varying P_COLOR vec4 outlineColor;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            P_DEFAULT float value = mod(floor(CoronaTotalTime * 1.0), 3.0);
            if (value < 0.001) {
                outlineColor = vec4(0.9, 0.3, 0.1, 0.9);
            }
            else if ((value - 1.0) < 0.001) {
                outlineColor = vec4(0.2, 0.8, 0.2, 0.9);
            }
            else {
                outlineColor = vec4(0.1, 0.7, 0.9, 0.9);
            }
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

-- ============================================================
-- 测试对象：非矩形 alpha 形状
-- ============================================================
local cx = display.contentCenterX
local cy = display.contentCenterY

local title = display.newText("Outline Edge Detection Test", cx, 25, native.systemFont, 18)
title:setFillColor(0, 0, 0)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local statusText = display.newText("Backend: " .. backend, cx, 50, native.systemFont, 16)
statusText:setFillColor(0.8, 0, 0)

-- Row 1: 原始形状（无 shader）— 基准
local label1 = display.newText("Original (no shader)", 80, 80, native.systemFont, 14)
label1:setFillColor(0, 0, 0)

local circle1 = display.newCircle(80, 140, 40)
circle1:setFillColor(0.2, 0.6, 0.2)

local star1 = display.newImageRect("test_star_alpha.png", 80, 80)
star1:translate(200, 140)

local rrect1 = display.newRoundedRect(320, 140, 80, 60, 16)
rrect1:setFillColor(0.2, 0.4, 0.8)

-- Row 2: outline shader (fragment only, alpha edge detection)
-- 这是问题所在：bgfx 下可能检测到矩形边界而非形状边界
local label2 = display.newText("outline (edge detect) — KEY TEST", 160, 200, native.systemFont, 14)
label2:setFillColor(0.8, 0, 0)

local circle2 = display.newCircle(80, 270, 40)
circle2:setFillColor(0.2, 0.6, 0.2)
circle2.fill.effect = "filter.custom.outline"
circle2.fill.effect.outlineWidth = 3
circle2.fill.effect.color = { 1, 0.3, 0, 1 }

local star2 = display.newImageRect("test_star_alpha.png", 80, 80)
star2:translate(200, 270)
star2.fill.effect = "filter.custom.outline"
star2.fill.effect.outlineWidth = 3
star2.fill.effect.color = { 1, 0, 0, 1 }

local rrect2 = display.newRoundedRect(320, 270, 80, 60, 16)
rrect2:setFillColor(0.2, 0.4, 0.8)
rrect2.fill.effect = "filter.custom.outline"
rrect2.fill.effect.outlineWidth = 3
rrect2.fill.effect.color = { 0, 0.8, 0, 1 }

-- Row 3: colored_outline (vertex+varying, full alpha tint)
local label3 = display.newText("colored_outline (alpha fill)", 160, 330, native.systemFont, 14)
label3:setFillColor(0, 0, 0)

local circle3 = display.newCircle(80, 400, 40)
circle3:setFillColor(0.2, 0.6, 0.2)
circle3.fill.effect = "filter.custom.colored_outline"

local star3 = display.newImageRect("test_star_alpha.png", 80, 80)
star3:translate(200, 400)
star3.fill.effect = "filter.custom.colored_outline"

local rrect3 = display.newRoundedRect(320, 400, 80, 60, 16)
rrect3:setFillColor(0.2, 0.4, 0.8)
rrect3.fill.effect = "filter.custom.colored_outline"

-- Row 4: Tank 实际积木图片测试（不规则形状，47% 透明像素）
local label4 = display.newText("Tank body (irregular shape) — CRITICAL", 180, 455, native.systemFont, 14)
label4:setFillColor(0.8, 0, 0)

-- 原始积木
local tankOrig = display.newImageRect("test_tank_body.png", 200, 144)
tankOrig:translate(120, 530)

-- outline shader 应用到积木
local tankOutline = display.newImageRect("test_tank_body.png", 200, 144)
tankOutline:translate(320, 530)
tankOutline.fill.effect = "filter.custom.outline"
tankOutline.fill.effect.outlineWidth = 3
tankOutline.fill.effect.color = { 1, 0.3, 0, 1 }

-- Row 5: colored_outline 应用到积木
local label5 = display.newText("Tank + colored_outline", 160, 610, native.systemFont, 14)
label5:setFillColor(0, 0, 0)

local tankColored = display.newImageRect("test_tank_body.png", 200, 144)
tankColored:translate(120, 680)
tankColored.fill.effect = "filter.custom.colored_outline"

-- 叠加用法（outline 稍大 + 原件盖上）
local tankOverOutline = display.newImageRect("test_tank_body.png", 210, 152)
tankOverOutline:translate(320, 680)
tankOverOutline.fill.effect = "filter.custom.outline"
tankOverOutline.fill.effect.outlineWidth = 4
tankOverOutline.fill.effect.color = { 1, 0.3, 0, 1 }
local tankOver = display.newImageRect("test_tank_body.png", 200, 144)
tankOver:translate(320, 680)

print("=== test_outline: Outline Edge Detection Test ===")
print("Backend: " .. backend)
print("Row 1: Original shapes (baseline)")
print("Row 2: outline shader (edge detect) — should follow shape contour, NOT rectangle")
print("Row 3: colored_outline (alpha fill)")
print("Row 4: outline over original (simulates tank tutorial)")
