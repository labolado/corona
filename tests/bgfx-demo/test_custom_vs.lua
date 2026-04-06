-- test_custom_vs.lua: 自定义 Vertex Shader + Varying 测试
-- 验证 bgfx 后端对 custom vertex kernel 的支持
-- 3 个 shader: outline (fragment only), colored_outline (vertex+varying), blinked_outline (vertex+varying+uniform)
display.setDefault("background", 0.3, 0.3, 0.35)

-- ============================================================
-- Shader 1: outline (仅 fragment shader + uniform，应该正常工作)
-- ============================================================
local outlineKernel = {
    language = "glsl",
    category = "filter",
    name = "outline",
    uniformData = {
        { name = "outlineWidth", default = 3, min = 0, max = 100, type = "float", index = 0 },
        { name = "color", default = { 1, 0.4, 0, 1 }, min = { 0,0,0,0 }, max = { 1,1,1,1 }, type = "vec4", index = 1 },
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
graphics.defineEffect(outlineKernel)

-- ============================================================
-- Shader 2: colored_outline (自定义 vertex shader + varying)
-- 这是当前 bgfx 不支持的 — 应该显示彩色闪烁轮廓，实际显示白色
-- ============================================================
local coloredOutlineKernel = {
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
                outlineColor.r = 0.5;
                outlineColor.g = 0.4;
                outlineColor.b = 0.4;
                outlineColor.a = 0.9;
            }
            else if ((value - 1.0) < 0.001) {
                outlineColor.r = 0.2;
                outlineColor.g = 1.0;
                outlineColor.b = 0.2;
                outlineColor.a = 0.9;
            }
            else {
                outlineColor.r = 0.157;
                outlineColor.g = 0.835;
                outlineColor.b = 0.835;
                outlineColor.a = 0.9;
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
graphics.defineEffect(coloredOutlineKernel)

-- ============================================================
-- Shader 3: blinked_outline (自定义 vertex + varying + uniform)
-- 也是 bgfx 不支持的 — vertex 读 uniform 计算颜色传给 fragment
-- ============================================================
local blinkedOutlineKernel = {
    language = "glsl",
    category = "filter",
    name = "blinked_outline",
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
            if (value < 0.001) {
                outlineColor = u_UserData2;
            }
            else {
                outlineColor = u_UserData1;
            }
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
            color = mix(vec4(0.0), outlineColor, maxa - mina);
            return CoronaColorScale(color);
        }
    ]],
}
graphics.defineEffect(blinkedOutlineKernel)

-- ============================================================
-- 测试场景：3 行，每行一个 shader
-- ============================================================
local cx = display.contentCenterX
local W = display.contentWidth

-- 标题
local title = display.newText("Custom Vertex Shader Test", cx, 30, native.systemFont, 22)
title:setFillColor(1, 1, 1)

-- 测试图片
local testImg = "test_icon.png"

-- Row 1: outline (fragment only — 应正常)
local label1 = display.newText("1. outline (frag only) — SHOULD WORK", cx, 70, native.systemFont, 16)
label1:setFillColor(0.5, 1, 0.5)

local obj1 = display.newImageRect(testImg, 80, 80)
obj1:translate(cx - 120, 140)
obj1.fill.effect = "filter.custom.outline"
obj1.fill.effect.outlineWidth = 4
obj1.fill.effect.color = { 1, 0.4, 0, 1 }  -- orange

local obj1b = display.newImageRect(testImg, 80, 80)
obj1b:translate(cx, 140)
obj1b.fill.effect = "filter.custom.outline"
obj1b.fill.effect.outlineWidth = 6
obj1b.fill.effect.color = { 0, 0.8, 1, 1 }  -- cyan

local obj1c = display.newRoundedRect(cx + 120, 140, 80, 80, 12)
obj1c:setFillColor(0.8, 0.2, 0.2)
obj1c.fill.effect = "filter.custom.outline"
obj1c.fill.effect.outlineWidth = 4
obj1c.fill.effect.color = { 1, 1, 0, 1 }  -- yellow

-- Row 2: colored_outline (vertex + varying — bgfx 当前不支持)
local label2 = display.newText("2. colored_outline (vtx+varying) — BROKEN in bgfx", cx, 200, native.systemFont, 16)
label2:setFillColor(1, 0.5, 0.5)

local obj2 = display.newImageRect(testImg, 80, 80)
obj2:translate(cx - 120, 270)
obj2.fill.effect = "filter.custom.colored_outline"

local obj2b = display.newImageRect(testImg, 80, 80)
obj2b:translate(cx, 270)
obj2b.fill.effect = "filter.custom.colored_outline"

local obj2c = display.newRoundedRect(cx + 120, 270, 80, 80, 12)
obj2c:setFillColor(0.8, 0.2, 0.2)
obj2c.fill.effect = "filter.custom.colored_outline"

-- Row 3: blinked_outline (vertex + varying + uniform — bgfx 当前不支持)
local label3 = display.newText("3. blinked_outline (vtx+varying+uni) — BROKEN in bgfx", cx, 330, native.systemFont, 16)
label3:setFillColor(1, 0.5, 0.5)

local obj3 = display.newImageRect(testImg, 80, 80)
obj3:translate(cx - 120, 400)
obj3.fill.effect = "filter.custom.blinked_outline"
obj3.fill.effect.outlineWidth = 4
obj3.fill.effect.color1 = { 1, 0.3, 0, 0.9 }
obj3.fill.effect.color2 = { 0, 0.8, 1, 0.9 }

local obj3b = display.newImageRect(testImg, 80, 80)
obj3b:translate(cx, 400)
obj3b.fill.effect = "filter.custom.blinked_outline"
obj3b.fill.effect.outlineWidth = 6
obj3b.fill.effect.color1 = { 0.2, 1, 0.2, 0.9 }
obj3b.fill.effect.color2 = { 1, 0.2, 1, 0.9 }

local obj3c = display.newRoundedRect(cx + 120, 400, 80, 80, 12)
obj3c:setFillColor(0.8, 0.2, 0.2)
obj3c.fill.effect = "filter.custom.blinked_outline"
obj3c.fill.effect.outlineWidth = 4
obj3c.fill.effect.color1 = { 1, 1, 0, 0.9 }
obj3c.fill.effect.color2 = { 0, 1, 1, 0.9 }

-- 状态文字
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local statusText = display.newText("Backend: " .. backend, cx, display.contentHeight - 30, native.systemFont, 18)
statusText:setFillColor(1, 1, 0)

print("=== test_custom_vs: Custom Vertex Shader Test ===")
print("Backend: " .. backend)
print("Row 1: outline (fragment only) — should show colored outlines")
print("Row 2: colored_outline (vertex+varying) — broken in bgfx, shows white")
print("Row 3: blinked_outline (vertex+varying+uniform) — broken in bgfx, shows white")
