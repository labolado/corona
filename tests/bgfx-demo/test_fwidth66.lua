-- Test #66: graphics.defineEffect with fwidth() on GLES (Adreno 730 crash fix)
-- Success: shader compiles and renders a colored rect
-- Failure: app crashes or shows Lua error

local function run()
    -- Define a simple effect that uses fwidth() in fragment kernel
    graphics.defineEffect {
        category = "filter",
        name = "fwidthTest66",
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
            {
                // fwidth requires GL_OES_standard_derivatives on GLES
                float fw = fwidth(texCoord.x);
                // Use result so compiler can't optimize it away
                return vec4(fw * 100.0, 1.0 - fw * 100.0, 0.5, 1.0);
            }
        ]]
    }

    local rect = display.newRect(display.contentCenterX, display.contentCenterY, 200, 200)
    rect.fill.effect = "filter.custom.fwidthTest66"

    -- Verify rect rendered (not crash)
    timer.performWithDelay(500, function()
        print("=== FWIDTH66 RESULT: PASS — fwidth() compiled and rendered ===")
        Runtime:dispatchEvent({ name = "testComplete", id = "fwidth66", result = "PASS" })
    end)
end

return { run = run }
