-- #65 isolation: toggle pieces via GRAYISO env to find what triggers whole-screen gray.
-- GRAYISO=bg      → only dark-blue bg rect (no image, no filter)
-- GRAYISO=img     → bg + image, NO custom filter
-- GRAYISO=filter  → bg + image + custom filter (full repro)
local mode = os.getenv("GRAYISO") or "filter"
print("=== grayiso mode=" .. mode .. " ===")

display.setStatusBar(display.HiddenStatusBar)
local W, H = display.contentWidth, display.contentHeight
local bg = display.newRect(display.contentCenterX, display.contentCenterY, W, H)
bg:setFillColor(0, 0, 0.15)

if mode == "twoframe" then
    -- render exactly a few frames then stop dirtying (test double-buffer latency theory)
    local n = 0
    local function onFrame()
        n = n + 1
        bg.x = bg.x + 0.01   -- dirty this frame
        if n >= 3 then Runtime:removeEventListener("enterFrame", onFrame) end
    end
    Runtime:addEventListener("enterFrame", onFrame)
end

if mode == "redraw" then
    -- force a re-render every frame by nudging the bg rect
    Runtime:addEventListener("enterFrame", function()
        bg.x = bg.x + (bg.x < display.contentCenterX + 1 and 0.01 or -0.01)
    end)
end

if mode ~= "bg" and mode ~= "redraw" then
    local r = display.newImageRect("test_icon.png", 200, 200)
    r.x = display.contentCenterX
    r.y = display.contentCenterY

    if mode == "filter" then
        local kernel = {
            language = "glsl", category = "filter", name = "varytest",
            vertex = [[
                varying P_UV vec2 vGrad;
                P_POSITION vec2 VertexKernel(P_POSITION vec2 position) {
                    vGrad = vec2(0.9, 0.3);
                    return position;
                }
            ]],
            fragment = [[
                varying P_UV vec2 vGrad;
                P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                    return CoronaColorScale(vec4(vGrad.x, vGrad.y, 0.0, 1.0));
                }
            ]],
        }
        graphics.defineEffect(kernel)
        r.fill.effect = "filter.custom.varytest"
    end
end

print("SCREENSHOT_READY")
