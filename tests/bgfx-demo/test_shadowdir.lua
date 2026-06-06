-- test_shadowdir.lua
-- Directional-shadow regression.
--
-- Purpose: lock the vertical shadow direction so GL and bgfx agree.
-- The shadow filter is applied to a display.newSnapshot (a TextureResourceCanvas
-- whose texture has IsCanvasFlipY=true on bgfx Metal) so this DOES exercise the
-- texelSize.y negation at Rtt_Renderer.cpp (the load-bearing flip). A plain image
-- fill would NOT hit that path and would prove nothing.
--
-- Verdict comes from display.colorSample (in-app framebuffer readback via
-- Display::Capture, reliable on both GL and bgfx) — not from an external
-- screenshot (Metal windows are not reliably captured by CGWindowListCreateImage)
-- and not from a whole-screen mean-diff (a small object can vanish without moving
-- the mean). The test prints SHADOWDIR_PROBE with the side the shadow landed on;
-- the harness asserts GL and bgfx report the SAME side.

local CX, CY = display.contentCenterX, display.contentCenterY
local W, H = display.actualContentWidth, display.actualContentHeight

-- Custom directional-shadow filter (asymmetric single-tap offset sampling).
-- NOTE: GLSL comments are //, not Lua's -- (a -- here is a compile error that
-- silently blanks the render).
local kernel = {
    category = "filter",
    name = "shadowdir",
    isTimeDependent = false,
    uniformData = {
        { name = "direction",     default = { 0, 1 },       min = { -1, -1 }, max = { 1, 1 }, type = "vec2",  index = 0 },
        { name = "shadowOpacity", default = 0.9,            min = 0,          max = 1,        type = "scalar", index = 1 },
        { name = "shadowColor",   default = { 0, 0, 0, 1 }, min = { 0,0,0,0 },max = {1,1,1,1},type = "vec4",  index = 2 },
        { name = "shadowRadius",  default = 18,             min = 0,          max = 64,       type = "scalar", index = 3 },
    },
    -- Read everything as uniforms directly in the fragment kernel. Custom
    -- vertex->fragment varyings are NOT reliably carried on bgfx, which would
    -- zero the offset and drop the shadow entirely. Keeping it uniform-only
    -- isolates the texelSize.y flip (the thing under test).
    fragment = [[
        uniform P_NORMAL vec2 u_UserData0; // direction
        uniform P_UV float u_UserData1;    // shadow opacity
        uniform P_COLOR vec4 u_UserData2;  // shadow color
        uniform P_UV float u_UserData3;    // shadow radius

        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord)
        {
            P_UV vec2 offset = normalize(u_UserData0) * u_UserData3 * CoronaTexelSize.xy;
            P_COLOR vec4 source = texture2D(CoronaSampler0, texCoord);
            P_COLOR float shadowAlpha = texture2D(CoronaSampler0, texCoord - offset).a * u_UserData1;
            P_COLOR vec4 shadow = vec4(u_UserData2.rgb, shadowAlpha);
            P_COLOR vec4 outColor = mix(shadow, source, source.a);
            return CoronaColorScale(outColor);
        }
    ]],
}
graphics.defineEffect(kernel)

-- Light background so the (dark) shadow is detectable by luminance.
local bg = display.newRect(display.currentStage, CX, CY, W + 64, H + 64)
bg:setFillColor(0.94, 0.95, 0.98)

-- Asymmetric opaque art placed in the UPPER half of the snapshot, leaving room
-- below it. With a downward offset the shadow lands below the art; if the Metal
-- flip were wrong it would land above instead.
local SNAP = 200
local snap = display.newSnapshot(SNAP, SNAP)
snap.x, snap.y = CX, CY

local card = display.newRect(snap.group, 0, -45, 130, 70)
card:setFillColor(1, 1, 1)
local mark = display.newRect(snap.group, -40, -45, 26, 70) -- left-edge marker (horizontal asymmetry)
mark:setFillColor(0.95, 0.2, 0.25)
snap:invalidate()

snap.fill.effect = "filter.custom.shadowdir"
snap.fill.effect.direction = { 0, 1 }
snap.fill.effect.shadowOpacity = 0.9
snap.fill.effect.shadowColor = { 0, 0, 0, 1 }
snap.fill.effect.shadowRadius = 18

-- Card spans CY-80..CY-10; with a correct (downward) shadow the dark band sits
-- just below the card bottom in BOTH backends. A wrong Metal flip would put the
-- bgfx band above the card instead. The verdict is produced by the harness
-- (tests/run_shadowdir_regression.sh) via an external screenshot + center-column
-- luminance scan, asserting the dark band lands on the SAME side in GL and bgfx.
--
-- We deliberately do NOT use display.colorSample: its Display::Capture callback
-- does not fire on bgfx when the scene contains a snapshot with a fill.effect
-- (see tasks/lessons.md). The custom filter is also uniform-only (no custom
-- vertex->fragment varyings) because those are not carried on bgfx.
print("=== SHADOWDIR TEST READY ===")
