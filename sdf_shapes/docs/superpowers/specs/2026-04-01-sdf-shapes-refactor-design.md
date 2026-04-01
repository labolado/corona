# SDF Shapes Library — Production Refactor Design

## Goal

Refactor the SDF shapes library into a production-grade, single-file Solar2D library with native-style API, unified stroke system, shadow, gradient, and boolean operations.

## Scope

- 14 SDF shapes (circle, ellipse, rect, roundedRect, hexagon, pentagon, octagon, triangle, diamond, star, ring, crescent, heart, cross)
- Stroke support for all shapes
- Shadow (SDF-based blur)
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

Implementation: composite effect. A gradient texture (generated via snapshot or paint) serves as paint1, the SDF shape serves as the alpha mask. The composite shader multiplies gradient color by SDF alpha.

### Boolean Operations

```lua
local shape = sdf.union(a, b)
local shape = sdf.intersect(a, b)
local shape = sdf.subtract(a, b)
```

Implementation:
- `union`: snapshot containing both shapes, wrapped in proxy
- `intersect` / `subtract`: composite shader comparing alpha channels of two snapshot-rendered shapes

Boolean result objects support: `x`, `y`, `alpha`, `isVisible`, `rotation`, `removeSelf()`. They do NOT support stroke/shadow/gradient (apply those to operands before combining).

### Cleanup

```lua
circle:removeSelf()    -- removes _group and all internal objects
display.remove(circle) -- also works (metatable intercept)
```

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

### Stroke — Dual Object Shader

- `_fill`: uses base SDF shader, outputs shape interior with anti-aliased edge
- `_stroke`: uses same SDF shader with uniforms adjusted to render only the edge band (shell)
- Stroke is inserted into group BEFORE fill, so fill paints on top
- When `strokeWidth` changes, only the `_stroke` uniform is updated (no object recreation)

For shapes where the SDF is a true distance field (circle, roundedRect, hexagon, octagon, diamond, rect, cross, crescent, ring):
```glsl
// Stroke shader: show only the band where |dist| < strokeWidth
float strokeAlpha = (1.0 - smoothstep(strokeW - smooth, strokeW + smooth, abs(dist)))
                  * step(0.001, strokeW);
```

For shapes with approximate SDFs (star, heart, pentagon, triangle):
- Stroke uses the outer-only approach: `step(radius, dist) * (1.0 - smoothstep(outerEdge - smooth, outerEdge + smooth, dist))`
- The stroke object is slightly larger than the fill to accommodate the stroke band

### Shadow Implementation

```
_shadow object:
  - Same shape SDF shader as _fill
  - Size: _fill size + shadow.blur * 2
  - Position: offset by shadow.offsetX, shadow.offsetY relative to group
  - smoothness: shadow.blur / (size * 0.5)  -- larger smoothness = more blur
  - Color: shadow.color via setFillColor
  - Inserted first in group (behind everything)
```

No extra blur shader needed — SDF's `smoothstep` with large smoothness naturally produces a soft falloff that looks like a shadow.

### Gradient Implementation

Uses Solar2D composite effect:

```lua
-- Gradient shader (composite)
-- CoronaSampler0 = gradient texture (from snapshot or GradientPaint)
-- CoronaSampler1 = SDF shape (from snapshot)
-- Output: gradient color * SDF alpha
```

Lua side:
1. Create a snapshot with a gradient rect (using Solar2D's built-in `graphics.newGradient` or a gradient paint)
2. Apply composite effect that samples gradient for color and SDF for alpha
3. Cache the gradient snapshot for reuse

### High DPI

```lua
local contentScale = display.contentScaleX or 1
local pixelSmooth = 0.5 / (sizeInContentCoords * contentScale)
```

All default `smoothness` values use this formula to get ~1 physical pixel of anti-aliasing regardless of device DPI.

## Bug Fixes

| Bug | Fix |
|---|---|
| Heart: `pow(q.x*0.5, 0.5)` undefined for x<0 | `sqrt(max(q.x*0.5, 0.0))` |
| Heart: 3 lines dead code (lines 318-320) | Delete |
| Star: `smoothstep(-0.2, 0.2, cos(...))` rounded points | Replace with Quilez sdStar SDF |
| Ring: angle wrapping when endAngle < startAngle | Normalize angles in shader, handle wrap-around |
| Rect: anisotropic smoothness after aspect scaling | Divide smoothness by aspect correction factor |
| Require paths: mixed `"sdf_shapes"` vs `"lib.sdf_shapes"` | Single file, no cross-requires |
| white_pixel.png path: inconsistent prefixes | Single file, single consistent path |
| M.init() called redundantly in every factory | Auto-init once at require time, remove from factories |
| `newOutlinedBox` broken with transparent fill | Remove; use `newRoundedRect` + `strokeWidth` + no fill |

## Star SDF — Quilez Formula

Replace the current approximate star with Inigo Quilez's exact sdStar:

```glsl
float sdStar(vec2 p, float r, int n, float m) {
    float an = 3.141593 / float(n);
    float en = 3.141593 / m;
    vec2 acs = vec2(cos(an), sin(an));
    vec2 ecs = vec2(cos(en), sin(en));
    float bn = mod(atan(p.x, p.y), 2.0 * an) - an;
    p = length(p) * vec2(cos(bn), abs(sin(bn)));
    p -= r * acs;
    p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
    return length(p) * sign(p.x);
}
```

Note: GLSL ES 2.0 has no `int` params in functions. Pre-compute `an/en/acs/ecs` as uniforms or inline constants. The `n` (point count) and `m` (inner/outer ratio) will be passed via `u_UserData`.

## Testing Strategy

- Demo file (`demo.lua`) exercising all 14 shapes with stroke, shadow, gradient
- Visual verification on iOS Simulator + Android emulator
- Edge cases: strokeWidth=0, shadow=nil, gradient=nil, very small radius, very large radius
- High DPI test: run on 1x, 2x, 3x content scales
- Boolean ops test: union/intersect/subtract visual verification
- Performance test: create 100+ shapes, verify 60fps on mid-range device

## Compatibility

- GLSL ES 2.0 (Android + iOS)
- Solar2D 2024.3727+
- All precision qualifiers use Solar2D macros (P_DEFAULT, P_UV, P_COLOR)
- No `if` branches in performance-critical shaders (use step/mix)
- `pow` replaced with `sqrt(max(...))` for safety
