# SDF Shapes Library — Production Refactor Design

## Goal

Refactor the SDF shapes library into a production-grade, single-file Solar2D library with native-style API, unified stroke system, shadow, gradient, and boolean operations.

## Scope

- 15 shapes: circle, ellipse, rect, roundedRect, hexagon, pentagon, octagon, triangle, diamond, star, ring, crescent, heart, cross, pill
- Stroke support for all shapes
- Shadow (SDF-based soft falloff)
- Gradient fill (composite effect)
- Boolean operations (union, intersect, subtract)
- Bug fixes for all known issues
- High DPI support

## File Structure

Single file: `sdf_shapes.lua` (~900 lines)

```
Section 1: Constants & Utils         (~30 lines)
Section 2: Shader Definitions        (~300 lines)
Section 3: Shader Registration       (~30 lines)
Section 4: Proxy Object System       (~150 lines)
Section 5: Factory Functions         (~250 lines)
Section 6: Boolean Ops               (~60 lines)
Section 7: Gradient Support          (~80 lines)
```

## API Design

### Creation — Solar2D native style

```lua
local sdf = require("sdf_shapes")

local circle = sdf.newCircle(x, y, radius)
local rect = sdf.newRoundedRect(x, y, width, height, cornerRadius)
local ellipse = sdf.newEllipse(x, y, width, height)
local hex = sdf.newHexagon(x, y, radius)
local pentagon = sdf.newPentagon(x, y, radius)
local octagon = sdf.newOctagon(x, y, radius)
local triangle = sdf.newTriangle(x, y, radius)
local diamond = sdf.newDiamond(x, y, width, height)
local star = sdf.newStar(x, y, radius, points, innerRadius)
local ring = sdf.newRing(x, y, outerRadius, innerRadius, startAngle, endAngle)
local crescent = sdf.newCrescent(x, y, radius, offset)
local heart = sdf.newHeart(x, y, radius)
local cross = sdf.newCross(x, y, size, thickness)
local pill = sdf.newPill(x, y, width, height)
```

### Properties — reactive via metatable

```lua
-- Standard display object properties (forwarded to internal group)
circle.x = 200
circle.y = 300
circle.alpha = 0.8
circle.isVisible = false
circle.rotation = 45

-- Color
circle:setFillColor(1, 0, 0)
circle:setStrokeColor(0, 0, 0)

-- Stroke (lazy-creates _stroke object when > 0)
circle.strokeWidth = 3

-- SDF-specific
circle.smoothness = 0.02
```

### Shadow

```lua
circle.shadow = {
    offsetX = 4,
    offsetY = 4,
    blur = 8,
    color = {0, 0, 0, 0.3}
}
-- Setting nil removes shadow
circle.shadow = nil
```

Implementation: a same-shape SDF object placed behind _fill, sized larger by `blur*2`, with increased `smoothness` to simulate blur, offset by `offsetX/offsetY`, colored by `shadow.color`.

**Known limitation:** SDF-based shadow uses the distance field's `smoothstep` falloff, not a true Gaussian blur. For convex shapes (circle, rect, ellipse, hexagon, octagon, diamond) the result is visually identical to a shadow. For non-convex shapes (star, heart, crescent, cross) the shadow may not look correct at concave regions — the falloff follows the SDF contour rather than spreading uniformly. Users needing pixel-perfect shadows on non-convex shapes should use Solar2D's snapshot + blur filter as an alternative.

### Gradient

```lua
circle:setFillGradient({
    type = "linear",  -- or "radial"
    color1 = {1, 0, 0},
    color2 = {0, 0, 1},
    direction = "down"  -- "up", "left", "right", "down" for linear
})
-- Remove gradient, revert to solid fill
circle:setFillGradient(nil)
```

Implementation: see Gradient Implementation section below.

### Boolean Operations

```lua
local shape = sdf.union(a, b)
local shape = sdf.intersect(a, b)
local shape = sdf.subtract(a, b)
```

Implementation:
- `union`: snapshot containing both shapes, wrapped in proxy
- `intersect` / `subtract`: two snapshots (one per operand), fed into a composite effect shader

Boolean result objects support: `x`, `y`, `alpha`, `isVisible`, `rotation`, `removeSelf()`. They do NOT support stroke/shadow/gradient (apply those to operands before combining).

### Cleanup

```lua
circle:removeSelf()  -- removes _group and all internal objects, nils references
```

**Note:** `display.remove(proxy)` will NOT work because the proxy is a plain Lua table, not a Solar2D display object. Solar2D's C-side `display.remove` only operates on actual display objects. Always use `obj:removeSelf()` for SDF proxy objects. To remove the underlying group directly: `display.remove(obj._group)`.

## Internal Architecture

### Proxy Object

Every factory function returns a proxy table, not a raw display object:

```lua
proxy = {
    _fill   = displayObj,        -- always present
    _stroke = displayObj | nil,  -- lazy, created when strokeWidth > 0
    _shadow = displayObj | nil,  -- lazy, created when shadow is set
    _group  = displayGroup,      -- container: shadow -> fill -> stroke (insert order)
    _params = {},                -- shape parameters cache for rebuilding
    _type   = "circle",          -- shape type string
}
setmetatable(proxy, proxyMT)
```

### Metatable Dispatch

| Assignment | Behavior |
|---|---|
| `obj.x/y/alpha/isVisible/rotation/xScale/yScale` | Forward to `_group` |
| `obj.strokeWidth = N` | N>0: lazy-create `_stroke`, set shader uniform. N==0: hide `_stroke` |
| `obj:setFillColor(...)` | `_fill:setFillColor(...)` |
| `obj:setStrokeColor(...)` | `_stroke:setFillColor(...)` (no-op if no stroke) |
| `obj.shadow = {...}` | Lazy-create `_shadow`, configure size/offset/color/smoothness |
| `obj.shadow = nil` | Remove `_shadow` from group |
| `obj:removeSelf()` | `_group:removeSelf()`, nil all references |
| `obj.width/height` (read) | Return from `_params` |

### Uniform Budget Strategy

Solar2D filter effects have a maximum of 4 scalar uniforms (`u_UserData0`–`u_UserData3`). Each shape must fit its parameters within this budget.

**Fill shader uniform layout (per shape):**

| Shape | u_UserData0 | u_UserData1 | u_UserData2 | u_UserData3 |
|---|---|---|---|---|
| circle | radius | smoothness | — | — |
| ellipse | aspect | radius | smoothness | — |
| roundedRect | aspect | cornerRadius | smoothness | — |
| rect | aspect | smoothness | — | — |
| hexagon | radius | smoothness | — | — |
| pentagon | radius | smoothness | — | — |
| octagon | radius | smoothness | — | — |
| triangle | radius | smoothness | — | — |
| diamond | aspect | smoothness | — | — |
| star | radius | smoothness | — | — |
| ring | innerRadius | outerRadius | startAngle | endAngle |
| crescent | radius | offset | smoothness | — |
| heart | radius | smoothness | — | — |
| cross | thickness | smoothness | — | — |
| pill | aspect | cornerRadius | smoothness | — |

**Star: point count and inner radius are baked at shader registration time.** Rather than passing `points` and `innerRadius` as runtime uniforms, we register a separate shader effect per (points, innerRadius) configuration. At `newStar(x, y, r, points, innerR)` call time, Lua checks if `sdf_star_N_M` is already registered; if not, it generates the GLSL with `points` and `innerRatio` as inline constants and calls `graphics.defineEffect()`. This frees up uniform slots for radius + smoothness only.

**Stroke shader:** The stroke object uses the SAME shader as the fill object. The trick: the stroke object is sized larger than the fill by `strokeWidth * 2` on each side. Its SDF radius uniform is adjusted so the SDF boundary falls at the outer edge of the desired stroke band. The fill object (on top) masks the interior. This means no extra `strokeWidth` uniform is needed — the stroke width is encoded in the object's size and SDF radius parameter.

**Ring:** Ring uses all 4 uniforms for shape parameters. Smoothness is hardcoded in the shader as `0.01` (adjustable at registration time if needed). As a result, setting `obj.smoothness` on ring objects has no effect — this is documented as a known limitation. Ring stroke is not supported via the dual-object approach — ring IS inherently a stroke shape (inner + outer radius). Users control "stroke width" by adjusting the gap between innerRadius and outerRadius.

### Stroke — Dual Object, Size-Based

- `_fill`: uses base SDF shader at original size, outputs shape interior
- `_stroke`: uses same SDF shader, but sized `(originalSize + strokeWidth * 2)`, with SDF radius adjusted to fill the larger canvas. The visible stroke band is the area of `_stroke` not covered by `_fill`
- Stroke is inserted into group BEFORE fill, so fill paints on top
- When `strokeWidth` changes: resize `_stroke` display object, recalculate its SDF radius uniform
- When `strokeWidth` set to 0: `_stroke.isVisible = false` (lazy hide, not destroy)

This approach avoids needing a separate stroke shader or extra uniform slots entirely.

### Shadow Implementation

```
_shadow object:
  - Same shape SDF shader as _fill
  - Size: _fill size + shadow.blur * 2 (each dimension)
  - Position: offset by shadow.offsetX, shadow.offsetY relative to group
  - smoothness: shadow.blur / (size * 0.5)  -- larger smoothness = more blur
  - SDF radius: adjusted proportionally to the larger size
  - Color: shadow.color via setFillColor
  - Inserted first in group (behind everything)
```

No extra blur shader needed — SDF's `smoothstep` with large smoothness produces a soft radial falloff. See Known Limitation in Shadow API section regarding non-convex shapes.

### Gradient Implementation

Uses Solar2D's `object.fill` gradient paint combined with a composite effect.

**Object graph:**

```
display.newSnapshot(w, h)          -- the final output object
  ├── gradientRect                  -- a rect with fill = { type="gradient", ... }
  └── (composite effect applied to snapshot)
```

**Approach:**

1. Create a `display.newSnapshot(width, height)`
2. Inside the snapshot, place a `display.newRect` with `fill = { type="gradient", color1={r,g,b}, color2={r,g,b}, direction="down" }` (Solar2D's built-in gradient fill on rects)
3. Register a composite effect (`filter.custom.sdf_gradient_<shape>`) that:
   - `CoronaSampler0` = the snapshot content (gradient rect)
   - Computes the SDF shape in the fragment shader
   - Outputs: `texture2D(CoronaSampler0, uv) * sdfAlpha`
4. Apply the composite effect to the snapshot
5. The snapshot replaces `_fill` in the proxy's group

**Composite effect uniform layout for gradient shapes:** Same as the base fill shader — shape params only. The gradient color comes from the texture, not uniforms.

When `setFillGradient(nil)` is called, the snapshot is removed and the original `_fill` display object is restored.

### Boolean Operations Implementation

**Union:**
```lua
function sdf.union(a, b)
    local w = math.max(a._params.totalWidth, b._params.totalWidth)
    local h = math.max(a._params.totalHeight, b._params.totalHeight)
    local snap = display.newSnapshot(w, h)
    snap.group:insert(a._group)
    snap.group:insert(b._group)
    snap:invalidate()
    return wrapBoolProxy(snap, {a, b})
end
```

**Intersect / Subtract:**
Two snapshots (one per operand), combined via composite effect:

```lua
function sdf.intersect(a, b)
    local snapA = display.newSnapshot(w, h)
    snapA.group:insert(a._group)
    snapA:invalidate()

    local snapB = display.newSnapshot(w, h)
    snapB.group:insert(b._group)
    snapB:invalidate()

    -- Apply composite effect: output = snapA.alpha * snapB.alpha
    -- Apply composite effect with snapA as paint1, snapB as paint2
    local result = display.newSnapshot(w, h)
    result.fill = {
        type = "composite",
        paint1 = { type = "image", filename = snapA },
        paint2 = { type = "image", filename = snapB }
    }
    result.fill.effect = "composite.custom.sdf_bool_intersect"
    result:invalidate()
    return wrapBoolProxy(result, {a, b, snapA, snapB})
end
```

**Composite shader for intersect:**
```glsl
// sdf_bool_intersect (composite)
P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    P_COLOR vec4 c1 = texture2D(CoronaSampler0, uv);
    P_COLOR vec4 c2 = texture2D(CoronaSampler1, uv);
    return vec4(c1.rgb, c1.a * c2.a);
}
```

**Composite shader for subtract:**
```glsl
// sdf_bool_subtract (composite)
P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    P_COLOR vec4 c1 = texture2D(CoronaSampler0, uv);
    P_COLOR vec4 c2 = texture2D(CoronaSampler1, uv);
    return vec4(c1.rgb, c1.a * (1.0 - c2.a));
}
```

**Uniform layout for boolean composite effects:** No shape-specific uniforms needed. The SDF computation happens inside each operand's own shader; the composite only combines alpha channels. All 4 uniform slots are free (unused).

### High DPI

```lua
local contentScale = display.pixelWidth / display.actualContentWidth
local pixelSmooth = 0.5 / (sizeInContentCoords * contentScale)
```

`display.pixelWidth / display.actualContentWidth` gives the ratio of physical pixels to content units. All default `smoothness` values use this formula to get ~1 physical pixel of anti-aliasing regardless of device DPI.

## Bug Fixes

| Bug | Fix |
|---|---|
| Heart: `pow(q.x*0.5, 0.5)` undefined for x<0 | `sqrt(max(q.x*0.5, 0.0))` |
| Heart: 3 lines dead code (lines 318-320) | Delete |
| Star: `smoothstep(-0.2, 0.2, cos(...))` rounded points | Replace with Quilez sdStar SDF (see below) |
| Ring: angle wrapping when endAngle < startAngle | Lua-side normalization (see below) |
| Rect: anisotropic smoothness after aspect scaling | Divide smoothness by aspect correction factor |
| Require paths: mixed `"sdf_shapes"` vs `"lib.sdf_shapes"` | Single file, no cross-requires |
| white_pixel.png path: inconsistent prefixes | Single file, single consistent path relative to require location |
| M.init() called redundantly in every factory | Auto-init once at require time, remove from factories |
| `newOutlinedBox` broken with transparent fill | Remove; use `newRoundedRect` + `strokeWidth` + transparent fill via proxy |

## Star SDF — Quilez Formula (GLSL ES 2.0 compatible)

The star shader is unique: `points` and `innerRadius` are baked as inline GLSL constants at shader registration time (not passed as uniforms). Lua generates the shader source string dynamically:

```lua
local function makeStarShader(points, innerRatio)
    local an = math.pi / points
    local m = 2.0 + (1.0 - innerRatio) * (points - 2.0)  -- convert ratio to Quilez 'm'
    local en = math.pi / m
    return string.format([[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float r = u_UserData0;

            // Baked constants for %d-point star
            const P_UV float an = %.10f;
            const P_UV vec2 acs = vec2(%.10f, %.10f);
            const P_UV float en = %.10f;
            const P_UV vec2 ecs = vec2(%.10f, %.10f);

            P_UV float bn = mod(atan(p.x, p.y), 2.0 * an) - an;
            p = length(p) * vec2(cos(bn), abs(sin(bn)));
            p = p - r * acs;
            p = p + ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
            P_UV float dist = length(p) * sign(p.x);

            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]], points, an,
        math.cos(an), math.sin(an),
        en,
        math.cos(en), math.sin(en))
end
```

At `newStar()` call time:
1. Compute a cache key: `"sdf_star_" .. points .. "_" .. tostring(innerRatio)`
2. If not registered, call `makeStarShader(points, innerRatio)` and `graphics.defineEffect()`
3. Apply the cached effect name to the display object

This avoids GLSL ES 2.0 integer parameter issues and stays within the 2-uniform budget (radius + smoothness).

**Note:** The `m` conversion formula (`2.0 + (1.0 - innerRatio) * (points - 2.0)`) is an approximation mapping innerRatio [0,1] to Quilez's `m` parameter. Must be verified against the Quilez sdStar reference during implementation, especially at extreme values (innerRatio near 0 or 1).

## Ring Angle Wrap-Around Fix

The angle wrapping issue is resolved on the Lua side, not in the shader:

```lua
function M.newRing(x, y, outerRadius, innerRadius, startAngle, endAngle)
    -- Normalize to [0, 2π]
    local startRad = math.rad(startAngle or 0) % (2 * math.pi)
    local endRad = math.rad(endAngle or 360) % (2 * math.pi)

    -- Handle wrap-around: if end < start, add 2π to end
    if endRad <= startRad and (endAngle or 360) ~= (startAngle or 0) then
        endRad = endRad + 2 * math.pi
    end

    -- Pass normalized values to shader
    obj.fill.effect.startAngle = startRad
    obj.fill.effect.endAngle = endRad
end
```

The shader continues to use the simple comparison `angle >= start && angle <= end` (via step/smoothstep), with the Lua side guaranteeing `endAngle >= startAngle` always.

For arcs that wrap past 360° (e.g., 350° to 10°), the Lua normalization produces `startRad=6.109, endRad=6.283+0.175=6.458`, and the shader `angle` (which is `atan + 2π` when negative) correctly falls within this range for fragments in the arc.

## Testing Strategy

- Demo file (`demo.lua`) exercising all 15 shapes with stroke, shadow, gradient
- Visual verification on iOS Simulator + Android emulator
- Edge cases: strokeWidth=0, shadow=nil, gradient=nil, very small radius, very large radius
- High DPI test: run on 1x, 2x, 3x content scales
- Boolean ops test: union/intersect/subtract visual verification
- Performance test: create 100+ shapes, verify 60fps on mid-range device
- Ring arc test: arcs crossing 0°/360° boundary (e.g., 350° to 10°)
- Star test: various point counts (3, 5, 8, 12) and inner radius ratios

## Compatibility

- GLSL ES 2.0 (Android + iOS)
- Solar2D 2024.3727+
- All precision qualifiers use Solar2D macros (P_DEFAULT, P_UV, P_COLOR)
- No `if` branches in performance-critical shaders (use step/mix)
- `pow` replaced with `sqrt(max(...))` for safety
- Star shader uses inline constants, no GLSL int parameters
- All shapes fit within 4-uniform filter effect budget
