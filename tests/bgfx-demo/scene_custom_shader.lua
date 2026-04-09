--[[
    scene_custom_shader.lua - Scene 11: Custom Shader Effects

    Tests:
    - graphics.defineEffect with custom fragment shader (filter, composite, generator)
    - graphics.defineEffect with custom vertex shader
    - Runtime shader compilation (shaderc on macOS, binary construction on Android)
--]]

local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    local sceneGroup = self.view

    print("[Scene 11: Custom Shader] Creating...")

    local W = display.contentWidth
    local H = display.contentHeight

    -- Background
    local bg = display.newRect(sceneGroup, W/2, H/2, W, H)
    bg:setFillColor(0.2, 0.2, 0.25)

    -- ============================================================
    -- Define effects
    -- ============================================================

    -- Filter: tint
    graphics.defineEffect({
        category = "filter",
        name = "testTint",
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
            {
                P_COLOR vec4 color = texture2D(CoronaSampler0, texCoord);
                color.rgb = color.rgb * CoronaVertexUserData.rgb;
                return CoronaColorScale(color);
            }
        ]],
        vertexData = {
            { name = "r", default = 1, index = 0 },
            { name = "g", default = 1, index = 1 },
            { name = "b", default = 1, index = 2 },
        },
    })

    -- Composite: blend
    graphics.defineEffect({
        category = "composite",
        name = "testBlend",
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
            {
                P_COLOR vec4 a = texture2D(CoronaSampler0, texCoord);
                P_COLOR vec4 b = texture2D(CoronaSampler1, texCoord);
                return CoronaColorScale(a * 0.5 + b * 0.5);
            }
        ]],
    })

    -- Generator: checkerboard
    graphics.defineEffect({
        category = "generator",
        name = "testChecker",
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
            {
                P_UV float cx = floor(texCoord.x * CoronaVertexUserData.x);
                P_UV float cy = floor(texCoord.y * CoronaVertexUserData.y);
                P_COLOR float s = cx + cy;
                P_COLOR float checker = s - 2.0 * floor(s * 0.5);
                P_COLOR vec4 c1 = vec4(0.1, 0.8, 0.9, 1.0);
                P_COLOR vec4 c2 = vec4(0.9, 0.2, 0.4, 1.0);
                return CoronaColorScale(c1 + (c2 - c1) * checker);
            }
        ]],
        vertexData = {
            { name = "cols", default = 8, index = 0 },
            { name = "rows", default = 8, index = 1 },
        },
    })

    -- Filter with custom VS: wave
    graphics.defineEffect({
        category = "filter",
        name = "testWave",
        vertex = [[
            P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
            {
                P_UV float wave = sin(position.y * 0.05 + CoronaTotalTime * 3.0) * CoronaVertexUserData.x;
                return vec2(position.x + wave, position.y);
            }
        ]],
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
            {
                P_COLOR vec4 color = texture2D(CoronaSampler0, texCoord);
                return CoronaColorScale(color);
            }
        ]],
        vertexData = {
            { name = "amplitude", default = 10, index = 0 },
        },
    })

    -- ============================================================
    -- Layout: 2x2 grid
    -- ============================================================
    local cols = 2
    local rowH = (H - 40) / 2
    local colW = W / cols
    local boxSize = math.min(colW - 20, rowH - 40)

    local function label(text, x, y)
        local t = display.newText(sceneGroup, text, x, y, native.systemFont, 10)
        t:setFillColor(1, 1, 1)
    end

    label("Custom Shader Effects", W/2, 12)

    -- 1: Tint
    local r1 = display.newRect(sceneGroup, colW*0.5, 30+rowH*0.5, boxSize, boxSize)
    r1:setFillColor(1, 1, 1)
    r1.fill.effect = "filter.custom.testTint"
    r1.fill.effect.r = 1.0; r1.fill.effect.g = 0.6; r1.fill.effect.b = 0.2
    label("Tint (filter)", colW*0.5, 30+rowH*0.5+boxSize/2+8)

    -- 2: Composite blend
    local comp = display.newRect(sceneGroup, colW*1.5, 30+rowH*0.5, boxSize, boxSize)
    comp.fill = {
        type = "composite",
        paint1 = { type = "image", filename = "test_red.png" },
        paint2 = { type = "image", filename = "test_blue.png" },
    }
    comp.fill.effect = "composite.custom.testBlend"
    label("Blend (composite)", colW*1.5, 30+rowH*0.5+boxSize/2+8)

    -- 3: Generator
    local gen = display.newRect(sceneGroup, colW*0.5, 30+rowH*1.5, boxSize, boxSize)
    gen.fill = { type = "image", filename = "test_cyan.png" }
    gen.fill.effect = "generator.custom.testChecker"
    gen.fill.effect.cols = 8; gen.fill.effect.rows = 8
    label("Checker (generator)", colW*0.5, 30+rowH*1.5+boxSize/2+8)

    -- 4: Wave (custom VS)
    local wave = display.newRect(sceneGroup, colW*1.5, 30+rowH*1.5, boxSize, boxSize)
    wave.fill = { type = "image", filename = "test_cyan.png" }
    wave.fill.effect = "filter.custom.testWave"
    wave.fill.effect.amplitude = 15
    label("Wave (custom VS)", colW*1.5, 30+rowH*1.5+boxSize/2+8)

    print("[Scene 11: Custom Shader] 4 effects defined and applied")
end

function scene:show(event) end
function scene:hide(event) end
function scene:destroy(event) end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)

return scene
