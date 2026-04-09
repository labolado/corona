-- test_outline2.lua: 精确模拟 tank 教程的 outline 用法
-- 重点测试：大尺寸不规则形状 + outline 叠加（outline 稍大放后面，原件盖上面）
display.setDefault("background", 0.9, 0.9, 0.88)

-- 注册 outline shader
graphics.defineEffect{
    language = "glsl", category = "filter", name = "outline",
    uniformData = {
        { name = "outlineWidth", default = 3, min = 0, max = 100, type = "float", index = 0 },
        { name = "color", default = { 1, 0.3, 0, 1 }, min = {0,0,0,0}, max = {1,1,1,1}, type = "vec4", index = 1 },
    },
    fragment = [[
        uniform P_COLOR float u_UserData0;
        uniform P_COLOR vec4 u_UserData1;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
            P_UV float w = u_UserData0 * CoronaTexelSize.x;
            P_UV float h = u_UserData0 * CoronaTexelSize.y;
            P_COLOR float maxa = color.a; P_COLOR float mina = color.a; P_COLOR float a;
            a = texture2D(CoronaSampler0, uv + vec2(0,-h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(0, h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w,0)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2( w,0)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w, h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w,-h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2( w,-h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2( w, h)).a; maxa = max(a,maxa); mina = min(a,mina);
            color = mix(vec4(0.0), u_UserData1, maxa - mina);
            return CoronaColorScale(color);
        }
    ]],
}

-- 注册 colored_outline shader (vertex + varying)
graphics.defineEffect{
    language = "glsl", category = "filter", name = "colored_outline",
    isTimeDependent = true,
    vertex = [[
        varying P_COLOR vec4 outlineColor;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            P_DEFAULT float value = mod(floor(CoronaTotalTime * 1.0), 3.0);
            if (value < 0.001) { outlineColor = vec4(0.9, 0.3, 0.1, 0.9); }
            else if ((value - 1.0) < 0.001) { outlineColor = vec4(0.2, 0.8, 0.2, 0.9); }
            else { outlineColor = vec4(0.1, 0.7, 0.9, 0.9); }
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

-- 注册 blinked_outline shader (vertex + varying + uniform)
graphics.defineEffect{
    language = "glsl", category = "filter", name = "blinked_outline",
    isTimeDependent = true,
    uniformData = {
        { name = "outlineWidth", default = 3, min = 0, max = 100, type = "float", index = 0 },
        { name = "color1", default = { 1, 0.3, 0, 0.9 }, min = {0,0,0,0}, max = {1,1,1,1}, type = "vec4", index = 1 },
        { name = "color2", default = { 0, 0.8, 1, 0.9 }, min = {0,0,0,0}, max = {1,1,1,1}, type = "vec4", index = 2 },
    },
    vertex = [[
        uniform P_COLOR vec4 u_UserData1;
        uniform P_COLOR vec4 u_UserData2;
        varying P_COLOR vec4 outlineColor;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            P_DEFAULT float value = mod(floor(CoronaTotalTime * 1.0), 2.0);
            if (value < 0.001) { outlineColor = u_UserData2; }
            else { outlineColor = u_UserData1; }
            return position;
        }
    ]],
    fragment = [[
        varying P_COLOR vec4 outlineColor;
        uniform P_UV float u_UserData0;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            P_COLOR vec4 color = texture2D(CoronaSampler0, uv);
            P_UV float w = u_UserData0 * CoronaTexelSize.x;
            P_UV float h = u_UserData0 * CoronaTexelSize.y;
            P_COLOR float maxa = color.a; P_COLOR float mina = color.a; P_COLOR float a;
            a = texture2D(CoronaSampler0, uv + vec2(0,-h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(0, h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w,0)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2( w,0)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w, h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2(-w,-h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2( w,-h)).a; maxa = max(a,maxa); mina = min(a,mina);
            a = texture2D(CoronaSampler0, uv + vec2( w, h)).a; maxa = max(a,maxa); mina = min(a,mina);
            color = mix(vec4(0.0), outlineColor, maxa - mina);
            return CoronaColorScale(color);
        }
    ]],
}

local cx = display.contentCenterX
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local title = display.newText("Outline Precision Test (" .. backend .. ")", cx, 20, native.systemFont, 16)
title:setFillColor(0, 0, 0)

local hullFile = "test_hull.png"
local origW, origH = 300, 150
local outlineW = 8  -- tank 项目用的 outlineWidth

-- ============================================================
-- Test 1: 单独 outline (edge detect) 直接应用
-- ============================================================
local label1 = display.newText("1. outline (edge detect only)", cx, 50, native.systemFont, 14)
label1:setFillColor(0, 0, 0)

local obj1 = display.newImageRect(hullFile, origW, origH)
obj1:translate(cx, 130)
obj1.fill.effect = "filter.custom.outline"
obj1.fill.effect.outlineWidth = 4
obj1.fill.effect.color = { 1, 0.3, 0, 1 }

-- ============================================================
-- Test 2: colored_outline (模拟 tank — 稍大放后面 + 原件盖上)
-- ============================================================
local label2 = display.newText("2. colored_outline BEHIND + original ON TOP", cx, 215, native.systemFont, 14)
label2:setFillColor(0.8, 0, 0)

local outline2 = display.newImageRect(hullFile, origW + outlineW, origH + outlineW)
outline2:translate(cx, 290)
outline2.fill.effect = "filter.custom.colored_outline"

local orig2 = display.newImageRect(hullFile, origW, origH)
orig2:translate(cx, 290)

-- ============================================================
-- Test 3: blinked_outline (edge detect + varying color)
-- ============================================================
local label3 = display.newText("3. blinked_outline BEHIND + original ON TOP", cx, 375, native.systemFont, 14)
label3:setFillColor(0, 0, 0.8)

local outline3 = display.newImageRect(hullFile, origW + outlineW, origH + outlineW)
outline3:translate(cx, 450)
outline3.fill.effect = "filter.custom.blinked_outline"
outline3.fill.effect.outlineWidth = 4
outline3.fill.effect.color1 = { 1, 0.3, 0, 0.9 }
outline3.fill.effect.color2 = { 0, 0.8, 1, 0.9 }

local orig3 = display.newImageRect(hullFile, origW, origH)
orig3:translate(cx, 450)

-- ============================================================
-- Test 4: 在 display group 里（模拟 tank 的 ScalableImage）
-- ============================================================
local label4 = display.newText("4. Inside group (simulates ScalableImage)", cx, 535, native.systemFont, 14)
label4:setFillColor(0.5, 0, 0)

local group4 = display.newGroup()
group4:translate(cx, 610)

local outline4 = display.newImageRect(group4, hullFile, origW + outlineW, origH + outlineW)
outline4.fill.effect = "filter.custom.colored_outline"

local orig4 = display.newImageRect(group4, hullFile, origW, origH)

print("=== test_outline2: Outline Precision Test ===")
print("Backend: " .. backend)
