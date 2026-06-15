-- #60 repro (distinct colors): snapshot(green inner) + custom fill.effect that
-- outputs MAGENTA, then colorSample at center.
--   bg = blue(0,0,0.15); snapshot+effect should show/sample MAGENTA(1,0,1).
-- GL fires listener with magenta. bgfx originally never fired (#60).
print("=== colorsample60 test ===")
display.setStatusBar(display.HiddenStatusBar)
local cx, cy = display.contentCenterX, display.contentCenterY

local bg = display.newRect(cx, cy, display.contentWidth, display.contentHeight)
bg:setFillColor(0, 0, 0.15)

local kernel = {
    language = "glsl", category = "filter", name = "magenta60",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            return CoronaColorScale(vec4(1.0, 0.0, 1.0, 1.0));
        }
    ]],
}
graphics.defineEffect(kernel)

local snap = display.newSnapshot(200, 200)
snap.x, snap.y = cx, cy
local inner = display.newRect(snap.group, 0, 0, 200, 200)
inner:setFillColor(0, 1, 0) -- green
snap:invalidate()
snap.fill.effect = "filter.custom.magenta60"

local fired = false
-- sample a couple frames in so the snapshot FBO has composited
timer.performWithDelay(300, function()
    display.colorSample(cx, cy, function(e)
        fired = true
        print(string.format("COLORSAMPLE_CALLBACK_FIRED r=%.2f g=%.2f b=%.2f a=%.2f", e.r, e.g, e.b, e.a))
    end)
end)

timer.performWithDelay(1500, function()
    print("COLORSAMPLE_RESULT: " .. (fired and "FIRED" or "NEVER_FIRED (#60)"))
    print("SCREENSHOT_READY")
end)

-- recovery probe: keep the scene dirty after colorSample (toggle by env)
if os.getenv("CS60_ANIMATE") then
    Runtime:addEventListener("enterFrame", function() bg.x = bg.x + (bg.x < display.contentCenterX + 1 and 0.01 or -0.01) end)
end
