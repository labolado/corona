-- test_shader_compat.lua
-- Test: Shader transformer compatibility with complex GLSL patterns
-- Verifies: array varyings, GL attribute names, raw uniform access

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0.2, 0.2, 0.2)

local results = {}
local function report(name, status, detail)
    results[#results+1] = {name=name, status=status, detail=detail}
    print(string.format("[%s] %s: %s", status, name, detail or ""))
end

local y = 40

-- Test 1: Array varying (fast_blur pattern)
-- varying vec2 blurCoordinates[5] — should fail gracefully, not crash
local function testArrayVarying()
    local kernel = {
        language = "glsl",
        category = "filter",
        name = "test_array_varying",
        vertex = [[
varying P_UV vec2 blurCoords[3];
P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
{
    blurCoords[0] = CoronaTexCoord.xy;
    blurCoords[1] = CoronaTexCoord.xy + vec2(0.01, 0.0);
    blurCoords[2] = CoronaTexCoord.xy - vec2(0.01, 0.0);
    return position;
}
]],
        fragment = [[
varying P_UV vec2 blurCoords[3];
P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
{
    P_COLOR vec4 sum = texture2D(CoronaSampler0, blurCoords[0]) * 0.5;
    sum += texture2D(CoronaSampler0, blurCoords[1]) * 0.25;
    sum += texture2D(CoronaSampler0, blurCoords[2]) * 0.25;
    return CoronaColorScale(sum);
}
]]
    }
    local ok, err = pcall(function()
        graphics.defineEffect(kernel)
    end)
    if ok then
        local rect = display.newRect(W*0.25, y + 40, 100, 60)
        rect:setFillColor(1, 0.5, 0)
        rect.fill.effect = "filter.custom.test_array_varying"
        -- If effect works, it blurs. If not, shows original color.
        report("array_varying", "INFO", "Effect defined (may fall back to default VS)")
    else
        report("array_varying", "FAIL", "defineEffect error: " .. tostring(err))
    end
end

-- Test 2: GL attribute names (water.lua pattern)
-- a_TexCoord, a_UserData — not available in bgfx
local function testGLAttributes()
    local kernel = {
        language = "glsl",
        category = "filter",
        name = "test_gl_attrs",
        vertexData = {
            { name = "amplitude", index = 0, default = 5 },
            { name = "speed", index = 1, default = 2 },
            { name = "frequency", index = 2, default = 3 },
        },
        vertex = [[
P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
{
    P_POSITION float amplitudeScale = cos(4.0 * a_TexCoord.x * a_UserData.z) * a_UserData.x;
    position.y += 3.0 * amplitudeScale * sin(3.0 * u_TotalTime * a_UserData.y + 2.0 * a_TexCoord.x * a_UserData.z);
    return position;
}
]],
        fragment = [[
P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
{
    return CoronaColorScale(texture2D(CoronaSampler0, uv));
}
]]
    }
    local ok, err = pcall(function()
        graphics.defineEffect(kernel)
    end)
    if ok then
        local rect = display.newRect(W*0.5, y + 40, 100, 60)
        rect.fill = {0.3, 0.7, 1.0}
        rect.fill.effect = "filter.custom.test_gl_attrs"
        rect.fill.effect.amplitude = 5
        rect.fill.effect.speed = 2
        rect.fill.effect.frequency = 3
        report("gl_attributes", "INFO", "Effect defined (a_TexCoord/a_UserData may fail in bgfx)")
    else
        report("gl_attributes", "FAIL", "defineEffect error: " .. tostring(err))
    end
end

-- Test 3: Correct macro usage (control — should always work)
local function testCorrectMacros()
    local kernel = {
        language = "glsl",
        category = "filter",
        name = "test_correct_macros",
        vertex = [[
varying P_COLOR vec4 vColor;
P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
{
    P_DEFAULT float t = mod(CoronaTotalTime, 3.0) / 3.0;
    vColor = vec4(t, 1.0 - t, 0.5, 1.0);
    return position;
}
]],
        fragment = [[
varying P_COLOR vec4 vColor;
P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
{
    P_COLOR vec4 tex = texture2D(CoronaSampler0, uv);
    return CoronaColorScale(vColor * tex.a + tex * (1.0 - vColor.a));
}
]]
    }
    graphics.defineEffect(kernel)
    local rect = display.newRect(W*0.75, y + 40, 100, 60)
    rect:setFillColor(0.8, 0.2, 0.8)
    rect.fill.effect = "filter.custom.test_correct_macros"
    report("correct_macros", "PASS", "Simple varying + CoronaTotalTime works")
end

-- Run tests
testArrayVarying()
y = y + 100
testGLAttributes()
y = y + 100
testCorrectMacros()

-- Summary
timer.performWithDelay(2000, function()
    print("\n=== SHADER COMPAT TEST SUMMARY ===")
    for _, r in ipairs(results) do
        print(string.format("  [%s] %s: %s", r.status, r.name, r.detail or ""))
    end
    print("=== END ===")
end)
