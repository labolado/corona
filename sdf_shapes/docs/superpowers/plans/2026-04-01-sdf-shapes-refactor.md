# SDF Shapes Production Refactor — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite the SDF shapes library as a single production-grade `sdf_shapes.lua` with native Solar2D API, proxy object system, stroke, shadow, gradient, and boolean operations.

**Architecture:** Single-file library. Every factory returns a metatable proxy wrapping a display group containing fill/stroke/shadow objects. Shaders are GLSL ES 2.0 filter effects registered once at require time. Advanced features (gradient, boolean) use composite effects with snapshots.

**Tech Stack:** Lua 5.1 (Solar2D), GLSL ES 2.0, Solar2D display/snapshot/graphics APIs

**Spec:** `docs/superpowers/specs/2026-04-01-sdf-shapes-refactor-design.md`

---

## Chunk 1: Core Library — Shaders, Proxy, Basic Factories

### Task 1: Foundation — Constants, Utils, Shader Registration

**Files:**
- Create: `sdf_shapes.lua` (start of file, ~60 lines)

- [ ] **Step 1: Create file scaffold with constants and utils**

```lua
------------------------------------------------------------------------------
-- SDF Shapes Library for Solar2D
-- Production-ready anti-aliased vector shapes with stroke, shadow, gradient
-- Version: 2.0.0
------------------------------------------------------------------------------

local M = {}
local _initialized = false
local _registeredShaders = {}  -- cache for dynamically registered shaders (star)

-- Constants
local PI = math.pi
local TWO_PI = PI * 2
local RAD = math.rad
local SQRT = math.sqrt
local MAX = math.max
local MIN = math.min
local FORMAT = string.format

-- High DPI: ratio of physical pixels to content units
local function getContentScale()
    if display.pixelWidth and display.actualContentWidth then
        return display.pixelWidth / display.actualContentWidth
    end
    return 1
end

-- Default smoothness for ~1 physical pixel anti-aliasing
local function defaultSmoothness(sizeInContent)
    return 0.5 / (sizeInContent * getContentScale())
end

-- Create white pixel carrier object
local function createObject(width, height)
    local obj = display.newImageRect("white_pixel.png", width, height)
    if not obj then
        obj = display.newRect(0, 0, width, height)
        obj:setFillColor(1, 1, 1)
    end
    return obj
end
```

- [ ] **Step 2: Add shader definitions table — circle, ellipse, rect, roundedRect**

```lua
local shaders = {
    circle = {
        category = "filter",
        name = "sdf_circle",
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.5 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                P_UV float dist = length(p);
                P_UV float alpha = 1.0 - smoothstep(u_UserData0 - u_UserData1, u_UserData0 + u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    ellipse = {
        category = "filter",
        name = "sdf_ellipse",
        uniformData = {
            { name = "aspect",     type = "scalar", index = 0, default = 1.0 },
            { name = "radius",     type = "scalar", index = 1, default = 0.5 },
            { name = "smoothness", type = "scalar", index = 2, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            uniform P_DEFAULT float u_UserData2;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                p.x = p.x * u_UserData0;
                P_UV float dist = length(p);
                P_UV float alpha = 1.0 - smoothstep(u_UserData1 - u_UserData2, u_UserData1 + u_UserData2, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    rect = {
        category = "filter",
        name = "sdf_rect",
        uniformData = {
            { name = "aspect",     type = "scalar", index = 0, default = 1.0 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                P_UV float aspect = u_UserData0;
                p.x = p.x * aspect;
                P_UV vec2 d = abs(p) - vec2(aspect, 1.0);
                P_UV float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
                P_UV float smooth = u_UserData1 / max(aspect, 1.0);
                P_UV float alpha = 1.0 - smoothstep(-smooth, smooth, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    roundedRect = {
        category = "filter",
        name = "sdf_rounded_rect",
        uniformData = {
            { name = "aspect",       type = "scalar", index = 0, default = 1.0 },
            { name = "cornerRadius", type = "scalar", index = 1, default = 0.1 },
            { name = "smoothness",   type = "scalar", index = 2, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            uniform P_DEFAULT float u_UserData2;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                P_UV float aspect = u_UserData0;
                P_UV float radius = u_UserData1;
                p.x = p.x * aspect;
                P_UV vec2 b = vec2(aspect, 1.0) - vec2(radius * 2.0);
                P_UV vec2 q = abs(p) - b;
                P_UV float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
                P_UV float alpha = 1.0 - smoothstep(-u_UserData2, u_UserData2, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },
}
```

- [ ] **Step 3: Add remaining shape shaders — hexagon, pentagon, octagon, triangle, diamond, cross**

```lua
-- Add to shaders table:

    hexagon = {
        category = "filter",
        name = "sdf_hexagon",
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.5 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = abs((uv - 0.5) * 2.0);
                P_UV float dist = max(dot(vec2(1.73205080757, 1.0), p) / 2.0, p.x) - u_UserData0;
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    pentagon = {
        category = "filter",
        name = "sdf_pentagon",
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.5 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                p.y = -p.y - 0.15;
                P_UV float a = mod(atan(p.x, p.y) + 3.14159265359, 6.28318530718) / 5.0;
                P_UV float r = length(p);
                P_UV vec2 q = r * vec2(cos(a), sin(a));
                P_UV float dist = max(q.x - 0.809016994 * u_UserData0, abs(q.y) - 0.587785252 * u_UserData0);
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    octagon = {
        category = "filter",
        name = "sdf_octagon",
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.5 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = abs((uv - 0.5) * 2.0);
                P_UV float dist = dot(p, vec2(1.0, 0.414213562)) - u_UserData0 * 1.414213562;
                dist = max(dist, p.x - u_UserData0);
                dist = max(dist, p.y - u_UserData0);
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    triangle = {
        category = "filter",
        name = "sdf_triangle",
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.5 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                p.y = p.y + 0.35;
                P_UV float dist = max(abs(p.x) * 1.73205080757 + p.y, -p.y) - u_UserData0;
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    diamond = {
        category = "filter",
        name = "sdf_diamond",
        uniformData = {
            { name = "aspect",     type = "scalar", index = 0, default = 1.0 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                p.x = p.x * u_UserData0;
                P_UV float dist = abs(p.x) + abs(p.y) - 1.0;
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    cross = {
        category = "filter",
        name = "sdf_cross",
        uniformData = {
            { name = "thickness",  type = "scalar", index = 0, default = 0.3 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                P_UV float t = u_UserData0;
                P_UV float h1 = length(vec2(max(abs(p.x) - 1.0, 0.0), max(abs(p.y) - t, 0.0)));
                P_UV float h2 = length(vec2(max(abs(p.x) - t, 0.0), max(abs(p.y) - 1.0, 0.0)));
                P_UV float dist = min(h1, h2);
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },
```

- [ ] **Step 4: Add heart, crescent, ring shaders (with bug fixes)**

```lua
-- Add to shaders table:

    heart = {
        category = "filter",
        name = "sdf_heart",
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.4 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                p.y = -p.y + 0.25;
                P_UV vec2 q = abs(p);
                q.y = -p.y;
                P_UV float dist = sqrt(max(q.x * 0.5, 0.0)) + q.y - u_UserData0;
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    crescent = {
        category = "filter",
        name = "sdf_crescent",
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.5 },
            { name = "offset",     type = "scalar", index = 1, default = 0.3 },
            { name = "smoothness", type = "scalar", index = 2, default = 0.01 },
        },
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            uniform P_DEFAULT float u_UserData2;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                P_UV float d1 = length(p) - u_UserData0;
                P_UV float d2 = length(p - vec2(u_UserData1, 0.0)) - u_UserData0;
                P_UV float dist = max(d1, -d2);
                P_UV float alpha = 1.0 - smoothstep(-u_UserData2, u_UserData2, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },

    ring = {
        category = "filter",
        name = "sdf_ring",
        uniformData = {
            { name = "innerRadius", type = "scalar", index = 0, default = 0.3 },
            { name = "outerRadius", type = "scalar", index = 1, default = 0.5 },
            { name = "startAngle",  type = "scalar", index = 2, default = 0.0 },
            { name = "endAngle",    type = "scalar", index = 3, default = 6.28318530718 },
        },
        -- smoothness hardcoded to 0.01 (all 4 uniforms used by shape params)
        fragment = [[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            uniform P_DEFAULT float u_UserData2;
            uniform P_DEFAULT float u_UserData3;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                P_UV float r = length(p);
                P_UV float angle = atan(p.y, p.x);
                angle = angle + step(angle, 0.0) * 6.28318530718;
                P_UV float radialDist = max(u_UserData0 - r, r - u_UserData1);
                P_UV float start = u_UserData2;
                P_UV float endA = u_UserData3;
                P_UV float midAngle = (start + endA) * 0.5;
                P_UV float range = (endA - start) * 0.5;
                P_UV float angleDiff = abs(angle - midAngle);
                angleDiff = min(angleDiff, 6.28318530718 - angleDiff);
                P_UV float isFullCircle = step(3.14159265359 - 0.001, range);
                P_UV float angleDist = max(0.0, angleDiff - range) * (1.0 - isFullCircle);
                P_UV float dist = max(radialDist, angleDist * r);
                P_UV float alpha = 1.0 - smoothstep(-0.01, 0.01, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]],
    },
```

- [ ] **Step 5: Add star shader factory function (dynamic registration)**

```lua
-- Star shader: dynamically generated per (points, innerRatio) combination
local function makeStarShader(points, innerRatio)
    local an = PI / points
    -- Convert innerRatio to Quilez 'm' parameter
    -- m=2 gives maximum inner radius, higher m gives sharper points
    local m = 2.0 + (1.0 - innerRatio) * (points - 2.0)
    local en = PI / m
    local cosAn, sinAn = math.cos(an), math.sin(an)
    local cosEn, sinEn = math.cos(en), math.sin(en)

    -- Note: spec template includes a standalone `en` constant; we omit it here
    -- and bake cos(en)/sin(en) directly into ecs vec2. Numerically identical.
    return {
        category = "filter",
        name = FORMAT("sdf_star_%d_%.4f", points, innerRatio),
        uniformData = {
            { name = "radius",     type = "scalar", index = 0, default = 0.5 },
            { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
        },
        fragment = FORMAT([[
            uniform P_DEFAULT float u_UserData0;
            uniform P_DEFAULT float u_UserData1;
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_UV vec2 p = (uv - 0.5) * 2.0;
                P_UV float r = u_UserData0;
                const P_UV float an = %.10f;
                const P_UV vec2 acs = vec2(%.10f, %.10f);
                const P_UV vec2 ecs = vec2(%.10f, %.10f);
                P_UV float bn = mod(atan(p.x, p.y), 2.0 * an) - an;
                P_UV float len = length(p);
                p = len * vec2(cos(bn), abs(sin(bn)));
                p = p - r * acs;
                p = p + ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
                P_UV float dist = length(p) * sign(p.x);
                P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
                return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
            }
        ]], an, cosAn, sinAn, cosEn, sinEn),
    }
end

local function getStarEffectName(points, innerRatio)
    local key = FORMAT("sdf_star_%d_%.4f", points, innerRatio)
    if not _registeredShaders[key] then
        local shader = makeStarShader(points, innerRatio)
        local ok, err = pcall(graphics.defineEffect, shader)
        if ok then
            _registeredShaders[key] = true
        else
            print("SDF: Failed to register star shader: " .. tostring(err))
        end
    end
    return "filter.custom." .. key
end
```

- [ ] **Step 6: Add shader registration init function**

```lua
function M.init()
    if _initialized then return end
    for key, shader in pairs(shaders) do
        local ok, err = pcall(graphics.defineEffect, shader)
        if not ok then
            print("SDF: Failed to register " .. key .. ": " .. tostring(err))
        end
    end
    _initialized = true
end
```

- [ ] **Step 7: Commit foundation**

```bash
git add sdf_shapes.lua
git commit -m "feat(sdf): foundation — constants, utils, all shader definitions"
```

---

### Task 2: Proxy Object System

**Files:**
- Modify: `sdf_shapes.lua` (add after shader registration, ~120 lines)

- [ ] **Step 1: Write proxy metatable with property forwarding**

```lua
-- Properties forwarded directly to _group
local GROUP_PROPS = {
    x = true, y = true, alpha = true, isVisible = true,
    rotation = true, xScale = true, yScale = true,
    anchorX = true, anchorY = true,
}

-- Read-only properties from _params
local PARAM_PROPS = {
    width = true, height = true,
}

local proxyMT = {}
proxyMT.__index = function(self, key)
    -- Methods
    if key == "removeSelf" then
        return function(obj)
            if obj._group and obj._group.removeSelf then
                obj._group:removeSelf()
            end
            obj._fill = nil
            obj._stroke = nil
            obj._shadow = nil
            obj._group = nil
            obj._params = nil
        end
    end
    if key == "setFillColor" then
        return function(obj, ...)
            if obj._fill then obj._fill:setFillColor(...) end
        end
    end
    if key == "setStrokeColor" then
        return function(obj, ...)
            if obj._stroke then obj._stroke:setFillColor(...) end
            obj._strokeColor = {...}
        end
    end
    -- Group property reads
    if GROUP_PROPS[key] and self._group then
        return self._group[key]
    end
    -- Param reads
    if PARAM_PROPS[key] and self._params then
        return self._params[key]
    end
    return rawget(self, key)
end

proxyMT.__newindex = function(self, key, value)
    -- Group property writes
    if GROUP_PROPS[key] and self._group then
        self._group[key] = value
        return
    end
    -- strokeWidth: lazy create/update stroke
    if key == "strokeWidth" then
        rawset(self, "_strokeWidth", value)
        if self._updateStroke then
            self:_updateStroke(value)
        end
        return
    end
    -- smoothness: update fill shader
    if key == "smoothness" then
        rawset(self, "_smoothness", value)
        if self._fill and self._fill.fill and self._fill.fill.effect then
            self._fill.fill.effect.smoothness = value
        end
        return
    end
    -- shadow: lazy create/update
    if key == "shadow" then
        if self._updateShadow then
            self:_updateShadow(value)
        end
        return
    end
    rawset(self, key, value)
end
```

- [ ] **Step 2: Write proxy constructor helper**

```lua
local function newProxy(group, fill, params, shapeType)
    local proxy = {
        _group  = group,
        _fill   = fill,
        _stroke = nil,
        _shadow = nil,
        _params = params,
        _type   = shapeType,
        _strokeWidth = 0,
        _strokeColor = {1, 1, 1},
        _smoothness = params.smoothness,
    }
    setmetatable(proxy, proxyMT)
    return proxy
end
```

- [ ] **Step 3: Commit proxy system**

```bash
git add sdf_shapes.lua
git commit -m "feat(sdf): proxy object system with metatable property dispatch"
```

---

### Task 3: Factory Functions — All 15 Shapes

**Files:**
- Modify: `sdf_shapes.lua` (add after proxy system, ~250 lines)

Note: `newRect` is not explicitly listed in the spec's API section but IS listed in the spec's uniform budget table as a distinct shape. We include it here as the 15th factory alongside pill.

- [ ] **Step 1: Write factory functions for circle, ellipse, rect, roundedRect**

```lua
function M.newCircle(x, y, radius)
    M.init()
    local size = radius * 2
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(size, size)
    fill.fill.effect = "filter.custom.sdf_circle"
    fill.fill.effect.radius = 0.95
    fill.fill.effect.smoothness = defaultSmoothness(radius)
    group:insert(fill)

    return newProxy(group, fill, {
        width = size, height = size,
        radius = radius,
        sdfRadius = 0.95,
        smoothness = defaultSmoothness(radius),
        effectName = "filter.custom.sdf_circle",
    }, "circle")
end

function M.newEllipse(x, y, width, height)
    M.init()
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(width, height)
    fill.fill.effect = "filter.custom.sdf_ellipse"
    fill.fill.effect.aspect = width / height
    fill.fill.effect.radius = 0.95
    fill.fill.effect.smoothness = defaultSmoothness(MIN(width, height) * 0.5)
    group:insert(fill)

    return newProxy(group, fill, {
        width = width, height = height,
        aspect = width / height,
        sdfRadius = 0.95,
        smoothness = defaultSmoothness(MIN(width, height) * 0.5),
        effectName = "filter.custom.sdf_ellipse",
    }, "ellipse")
end

function M.newRect(x, y, width, height)
    M.init()
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(width, height)
    fill.fill.effect = "filter.custom.sdf_rect"
    fill.fill.effect.aspect = width / height
    fill.fill.effect.smoothness = defaultSmoothness(MIN(width, height) * 0.5)
    group:insert(fill)

    return newProxy(group, fill, {
        width = width, height = height,
        aspect = width / height,
        smoothness = defaultSmoothness(MIN(width, height) * 0.5),
        effectName = "filter.custom.sdf_rect",
    }, "rect")
end

function M.newRoundedRect(x, y, width, height, cornerRadius)
    M.init()
    cornerRadius = cornerRadius or 10
    local group = display.newGroup()
    group.x, group.y = x, y

    local aspect = width / height
    local minSize = MIN(width, height)
    local normalizedR = MIN(cornerRadius / (minSize * 0.5), 0.45)

    local fill = createObject(width, height)
    fill.fill.effect = "filter.custom.sdf_rounded_rect"
    fill.fill.effect.aspect = aspect
    fill.fill.effect.cornerRadius = normalizedR
    fill.fill.effect.smoothness = defaultSmoothness(minSize * 0.5)
    group:insert(fill)

    return newProxy(group, fill, {
        width = width, height = height,
        aspect = aspect,
        cornerRadius = normalizedR,
        smoothness = defaultSmoothness(minSize * 0.5),
        effectName = "filter.custom.sdf_rounded_rect",
    }, "roundedRect")
end
```

- [ ] **Step 2: Write factory functions for hexagon, pentagon, octagon, triangle, diamond, cross**

```lua
-- Helper for simple radius-based shapes
local function newRadiusShape(x, y, radius, effectName, shapeType, sdfRadius)
    M.init()
    sdfRadius = sdfRadius or 0.9
    local size = radius * 2
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(size, size)
    fill.fill.effect = effectName
    fill.fill.effect.radius = sdfRadius
    fill.fill.effect.smoothness = defaultSmoothness(radius)
    group:insert(fill)

    return newProxy(group, fill, {
        width = size, height = size,
        radius = radius,
        sdfRadius = sdfRadius,
        smoothness = defaultSmoothness(radius),
        effectName = effectName,
    }, shapeType)
end

function M.newHexagon(x, y, radius)
    return newRadiusShape(x, y, radius, "filter.custom.sdf_hexagon", "hexagon", 0.9)
end

function M.newPentagon(x, y, radius)
    return newRadiusShape(x, y, radius, "filter.custom.sdf_pentagon", "pentagon", 0.85)
end

function M.newOctagon(x, y, radius)
    return newRadiusShape(x, y, radius, "filter.custom.sdf_octagon", "octagon", 0.85)
end

function M.newTriangle(x, y, radius)
    return newRadiusShape(x, y, radius, "filter.custom.sdf_triangle", "triangle", 0.8)
end

function M.newDiamond(x, y, width, height)
    M.init()
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(width, height)
    fill.fill.effect = "filter.custom.sdf_diamond"
    fill.fill.effect.aspect = width / height
    fill.fill.effect.smoothness = defaultSmoothness(MIN(width, height) * 0.5)
    group:insert(fill)

    return newProxy(group, fill, {
        width = width, height = height,
        aspect = width / height,
        smoothness = defaultSmoothness(MIN(width, height) * 0.5),
        effectName = "filter.custom.sdf_diamond",
    }, "diamond")
end

function M.newCross(x, y, size, thickness)
    M.init()
    thickness = thickness or 0.3
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(size, size)
    fill.fill.effect = "filter.custom.sdf_cross"
    fill.fill.effect.thickness = thickness
    fill.fill.effect.smoothness = defaultSmoothness(size * 0.5)
    group:insert(fill)

    return newProxy(group, fill, {
        width = size, height = size,
        thickness = thickness,
        smoothness = defaultSmoothness(size * 0.5),
        effectName = "filter.custom.sdf_cross",
    }, "cross")
end
```

- [ ] **Step 3: Write factory functions for heart, crescent, ring, star, pill**

```lua
function M.newHeart(x, y, radius)
    return newRadiusShape(x, y, radius, "filter.custom.sdf_heart", "heart", 0.4)
end

function M.newCrescent(x, y, radius, offset)
    M.init()
    offset = offset or 0.3
    local size = radius * 2
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(size, size)
    fill.fill.effect = "filter.custom.sdf_crescent"
    fill.fill.effect.radius = 0.9
    fill.fill.effect.offset = offset
    fill.fill.effect.smoothness = defaultSmoothness(radius)
    group:insert(fill)

    return newProxy(group, fill, {
        width = size, height = size,
        radius = radius,
        sdfRadius = 0.9,
        offset = offset,
        smoothness = defaultSmoothness(radius),
        effectName = "filter.custom.sdf_crescent",
    }, "crescent")
end

function M.newRing(x, y, outerRadius, innerRadius, startAngle, endAngle)
    M.init()
    innerRadius = innerRadius or (outerRadius * 0.6)
    startAngle = startAngle or 0
    endAngle = endAngle or 360
    local size = outerRadius * 2
    local group = display.newGroup()
    group.x, group.y = x, y

    -- Normalize angles on Lua side (fix wrap-around bug)
    local startRad = RAD(startAngle) % TWO_PI
    local endRad = RAD(endAngle) % TWO_PI
    if endRad <= startRad and endAngle ~= startAngle then
        endRad = endRad + TWO_PI
    end

    local fill = createObject(size, size)
    fill.fill.effect = "filter.custom.sdf_ring"
    fill.fill.effect.innerRadius = innerRadius / outerRadius
    fill.fill.effect.outerRadius = 0.95
    fill.fill.effect.startAngle = startRad
    fill.fill.effect.endAngle = endRad
    group:insert(fill)

    return newProxy(group, fill, {
        width = size, height = size,
        outerRadius = outerRadius,
        innerRadius = innerRadius,
        smoothness = 0.01,  -- hardcoded in shader
        effectName = "filter.custom.sdf_ring",
    }, "ring")
end

function M.newStar(x, y, radius, points, innerRadius)
    M.init()
    points = points or 5
    innerRadius = innerRadius or (radius * 0.4)
    local innerRatio = innerRadius / radius
    -- Round to 4 decimal places for cache key stability
    innerRatio = math.floor(innerRatio * 10000 + 0.5) / 10000

    local effectName = getStarEffectName(points, innerRatio)
    local size = radius * 2
    local group = display.newGroup()
    group.x, group.y = x, y

    local fill = createObject(size, size)
    fill.fill.effect = effectName
    fill.fill.effect.radius = 0.9
    fill.fill.effect.smoothness = defaultSmoothness(radius)
    group:insert(fill)

    return newProxy(group, fill, {
        width = size, height = size,
        radius = radius,
        points = points,
        innerRatio = innerRatio,
        sdfRadius = 0.9,
        smoothness = defaultSmoothness(radius),
        effectName = effectName,
    }, "star")
end

function M.newPill(x, y, width, height)
    local proxy = M.newRoundedRect(x, y, width, height, height * 0.5)
    proxy._type = "pill"
    return proxy
end
```

- [ ] **Step 4: Add auto-init and module return**

```lua
-- Auto-initialize on require
M.init()

return M
```

- [ ] **Step 5: Commit all factories**

```bash
git add sdf_shapes.lua
git commit -m "feat(sdf): all 15 factory functions with native Solar2D API"
```

---

### Task 4: Basic Visual Test

**Files:**
- Create: `demo.lua` (~80 lines)

- [ ] **Step 1: Write basic demo exercising all shapes**

```lua
local sdf = require("sdf_shapes")

-- Background
display.setDefault("background", 0.15, 0.15, 0.15)

local shapes = {}
local cols = 5
local startX, startY = 60, 80
local spacingX, spacingY = 70, 80

local function addShape(shape, label, col, row)
    shape:setFillColor(0.3, 0.7, 1.0)
    shapes[#shapes + 1] = shape
    local t = display.newText(label, startX + (col-1)*spacingX, startY + (row-1)*spacingY + 35, native.systemFontBold, 9)
    t:setFillColor(0.7, 0.7, 0.7)
end

local cx, cy = startX, startY
local r = 25

-- Row 1
addShape(sdf.newCircle(cx, cy, r),                          "circle",  1, 1)
addShape(sdf.newEllipse(cx+70, cy, 50, 30),                 "ellipse", 2, 1)
addShape(sdf.newRect(cx+140, cy, 50, 40),                   "rect",    3, 1)
addShape(sdf.newRoundedRect(cx+210, cy, 50, 40, 8),         "rndRect", 4, 1)
addShape(sdf.newHexagon(cx+280, cy, r),                     "hexagon", 5, 1)

-- Row 2
addShape(sdf.newPentagon(cx, cy+80, r),                     "pentagon",  1, 2)
addShape(sdf.newOctagon(cx+70, cy+80, r),                   "octagon",   2, 2)
addShape(sdf.newTriangle(cx+140, cy+80, r),                 "triangle",  3, 2)
addShape(sdf.newDiamond(cx+210, cy+80, 40, 50),             "diamond",   4, 2)
addShape(sdf.newStar(cx+280, cy+80, r, 5, r*0.4),           "star5",     5, 2)

-- Row 3
addShape(sdf.newRing(cx, cy+160, r, r*0.6),                 "ring",      1, 3)
addShape(sdf.newRing(cx+70, cy+160, r, r*0.6, 0, 270),      "arc",       2, 3)
addShape(sdf.newCrescent(cx+140, cy+160, r, 0.3),           "crescent",  3, 3)
addShape(sdf.newHeart(cx+210, cy+160, r),                   "heart",     4, 3)
addShape(sdf.newCross(cx+280, cy+160, 50, 0.3),             "cross",     5, 3)

-- Row 4: pill
addShape(sdf.newPill(cx+35, cy+240, 80, 30),                "pill",      1, 4)

-- Row 4: star variants
addShape(sdf.newStar(cx+140, cy+240, r, 3, r*0.4),          "star3",     3, 4)
addShape(sdf.newStar(cx+210, cy+240, r, 8, r*0.4),          "star8",     4, 4)
addShape(sdf.newStar(cx+280, cy+240, r, 12, r*0.3),         "star12",    5, 4)

-- Test removeSelf
local temp = sdf.newCircle(160, 400, 20)
temp:setFillColor(1, 0, 0)
timer.performWithDelay(2000, function()
    temp:removeSelf()
    print("removeSelf test: OK")
end)
```

- [ ] **Step 2: Commit demo**

```bash
git add demo.lua
git commit -m "feat(sdf): basic visual demo for all 15 shapes"
```

- [ ] **Step 3: Run in Solar2D Simulator, verify all 15 shapes render correctly**

Visual checklist:
- All shapes visible with blue fill on dark background
- No shader errors in console
- Star shapes have sharp points (not rounded)
- Heart shape renders without NaN artifacts
- Ring arc (270°) clips correctly
- removeSelf cleans up after 2s
- No console errors

---

## Chunk 2: Stroke & Shadow

### Task 5: Stroke System — Size-Based Dual Object

**Files:**
- Modify: `sdf_shapes.lua` — add `_updateStroke` to proxy, update `newProxy`

- [ ] **Step 1: Write `_createStrokeObject` helper**

This creates a stroke display object using the same shader but at larger size:

```lua
-- Creates a stroke object: same SDF shader, larger size
-- The fill object on top masks the interior, leaving only the border visible
local function createStrokeObject(params, strokeWidth)
    local p = params
    local sw2 = strokeWidth * 2
    local w = p.width + sw2
    local h = p.height + sw2

    local obj = createObject(w, h)
    obj.fill.effect = p.effectName

    -- Adjust SDF uniforms for the larger canvas
    local shapeType = p.shapeType or ""
    if p.aspect then
        -- Aspect-based shapes: recalculate aspect for new dimensions
        obj.fill.effect.aspect = w / h
    end
    if p.sdfRadius then
        obj.fill.effect.radius = p.sdfRadius
    end
    if p.cornerRadius then
        obj.fill.effect.cornerRadius = p.cornerRadius
    end
    if p.thickness then
        obj.fill.effect.thickness = p.thickness
    end
    if p.offset then
        obj.fill.effect.offset = p.offset
    end
    if p.smoothness and p.effectName ~= "filter.custom.sdf_ring" then
        obj.fill.effect.smoothness = p.smoothness
    end

    return obj
end
```

- [ ] **Step 2: Write `_updateStroke` method on proxy**

```lua
-- Add to proxyMT.__index, as a stored function on proxy creation:

local function updateStroke(self, strokeWidth)
    if strokeWidth > 0 then
        if not self._stroke then
            -- Lazy create
            self._stroke = createStrokeObject(self._params, strokeWidth)
            if self._strokeColor then
                self._stroke:setFillColor(unpack(self._strokeColor))
            end
            -- Insert stroke BEFORE fill (behind it)
            local fillIndex = 1
            if self._shadow then fillIndex = 2 end
            self._group:insert(fillIndex, self._stroke)
        else
            -- Update existing: resize
            local sw2 = strokeWidth * 2
            local newW = self._params.width + sw2
            local newH = self._params.height + sw2
            self._stroke.width = newW
            self._stroke.height = newH
            -- Re-apply effect (Solar2D may need this after resize)
            self._stroke.fill.effect = self._params.effectName
            if self._params.aspect then
                self._stroke.fill.effect.aspect = newW / newH
            end
            if self._params.sdfRadius then
                self._stroke.fill.effect.radius = self._params.sdfRadius
            end
            if self._params.cornerRadius then
                self._stroke.fill.effect.cornerRadius = self._params.cornerRadius
            end
            if self._params.thickness then
                self._stroke.fill.effect.thickness = self._params.thickness
            end
            if self._params.offset then
                self._stroke.fill.effect.offset = self._params.offset
            end
            if self._params.smoothness and self._params.effectName ~= "filter.custom.sdf_ring" then
                self._stroke.fill.effect.smoothness = self._params.smoothness
            end
        end
        self._stroke.isVisible = true
    else
        if self._stroke then
            self._stroke.isVisible = false
        end
    end
end
```

- [ ] **Step 3: Wire `_updateStroke` into `newProxy`**

```lua
-- Modify newProxy to attach the method:
local function newProxy(group, fill, params, shapeType)
    params.shapeType = shapeType
    local proxy = {
        _group  = group,
        _fill   = fill,
        _stroke = nil,
        _shadow = nil,
        _params = params,
        _type   = shapeType,
        _strokeWidth = 0,
        _strokeColor = {1, 1, 1},
        _smoothness = params.smoothness,
        _updateStroke = updateStroke,
    }
    setmetatable(proxy, proxyMT)
    return proxy
end
```

- [ ] **Step 4: Commit stroke system**

```bash
git add sdf_shapes.lua
git commit -m "feat(sdf): stroke system — size-based dual object approach"
```

- [ ] **Step 5: Add stroke tests to demo.lua**

Add after existing shapes:

```lua
-- Row 5: Stroke tests
local strokeCircle = sdf.newCircle(cx, cy+320, r)
strokeCircle:setFillColor(0.3, 0.7, 1.0)
strokeCircle:setStrokeColor(1, 1, 0)
strokeCircle.strokeWidth = 3
display.newText("stroke", cx, cy+355, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)

local strokeRect = sdf.newRoundedRect(cx+70, cy+320, 50, 40, 8)
strokeRect:setFillColor(0.3, 0.7, 1.0)
strokeRect:setStrokeColor(1, 0.5, 0)
strokeRect.strokeWidth = 2
display.newText("strRect", cx+70, cy+355, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)

local strokeStar = sdf.newStar(cx+140, cy+320, r, 5, r*0.4)
strokeStar:setFillColor(0.3, 0.7, 1.0)
strokeStar:setStrokeColor(1, 0, 0.5)
strokeStar.strokeWidth = 3
display.newText("strStar", cx+140, cy+355, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)

-- Dynamic stroke width change
local dynCircle = sdf.newCircle(cx+210, cy+320, r)
dynCircle:setFillColor(0.3, 0.7, 1.0)
dynCircle:setStrokeColor(0, 1, 0.5)
local sw = 0
timer.performWithDelay(50, function()
    sw = (sw + 0.5) % 8
    dynCircle.strokeWidth = sw
end, 0)
display.newText("anim", cx+210, cy+355, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)
```

- [ ] **Step 6: Commit stroke demo and verify visually**

```bash
git add demo.lua
git commit -m "feat(sdf): stroke visual tests in demo"
```

---

### Task 6: Shadow System

**Files:**
- Modify: `sdf_shapes.lua` — add `_updateShadow`

- [ ] **Step 1: Write `_updateShadow` method**

```lua
local function updateShadow(self, shadowConfig)
    if shadowConfig == nil then
        -- Remove shadow
        if self._shadow then
            self._shadow:removeSelf()
            self._shadow = nil
        end
        return
    end

    local blur = shadowConfig.blur or 8
    local offsetX = shadowConfig.offsetX or 4
    local offsetY = shadowConfig.offsetY or 4
    local color = shadowConfig.color or {0, 0, 0, 0.3}
    local p = self._params

    local sw2 = blur * 2
    local w = p.width + sw2
    local h = p.height + sw2

    if not self._shadow then
        -- Lazy create
        self._shadow = createObject(w, h)
        -- Insert at position 1 (behind everything)
        self._group:insert(1, self._shadow)
    else
        self._shadow.width = w
        self._shadow.height = h
    end

    -- Apply same SDF shader with increased smoothness for blur
    self._shadow.fill.effect = p.effectName
    if p.aspect then
        self._shadow.fill.effect.aspect = w / h
    end
    if p.sdfRadius then
        self._shadow.fill.effect.radius = p.sdfRadius
    end
    if p.cornerRadius then
        self._shadow.fill.effect.cornerRadius = p.cornerRadius
    end
    if p.thickness then
        self._shadow.fill.effect.thickness = p.thickness
    end
    if p.offset then
        self._shadow.fill.effect.offset = p.offset
    end

    -- Key: larger smoothness = blur effect
    -- Known limitation: for non-convex shapes (star, heart, crescent, cross)
    -- the shadow follows SDF contours rather than spreading uniformly like a
    -- true Gaussian blur. Use snapshot + blur filter for those cases.
    local blurSmooth = blur / (MAX(w, h) * 0.5)
    if p.effectName ~= "filter.custom.sdf_ring" then
        self._shadow.fill.effect.smoothness = blurSmooth
    end

    -- Position and color
    self._shadow.x = offsetX
    self._shadow.y = offsetY
    self._shadow:setFillColor(unpack(color))
end
```

- [ ] **Step 2: Wire into newProxy**

```lua
-- Add to newProxy:
    _updateShadow = updateShadow,
```

- [ ] **Step 3: Commit shadow system**

```bash
git add sdf_shapes.lua
git commit -m "feat(sdf): shadow system — SDF-based soft falloff"
```

- [ ] **Step 4: Add shadow tests to demo.lua**

```lua
-- Row 5 (continued): Shadow tests
local shadowCircle = sdf.newCircle(cx+280, cy+320, r)
shadowCircle:setFillColor(0.3, 0.7, 1.0)
shadowCircle.shadow = { offsetX = 3, offsetY = 3, blur = 6, color = {0,0,0,0.4} }
display.newText("shadow", cx+280, cy+355, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)

-- Combined stroke + shadow
local combo = sdf.newRoundedRect(160, cy+420, 120, 50, 12)
combo:setFillColor(1, 1, 1)
combo:setStrokeColor(0.2, 0.5, 1.0)
combo.strokeWidth = 2
combo.shadow = { offsetX = 4, offsetY = 4, blur = 10, color = {0,0,0,0.3} }
display.newText("stroke+shadow", 160, cy+455, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)
```

- [ ] **Step 5: Commit and verify visually**

```bash
git add demo.lua
git commit -m "feat(sdf): shadow visual tests in demo"
```

---

## Chunk 3: Advanced Features — Gradient & Boolean Ops

### Task 7: Gradient System

**Files:**
- Modify: `sdf_shapes.lua` — add gradient composite shaders, `setFillGradient` method

- [ ] **Step 1: Register gradient composite effects for each shape**

Since each shape has its own SDF math, we need a composite shader per shape that combines the gradient texture with the SDF alpha. To avoid explosion, use a generic approach: render the SDF shape into a snapshot, then apply a simple alpha-mask composite.

```lua
-- Add to shaders table: a generic gradient masking composite
local gradientShaders = {
    gradientMask = {
        category = "composite",
        name = "sdf_gradient_mask",
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_COLOR vec4 gradient = texture2D(CoronaSampler0, uv);
                P_COLOR vec4 mask = texture2D(CoronaSampler1, uv);
                return vec4(gradient.rgb * mask.a, mask.a);
            }
        ]],
    },
}

-- Register in M.init():
for key, shader in pairs(gradientShaders) do
    local ok, err = pcall(graphics.defineEffect, shader)
    if not ok then
        print("SDF: Failed to register " .. key .. ": " .. tostring(err))
    end
end
```

- [ ] **Step 2: Write `setFillGradient` method**

```lua
-- Add to proxyMT.__index:
if key == "setFillGradient" then
    return function(obj, config)
        if config == nil then
            -- Remove gradient, restore solid fill
            if obj._gradientSnap then
                -- Restore original fill
                obj._fill.isVisible = true
                obj._gradientSnap:removeSelf()
                obj._gradientSnap = nil
            end
            return
        end

        local p = obj._params
        local w, h = p.width, p.height

        -- Create gradient source: a rect with gradient fill
        local gradSnap = display.newSnapshot(w, h)

        local gradRect = display.newRect(0, 0, w, h)
        local c1 = config.color1 or {1, 0, 0}
        local c2 = config.color2 or {0, 0, 1}
        local dir = config.direction or "down"

        gradRect.fill = {
            type = "gradient",
            color1 = c1,
            color2 = c2,
            direction = dir,
        }
        gradSnap.group:insert(gradRect)
        gradSnap:invalidate()

        -- Create SDF mask: render the fill object into a snapshot
        local maskSnap = display.newSnapshot(w, h)
        -- Temporarily reparent fill to mask snapshot
        local fillParent = obj._fill.parent
        maskSnap.group:insert(obj._fill)
        obj._fill.x, obj._fill.y = 0, 0
        maskSnap:invalidate()

        -- Apply composite: gradient * SDF alpha
        gradSnap.fill = {
            type = "composite",
            paint1 = { type = "image", filename = gradSnap },
            paint2 = { type = "image", filename = maskSnap },
        }
        gradSnap.fill.effect = "composite.custom.sdf_gradient_mask"

        -- Insert gradient snapshot into group where fill was
        obj._fill.isVisible = false
        -- Restore fill to group (hidden)
        obj._group:insert(obj._fill)
        obj._fill.x, obj._fill.y = 0, 0

        -- Insert gradient snap into group
        local insertIdx = 1
        if obj._shadow then insertIdx = 2 end
        obj._group:insert(insertIdx + 1, gradSnap)

        -- Store for cleanup
        if obj._gradientSnap then obj._gradientSnap:removeSelf() end
        obj._gradientSnap = gradSnap
        obj._gradientMaskSnap = maskSnap
    end
end
```

**Note:** The snapshot-as-paint-source syntax (`{ type = "image", filename = snapshot }`) should be verified against Solar2D's composite paint API during implementation. If this doesn't work, the alternative is to capture the snapshot to a file with `display.save()` and reference the filename. This is a blocking verification step before gradient can ship.

- [ ] **Step 3: Commit gradient system**

```bash
git add sdf_shapes.lua
git commit -m "feat(sdf): gradient fill via composite effect + snapshot"
```

- [ ] **Step 4: Add gradient tests to demo.lua**

```lua
-- Row 6: Gradient tests
local gradCircle = sdf.newCircle(cx, cy+490, r)
gradCircle:setFillGradient({
    type = "linear",
    color1 = {1, 0, 0},
    color2 = {0, 0, 1},
    direction = "down",
})
display.newText("gradient", cx, cy+525, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)

local gradRect = sdf.newRoundedRect(cx+70, cy+490, 50, 40, 8)
gradRect:setFillGradient({
    color1 = {1, 1, 0},
    color2 = {0, 1, 0},
    direction = "right",
})
display.newText("gradRect", cx+70, cy+525, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)
```

- [ ] **Step 5: Commit and verify visually**

```bash
git add demo.lua
git commit -m "feat(sdf): gradient visual tests in demo"
```

---

### Task 8: Boolean Operations

**Files:**
- Modify: `sdf_shapes.lua` — add composite shaders + union/intersect/subtract functions

- [ ] **Step 1: Register boolean composite shaders**

```lua
-- Add to gradientShaders (or a separate boolShaders table):
    boolIntersect = {
        category = "composite",
        name = "sdf_bool_intersect",
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_COLOR vec4 c1 = texture2D(CoronaSampler0, uv);
                P_COLOR vec4 c2 = texture2D(CoronaSampler1, uv);
                return vec4(c1.rgb, c1.a * c2.a);
            }
        ]],
    },
    boolSubtract = {
        category = "composite",
        name = "sdf_bool_subtract",
        fragment = [[
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_COLOR vec4 c1 = texture2D(CoronaSampler0, uv);
                P_COLOR vec4 c2 = texture2D(CoronaSampler1, uv);
                return vec4(c1.rgb, c1.a * (1.0 - c2.a));
            }
        ]],
    },
```

- [ ] **Step 2: Write boolean proxy wrapper**

```lua
local boolProxyMT = {}
boolProxyMT.__index = function(self, key)
    if key == "removeSelf" then
        return function(obj)
            if obj._snapshot then obj._snapshot:removeSelf() end
            -- Clean up operand snapshots
            if obj._snapshots then
                for _, s in ipairs(obj._snapshots) do
                    if s.removeSelf then s:removeSelf() end
                end
            end
            obj._snapshot = nil
            obj._snapshots = nil
        end
    end
    if GROUP_PROPS[key] and self._snapshot then
        return self._snapshot[key]
    end
    return rawget(self, key)
end
boolProxyMT.__newindex = function(self, key, value)
    if GROUP_PROPS[key] and self._snapshot then
        self._snapshot[key] = value
        return
    end
    rawset(self, key, value)
end
```

- [ ] **Step 3: Write union/intersect/subtract functions**

```lua
function M.union(a, b)
    local aw, ah = a._params.width, a._params.height
    local bw, bh = b._params.width, b._params.height
    local w = MAX(aw, bw) + 40  -- padding for offset shapes
    local h = MAX(ah, bh) + 40

    local snap = display.newSnapshot(w, h)
    snap.group:insert(a._group)
    snap.group:insert(b._group)
    snap:invalidate()

    local proxy = {
        _snapshot = snap,
        _snapshots = {},
        _operands = {a, b},
    }
    setmetatable(proxy, boolProxyMT)
    return proxy
end

function M.intersect(a, b)
    local w = MAX(a._params.width, b._params.width) + 40
    local h = MAX(a._params.height, b._params.height) + 40

    local snapA = display.newSnapshot(w, h)
    snapA.group:insert(a._group)
    snapA:invalidate()

    local snapB = display.newSnapshot(w, h)
    snapB.group:insert(b._group)
    snapB:invalidate()

    local result = display.newSnapshot(w, h)
    result.fill = {
        type = "composite",
        paint1 = { type = "image", filename = snapA },
        paint2 = { type = "image", filename = snapB },
    }
    result.fill.effect = "composite.custom.sdf_bool_intersect"
    result:invalidate()

    local proxy = {
        _snapshot = result,
        _snapshots = {snapA, snapB},
        _operands = {a, b},
    }
    setmetatable(proxy, boolProxyMT)
    return proxy
end

function M.subtract(a, b)
    local w = MAX(a._params.width, b._params.width) + 40
    local h = MAX(a._params.height, b._params.height) + 40

    local snapA = display.newSnapshot(w, h)
    snapA.group:insert(a._group)
    snapA:invalidate()

    local snapB = display.newSnapshot(w, h)
    snapB.group:insert(b._group)
    snapB:invalidate()

    local result = display.newSnapshot(w, h)
    result.fill = {
        type = "composite",
        paint1 = { type = "image", filename = snapA },
        paint2 = { type = "image", filename = snapB },
    }
    result.fill.effect = "composite.custom.sdf_bool_subtract"
    result:invalidate()

    local proxy = {
        _snapshot = result,
        _snapshots = {snapA, snapB},
        _operands = {a, b},
    }
    setmetatable(proxy, boolProxyMT)
    return proxy
end
```

**Note:** The snapshot-as-paint-source reference must be verified against Solar2D's actual composite paint API. If `{ type = "image", filename = snapshot }` doesn't work, fallback to `display.save()` + file reference. This is a blocking verification step — test with a minimal composite example before wiring into the full boolean system.

- [ ] **Step 4: Commit boolean ops**

```bash
git add sdf_shapes.lua
git commit -m "feat(sdf): boolean operations — union, intersect, subtract"
```

- [ ] **Step 5: Add boolean tests to demo.lua**

```lua
-- Row 7: Boolean operation tests
local boolA = sdf.newCircle(0, 0, 25)
boolA:setFillColor(1, 0, 0)
local boolB = sdf.newCircle(15, 0, 25)
boolB:setFillColor(0, 0, 1)

local unionShape = sdf.union(boolA, boolB)
unionShape.x, unionShape.y = cx, cy + 560
display.newText("union", cx, cy+595, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)

local intA = sdf.newCircle(0, 0, 25)
intA:setFillColor(1, 0.5, 0)
local intB = sdf.newRoundedRect(10, 0, 40, 40, 5)
intB:setFillColor(1, 0.5, 0)
local interShape = sdf.intersect(intA, intB)
interShape.x, interShape.y = cx+100, cy + 560
display.newText("intersect", cx+100, cy+595, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)

local subA = sdf.newCircle(0, 0, 25)
subA:setFillColor(0, 1, 0.5)
local subB = sdf.newCircle(15, 0, 20)
subB:setFillColor(0, 1, 0.5)
local subShape = sdf.subtract(subA, subB)
subShape.x, subShape.y = cx+200, cy + 560
display.newText("subtract", cx+200, cy+595, native.systemFontBold, 9):setFillColor(0.7,0.7,0.7)
```

- [ ] **Step 6: Commit and verify visually**

```bash
git add demo.lua
git commit -m "feat(sdf): boolean ops visual tests in demo"
```

---

## Chunk 4: Polish & Cleanup

### Task 9: Final Polish

**Files:**
- Modify: `sdf_shapes.lua` — final cleanup
- Delete: `sdf_shapes_stroke.lua`, `sdf_shapes_simple_stroke.lua`, `demo_basic.lua`, `demo_stroke.lua`
- Keep: `main.lua` (update to use new API)

- [ ] **Step 1: Delete old files**

```bash
rm sdf_shapes_stroke.lua sdf_shapes_simple_stroke.lua demo_basic.lua demo_stroke.lua
```

- [ ] **Step 2: Update main.lua to use new API**

Replace `main.lua` content to be a simple entry point that loads the demo:

```lua
-- SDF Shapes Library Demo
-- Run this file to see all shapes, stroke, shadow, gradient, and boolean operations

local composer = require("composer")

-- Or just load demo directly if not using composer:
require("demo")
```

Alternatively, if the project doesn't use composer, just make `main.lua` a redirect:

```lua
require("demo")
```

- [ ] **Step 3: Final review pass on sdf_shapes.lua**

Verify:
- No references to old `"lib.sdf_shapes"` paths
- No references to `"sdf_shapes/white_pixel.png"` (should be `"white_pixel.png"`)
- `M.init()` only called once at module load
- All `print()` debug statements either removed or gated behind a `DEBUG` flag
- Module returns `M`

- [ ] **Step 4: Commit final cleanup**

```bash
git add -A
git commit -m "chore(sdf): remove old files, clean up paths, final polish"
```

- [ ] **Step 5: Full visual verification**

Run in Solar2D Simulator and verify:
- [ ] All 15 base shapes render correctly
- [ ] Stroke works on circle, roundedRect, star
- [ ] Animated stroke width changes smoothly
- [ ] Shadow appears behind shapes with correct offset/blur
- [ ] Combined stroke + shadow renders correctly
- [ ] Gradient fills render with correct colors/direction
- [ ] Boolean union shows merged shapes
- [ ] Boolean intersect shows only overlap
- [ ] Boolean subtract shows A minus B
- [ ] removeSelf cleans up without errors
- [ ] No console errors or shader compilation failures
- [ ] Ring arc (350° to 10°) wraps correctly
