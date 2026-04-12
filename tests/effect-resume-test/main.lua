-- Minimal reproduction: custom VS+varying effect on Android bgfx
display.newRect(display.contentCenterX, display.contentCenterY, 320, 480):setFillColor(0.15,0.15,0.2)

graphics.defineEffect({
    language = "glsl", category = "filter", name = "test_blink",
    isTimeDependent = true,
    vertex = [[
        varying P_COLOR vec4 v_tint;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position) {
            P_DEFAULT float t = mod(CoronaTotalTime * 2.0, 2.0);
            v_tint = (t < 1.0) ? vec4(1,0,0,1) : vec4(0,1,0,1);
            return position;
        }
    ]],
    fragment = [[
        varying P_COLOR vec4 v_tint;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            P_COLOR vec4 c = texture2D(CoronaSampler0, uv);
            return CoronaColorScale(c * v_tint);
        }
    ]],
})

local r = display.newRoundedRect(160, 180, 200, 120, 15)
r:setFillColor(0.8, 0.8, 0.8)
r.fill.effect = "filter.custom.test_blink"

local label = display.newText("Should blink red/green", 160, 320, native.systemFont, 14)
local status = display.newText("Resumes: 0", 160, 350, native.systemFont, 12)

local count = 0
Runtime:addEventListener("system", function(e)
    if e.type == "applicationResume" then
        count = count + 1
        status.text = "Resumes: " .. count .. " effect=" .. tostring(r.fill.effect)
        print("[TEST] Resume #" .. count)
    end
end)
print("[TEST] Started. effect=" .. tostring(r.fill.effect))
