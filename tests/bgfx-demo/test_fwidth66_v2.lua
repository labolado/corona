-- Test #66 v2: fwidth via SDF (implicit) vs custom effect (explicit)
-- v1 only tested user-defined shader. v2 also tests the built-in SDF path,
-- which internally uses fwidth() in its fragment shader.
-- Both paths require GL_OES_standard_derivatives on GLES — two injection points:
--   1. Rtt_ShaderFactory.cpp  (user-defined kernel / GL path)
--   2. Rtt_BgfxShaderCompiler.cpp  (SDF built-in / bgfx ESSL path)
-- If either crashes = regression.
local W, H = display.contentWidth, display.contentHeight
local cx, cy = display.contentCenterX, display.contentCenterY
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"

display.setDefault("background", 0.1, 0.1, 0.1)
display.newText("#66 fwidth v2 — " .. backend, cx, 18, native.systemFontBold, 13):setFillColor(1,1,1)

-- ── Part A (same as v1): user-defined kernel with fwidth() ────────────────
-- Tests Rtt_ShaderFactory.cpp injection
graphics.defineEffect {
    category = "filter",
    name     = "fwidthTest66",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            float fw = fwidth(texCoord.x);
            return vec4(fw * 100.0, 1.0 - fw * 100.0, 0.5, 1.0);
        }
    ]]
}

local rectA = display.newRect(cx - 80, cy - 40, 120, 60)
rectA.fill.effect = "filter.custom.fwidthTest66"

display.newText("A: custom effect fwidth()", cx - 80, cy - 75, native.systemFont, 10)
    :setFillColor(0.8, 1, 0.8)

-- ── Part B (new in v2): SDF shape — built-in shader uses fwidth() internally ─
-- Tests Rtt_BgfxShaderCompiler.cpp injection (bgfx ESSL path)
-- A circle via SDF will crash on Adreno 730 GLES if the extension is missing.
graphics.setSDF(true)

local circleB = display.newCircle(cx + 80, cy - 40, 35)
circleB:setFillColor(0.4, 0.8, 1.0)
circleB.strokeWidth = 3
circleB:setStrokeColor(1, 1, 0)

display.newText("B: SDF circle (implicit fwidth)", cx + 80, cy - 75, native.systemFont, 10)
    :setFillColor(0.8, 0.8, 1)

-- Rounded rect — SDF also uses fwidth for AA edge
local rrB = display.newRoundedRect(cx, cy + 40, 200, 45, 12)
rrB:setFillColor(1.0, 0.5, 0.2)
rrB.strokeWidth = 2
rrB:setStrokeColor(1, 1, 1, 0.6)

display.newText("B2: SDF roundedRect", cx, cy + 65, native.systemFont, 10)
    :setFillColor(0.9, 0.7, 0.5)

-- ── Result: both must render without crash ────────────────────────────────
timer.performWithDelay(600, function()
    -- v1 only checked one path; v2 checks both injection points
    print("=== FWIDTH66_V2 A(custom-effect) + B(SDF-builtin): PASS ===")
    Runtime:dispatchEvent({ name = "testComplete", id = "fwidth66_v2", result = "PASS" })
end)
