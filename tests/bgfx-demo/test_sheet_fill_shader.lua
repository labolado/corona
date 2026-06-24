-- test_sheet_fill_shader.lua
-- 最小复现：fillWithPaintTable({type="image", sheet, frame}) + 自定义 filter shader
-- 对比 GL vs bgfx：shader 中 CoronaSampler0 是否正确绑定到 sprite frame 纹理
-- 运行: SOLAR2D_TEST=sheet_fill_shader SOLAR2D_BACKEND=bgfx

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Sheet Fill Shader Test (" .. backend .. ") ===")

display.setDefault("background", 0.2, 0.2, 0.3)

-- ===== 0a. sd_circle_sheet_e2e — vec2 uniforms (Row E) =====
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "sd_circle_sheet_e2e",
    uniformData = {
        { name = "wh",           default = {1, 1},           type = "vec2", index = 0 },
        { name = "tiling",       default = {0, 0, 1, 1},     type = "vec4", index = 1 },
        { name = "outlineColor", default = {0.2, 0.2, 0.2, 1}, type = "vec4", index = 2 },
        { name = "outlineWidth", default = {3, 1/3},         type = "vec2", index = 3 },
    },
    fragment = [[
uniform P_POSITION vec2 u_UserData0;
uniform P_UV vec4 u_UserData1;
uniform P_COLOR vec4 u_UserData2;
uniform P_POSITION vec2 u_UserData3;

P_UV float sdCircle_e(P_UV vec2 p, P_UV float r) { return length(p) - r; }

P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    P_UV vec2 uv2 = (uv - u_UserData1.xy) / u_UserData1.zw;
    P_COLOR vec4 col2 = CoronaColorScale(texture2D(CoronaSampler0, uv));
    P_UV float px = 1.0 / u_UserData0.x;
    P_UV vec2 p = (2.0 * uv2 * u_UserData0 - u_UserData0) * px;
    P_UV float outline = u_UserData3.x * px;
    P_UV float s = 2.0 * px;
    P_UV float offset = outline + 4.0 * px;
    P_UV float fill = sdCircle_e(p, 1.0 - offset);
    P_UV float stroke = abs(fill) - outline;
    P_COLOR vec4 col = vec4(0.0);
    P_COLOR float a1 = 1.0 - smoothstep(-s, s, fill);
    P_COLOR float ratio = u_UserData3.x * u_UserData3.y;
    P_COLOR float a2 = 1.0 - smoothstep(-s * ratio, s * ratio, stroke);
    col = mix(col, col2, a1);
    P_COLOR float m = step(u_UserData2.a, 0.01);
    col2 = mix(col2 * u_UserData2, col2, m);
    col = mix(col, col2, a2);
    return col;
}
]]
})

-- ===== 0b. sd_circle_sheet_vec4 — all vec4 uniforms (Row F, diagnostic) =====
-- Same shader logic but wh and outlineWidth declared as vec4 instead of vec2
-- If Row F looks correct but Row E is gray → vec2 uniform type is the bug
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "sd_circle_sheet_vec4",
    uniformData = {
        { name = "wh",           default = {1, 1, 0, 0},     type = "vec4", index = 0 },
        { name = "tiling",       default = {0, 0, 1, 1},     type = "vec4", index = 1 },
        { name = "outlineColor", default = {0.2, 0.2, 0.2, 1}, type = "vec4", index = 2 },
        { name = "outlineWidth", default = {3, 1/3, 0, 0},   type = "vec4", index = 3 },
    },
    fragment = [[
P_UV float sdCircle_f(P_UV vec2 p, P_UV float r) { return length(p) - r; }

P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    P_UV vec2 uv2 = (uv - u_UserData1.xy) / u_UserData1.zw;
    P_COLOR vec4 col2 = CoronaColorScale(texture2D(CoronaSampler0, uv));
    P_UV float px = 1.0 / u_UserData0.x;
    P_UV vec2 p = (2.0 * uv2 * u_UserData0.xy - u_UserData0.xy) * px;
    P_UV float outline = u_UserData3.x * px;
    P_UV float s = 2.0 * px;
    P_UV float offset = outline + 4.0 * px;
    P_UV float fill = sdCircle_f(p, 1.0 - offset);
    P_UV float stroke = abs(fill) - outline;
    P_COLOR vec4 col = vec4(0.0);
    P_COLOR float a1 = 1.0 - smoothstep(-s, s, fill);
    P_COLOR float ratio = u_UserData3.x * u_UserData3.y;
    P_COLOR float a2 = 1.0 - smoothstep(-s * ratio, s * ratio, stroke);
    col = mix(col, col2, a1);
    P_COLOR float m = step(u_UserData2.a, 0.01);
    col2 = mix(col2 * u_UserData2, col2, m);
    col = mix(col, col2, a2);
    return col;
}
]]
})

-- ===== 0c. uniform_debug — output u_UserData0 as color (Row G) =====
-- Red = u_UserData0.x / 100 (should be ~0.6 if wh.x=60)
-- Green = u_UserData1.z (should be ~0.25 if tiling.z=0.25)
-- Blue = u_UserData3.x / 10 (should be ~0.3 if outlineWidth=3)
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "uniform_debug",
    uniformData = {
        { name = "wh",           default = {1, 1, 0, 0},     type = "vec4", index = 0 },
        { name = "tiling",       default = {0, 0, 1, 1},     type = "vec4", index = 1 },
        { name = "outlineColor", default = {0.2, 0.2, 0.2, 1}, type = "vec4", index = 2 },
        { name = "outlineWidth", default = {3, 1/3, 0, 0},   type = "vec4", index = 3 },
    },
    fragment = [[
P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    // Encode uniform values as color to verify GPU receives correct data
    // R = wh.x / 100  (should be ~0.6 for wh.x=60)
    // G = tiling.z    (should be ~0.25 for 4x4 grid)
    // B = outlineWidth.x / 10  (should be ~0.3 for 3px)
    return vec4(u_UserData0.x / 100.0, u_UserData1.z, u_UserData3.x / 10.0, 1.0);
}
]]
})

-- ===== 1. 定义最简 passthrough filter shader =====
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "passthrough",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            return CoronaColorScale(texture2D(CoronaSampler0, uv));
        }
    ]]
})

-- ===== 2. 定义 UV debug shader（输出 uv.xy 作为 RG 颜色，诊断 UV 范围）=====
graphics.defineEffect({
    language = "glsl",
    category = "filter",
    name = "uv_debug",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            return vec4(uv.x, uv.y, 0.0, 1.0);
        }
    ]]
})

-- ===== 3. 创建 ImageSheet (t1.jpg, 1200x1200, 4x4 grid, 16 frames) =====
local sheetOpts = {
    width = 300, height = 300,
    numFrames = 16,
    sheetContentWidth = 1200,
    sheetContentHeight = 1200,
}
local ok, sheet = pcall(function()
    return graphics.newImageSheet("t1.jpg", sheetOpts)
end)
if not (ok and sheet) then
    display.newText("ERROR: cannot load t1.jpg as ImageSheet", W/2, H/2, native.systemFont, 16):setFillColor(1,0,0)
    print("[FAIL] Cannot create ImageSheet")
    return
end
print("[OK] ImageSheet created from t1.jpg (4x4, 16 frames)")

-- ===== 4. 显示各行说明 =====
local rowH = H / 8
local cols = 4
local cellW = W / cols

local function label(txt, x, y, size)
    local t = display.newText(txt, x, y, native.systemFont, size or 10)
    t:setFillColor(1, 1, 0.7)
    return t
end

label("A: newImageRect — 基准", W/2, rowH * 0.5, 10)
label("B: fill no shader", W/2, rowH * 1.5, 10)
label("C: fill+passthrough", W/2, rowH * 2.5, 10)
label("D: UV debug", W/2, rowH * 3.5, 10)
label("E: sd_circle vec2 uniforms", W/2, rowH * 4.5, 10)
label("F: sd_circle vec4 uniforms", W/2, rowH * 5.5, 10)
label("G: uniform_debug R=wh.x/100 G=tiling.z B=outW/10", W/2, rowH * 6.5, 10)

local testFrames = {1, 4, 9, 16}  -- 4 corner frames of 4x4 grid
-- sheet is 1200x1200, 4x4 grid, each frame 300x300; frame UVs in [0,1] space
local frameUVs = {
    {0,   0,   0.25, 0.25},  -- frame 1: top-left
    {0.75,0,   0.25, 0.25},  -- frame 4: top-right
    {0,   0.5, 0.25, 0.25},  -- frame 9: mid-left
    {0.75,0.75,0.25, 0.25},  -- frame 16: bottom-right
}

for col, frameId in ipairs(testFrames) do
    local cx = cellW * (col - 0.5)
    local sz = math.min(cellW, rowH) * 0.75

    -- Row A: newImageRect (ground truth)
    local a = display.newImageRect(sheet, frameId, sz, sz)
    a.x, a.y = cx, rowH * 0.8
    label("f" .. frameId, cx, rowH * 0.3, 10)

    -- Row B: fill={sheet,frame}, NO shader (standard Solar2D API)
    local b = display.newRect(cx, rowH * 1.8, sz, sz)
    b.fill = {type = "image", sheet = sheet, frame = frameId}

    -- Row C: fill={sheet,frame} + passthrough shader
    local c = display.newRect(cx, rowH * 2.8, sz, sz)
    c.fill = {type = "image", sheet = sheet, frame = frameId}
    c.fill.effect = "filter.custom.passthrough"

    -- Row D: fill={sheet,frame} + UV debug
    local d = display.newRect(cx, rowH * 3.8, sz, sz)
    d.fill = {type = "image", sheet = sheet, frame = frameId}
    d.fill.effect = "filter.custom.uv_debug"

    local uv = frameUVs[col]

    -- Row E: sd_circle_sheet_e2e with vec2 uniform type
    local e = display.newRect(cx, rowH * 4.8, sz, sz)
    e.fill = {type = "image", sheet = sheet, frame = frameId}
    e.fill.effect = "filter.custom.sd_circle_sheet_e2e"
    e.fill.effect.wh = {sz, sz}
    e.fill.effect.tiling = uv
    e.fill.effect.outlineColor = {0.9, 0.9, 0.9, 1}
    e.fill.effect.outlineWidth = {3, 1/3}

    -- Row F: sd_circle_sheet_vec4 — same logic but all vec4 uniforms
    local f = display.newRect(cx, rowH * 5.8, sz, sz)
    f.fill = {type = "image", sheet = sheet, frame = frameId}
    f.fill.effect = "filter.custom.sd_circle_sheet_vec4"
    f.fill.effect.wh = {sz, sz, 0, 0}
    f.fill.effect.tiling = uv
    f.fill.effect.outlineColor = {0.9, 0.9, 0.9, 1}
    f.fill.effect.outlineWidth = {3, 1/3, 0, 0}

    -- Row G: uniform value debug — outputs uniform values as RGB color
    local g = display.newRect(cx, rowH * 6.8, sz, sz)
    g.fill = {type = "image", sheet = sheet, frame = frameId}
    g.fill.effect = "filter.custom.uniform_debug"
    g.fill.effect.wh = {sz, sz, 0, 0}
    g.fill.effect.tiling = uv
    g.fill.effect.outlineColor = {0.9, 0.9, 0.9, 1}
    g.fill.effect.outlineWidth = {3, 1/3, 0, 0}

    print(string.format("[INFO] Col %d frame=%d: E=vec2_circle F=vec4_circle G=uniform_debug wh={%g,%g}", col, frameId, sz, sz))
end

-- ===== 5. 标题 =====
label("Sheet Fill + Shader Test — " .. backend, W/2, 20, 14)

-- ===== 6. 预期说明 =====
-- Row A vs Row B: 应该完全一样 (相同纹理区域)
-- Row B vs Row C: 应该完全一样 (passthrough shader 只传递 CoronaSampler0)
-- Row D: 每个格子应显示 RG 渐变，且渐变范围是 frame 的 UV 子区间，而非 [0,1]
--        如果所有格子都显示 [0,1] 的同样大 RG 渐变 → UV 没有被裁切到 frame 子区间
--        如果 bgfx 下 Row C 全灰 → passthrough 里 CoronaSampler0 没绑到正确纹理
-- Row E: 应该显示圆形，纹理在圆内，带灰色(0.9)描边
--        如果全灰 → wh/tiling/outlineColor 未正确传到 GPU

print("[INFO] Expected:")
print("[INFO]   Row A == Row B == Row C (all show same frame content)")
print("[INFO]   Row D shows sub-range UV gradient per frame (not full [0,1])")
print("[INFO]   Row E shows textured circles with gray outline")
print("[INFO]   DBG-082 log lines show u_UserData0-3 values in macOS console")
print("SCREENSHOT_READY")

-- Auto-capture disabled (captureBounds fails in bgfx mode)
print("[INFO] Test running — take manual screenshot to see Row E")
