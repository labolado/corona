-- test_outline3.lua: 用 tank 教程真实图片，大尺寸测试 outline
display.setDefault("background", 0.9, 0.9, 0.88)

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
local cy = display.contentCenterY
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"

-- 大尺寸：接近实际教程使用的大小
local shapeW = display.contentWidth * 0.7   -- 占屏幕 70%
local shapeH = shapeW * (510 / 1768)        -- 保持比例
local outlineExtra = 8

-- 只放一个大图：colored_outline 后面 + 原件前面（模拟教程选中）
local outline = display.newImageRect("tank_shape-1.png", shapeW + outlineExtra, shapeH + outlineExtra)
outline:translate(cx, cy)
outline.fill.effect = "filter.custom.colored_outline"

local orig = display.newImageRect("tank_shape-1.png", shapeW, shapeH)
orig:translate(cx, cy)

local label = display.newText(backend .. " — colored_outline behind + original on top", cx, 30, native.systemFont, 18)
label:setFillColor(0, 0, 0)

print("=== test_outline3: size=" .. shapeW .. "x" .. shapeH .. " backend=" .. backend .. " ===")
