------------------------------------------------------------------------------
-- SDF Shapes Library for Solar2D
-- Anti-aliased vector shapes via GLSL signed distance fields
------------------------------------------------------------------------------

-- ─────────────────────────────────────────────
-- Section 1: Constants & Utils
-- ─────────────────────────────────────────────

local M = {}
local _initialized   = false
local _registeredShaders = {}

local PI     = math.pi
local TWO_PI = math.pi * 2.0
local RAD    = math.rad
local SQRT   = math.sqrt
local MAX    = math.max
local MIN    = math.min
local FORMAT = string.format

local function getContentScale()
    if display and display.pixelWidth and display.actualContentWidth
       and display.actualContentWidth > 0 then
        return display.pixelWidth / display.actualContentWidth
    end
    return 1
end

local function defaultSmoothness(sizeInContent)
    return 0.5 / (sizeInContent * getContentScale())
end

local function createObject(width, height)
    local ok, obj = pcall(function()
        return display.newImageRect("sdf_shapes/white_pixel.png", width, height)
    end)
    if not ok or not obj then
        obj = display.newRect(0, 0, width, height)
        obj:setFillColor(1, 1, 1)
    end
    return obj
end

-- ─────────────────────────────────────────────
-- Section 2: Shader Definitions
-- ─────────────────────────────────────────────

local shaders = {}

-- Circle
shaders.circle = {
    category = "filter",
    name     = "sdf_circle",
    uniformData = {
        { name = "radius",     type = "scalar", index = 0, default = 0.95 },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float dist = length(p);
            float alpha = 1.0 - smoothstep(
                u_UserData0 - u_UserData1,
                u_UserData0 + u_UserData1,
                dist
            );
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Ellipse
shaders.ellipse = {
    category = "filter",
    name     = "sdf_ellipse",
    uniformData = {
        { name = "aspect",     type = "scalar", index = 0, default = 1.0  },
        { name = "radius",     type = "scalar", index = 1, default = 0.95 },
        { name = "smoothness", type = "scalar", index = 2, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // aspect (width/height)
        uniform P_DEFAULT float u_UserData1; // radius
        uniform P_DEFAULT float u_UserData2; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_UserData0;
            float dist = length(p);
            float alpha = 1.0 - smoothstep(
                u_UserData1 - u_UserData2,
                u_UserData1 + u_UserData2,
                dist
            );
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Rect
shaders.rect = {
    category = "filter",
    name     = "sdf_rect",
    uniformData = {
        { name = "aspect",     type = "scalar", index = 0, default = 1.0  },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // aspect
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float aspect = u_UserData0;
            p.x *= aspect;
            vec2 d = abs(p) - vec2(aspect, 1.0);
            float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
            float smooth = u_UserData1 / max(aspect, 1.0);
            float alpha = 1.0 - smoothstep(-smooth, smooth, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Rounded Rect
shaders.roundedRect = {
    category = "filter",
    name     = "sdf_rounded_rect",
    uniformData = {
        { name = "aspect",       type = "scalar", index = 0, default = 1.0  },
        { name = "cornerRadius", type = "scalar", index = 1, default = 0.1  },
        { name = "smoothness",   type = "scalar", index = 2, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // aspect
        uniform P_DEFAULT float u_UserData1; // cornerRadius
        uniform P_DEFAULT float u_UserData2; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float aspect = u_UserData0;
            float radius = u_UserData1;
            p.x *= aspect;
            vec2 b = vec2(aspect, 1.0) - vec2(radius * 2.0);
            vec2 q = abs(p) - b;
            float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
            float alpha = 1.0 - smoothstep(-u_UserData2, u_UserData2, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Hexagon
shaders.hexagon = {
    category = "filter",
    name     = "sdf_hexagon",
    uniformData = {
        { name = "radius",     type = "scalar", index = 0, default = 0.9  },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            p = abs(p);
            float dist = max(dot(vec2(1.73205, 1.0), p) / 2.0, p.x) - u_UserData0;
            float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Pentagon
shaders.pentagon = {
    category = "filter",
    name     = "sdf_pentagon",
    uniformData = {
        { name = "radius",     type = "scalar", index = 0, default = 0.85 },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // smoothness

        #define PI 3.14159265359
        #define TWO_PI 6.28318530718

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float r = u_UserData0;
            p.y = -p.y - 0.15;
            float a = mod(atan(p.x, p.y) + PI, TWO_PI) / 5.0;
            vec2 q = length(p) * vec2(cos(a), sin(a));
            float dist = max(q.x - 0.809017 * r, abs(q.y) - 0.587785 * r);
            float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Octagon
shaders.octagon = {
    category = "filter",
    name     = "sdf_octagon",
    uniformData = {
        { name = "radius",     type = "scalar", index = 0, default = 0.85 },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = abs((uv - 0.5) * 2.0);
            float r = u_UserData0;
            float dist = dot(p, vec2(1.0, 0.414213562)) - r * 1.414213562;
            dist = max(dist, p.x - r);
            dist = max(dist, p.y - r);
            float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Triangle
shaders.triangle = {
    category = "filter",
    name     = "sdf_triangle",
    uniformData = {
        { name = "radius",     type = "scalar", index = 0, default = 0.8  },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            p.y += 0.35;
            float dist = max(abs(p.x) * 1.73205 + p.y, -p.y) - u_UserData0;
            float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Diamond
shaders.diamond = {
    category = "filter",
    name     = "sdf_diamond",
    uniformData = {
        { name = "aspect",     type = "scalar", index = 0, default = 1.0  },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // aspect
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_UserData0;
            float dist = abs(p.x) + abs(p.y) - 1.0;
            float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Cross
shaders.cross = {
    category = "filter",
    name     = "sdf_cross",
    uniformData = {
        { name = "thickness",  type = "scalar", index = 0, default = 0.3  },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // thickness
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float t = u_UserData0;
            float h1 = length(vec2(max(abs(p.x) - 1.0, 0.0), max(abs(p.y) - t, 0.0)));
            float h2 = length(vec2(max(abs(p.x) - t, 0.0), max(abs(p.y) - 1.0, 0.0)));
            float dist = min(h1, h2);
            float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Heart (BUG FIX: use sqrt(max(...)) not pow)
shaders.heart = {
    category = "filter",
    name     = "sdf_heart",
    uniformData = {
        { name = "radius",     type = "scalar", index = 0, default = 0.4  },
        { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            p.y = -p.y + 0.25;
            vec2 q = abs(p);
            q.y = -p.y;
            float dist = sqrt(max(q.x * 0.5, 0.0)) + q.y - u_UserData0;
            float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Crescent
shaders.crescent = {
    category = "filter",
    name     = "sdf_crescent",
    uniformData = {
        { name = "radius",     type = "scalar", index = 0, default = 0.9  },
        { name = "offset",     type = "scalar", index = 1, default = 0.3  },
        { name = "smoothness", type = "scalar", index = 2, default = 0.01 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // radius
        uniform P_DEFAULT float u_UserData1; // offset
        uniform P_DEFAULT float u_UserData2; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float d1 = length(p) - u_UserData0;
            float d2 = length(p - vec2(u_UserData1, 0.0)) - u_UserData0;
            float dist = max(d1, -d2);
            float alpha = 1.0 - smoothstep(-u_UserData2, u_UserData2, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Ring / Arc (all 4 uniforms, no branching on angles)
shaders.ring = {
    category = "filter",
    name     = "sdf_ring",
    uniformData = {
        { name = "innerRadius", type = "scalar", index = 0, default = 0.4 },
        { name = "outerRadius", type = "scalar", index = 1, default = 0.95 },
        { name = "startAngle",  type = "scalar", index = 2, default = 0.0 },
        { name = "endAngle",    type = "scalar", index = 3, default = 6.28318530718 },
    },
    fragment = [[
        uniform P_DEFAULT float u_UserData0; // innerRadius
        uniform P_DEFAULT float u_UserData1; // outerRadius
        uniform P_DEFAULT float u_UserData2; // startAngle (radians, [0, 2pi))
        uniform P_DEFAULT float u_UserData3; // endAngle   (radians, > startAngle)

        #define PI   3.14159265359
        #define TWO_PI 6.28318530718

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float r = length(p);

            // Radial mask
            float radialDist = max(u_UserData0 - r, r - u_UserData1);

            // Angle in [0, 2pi)
            float angle = atan(p.y, p.x);
            angle += step(angle, 0.0) * TWO_PI;

            float start = u_UserData2;
            float end   = u_UserData3;
            float range = (end - start) * 0.5;

            // Full-circle guard: skip angular clipping when range >= PI
            float isFullCircle = step(PI - 0.001, range);

            float mid = start + range;
            float angleDiff = abs(angle - mid);
            angleDiff = min(angleDiff, TWO_PI - angleDiff);
            float angleDist = max(0.0, angleDiff - range);
            angleDist *= (1.0 - isFullCircle);

            float dist = max(radialDist, angleDist * r);
            float alpha = 1.0 - smoothstep(-0.01, 0.01, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- ─────────────────────────────────────────────
-- Star: dynamic shader (baked constants)
-- ─────────────────────────────────────────────

local function makeStarShader(points, innerRatio)
    local an  = PI / points
    local m   = 2.0 + (1.0 - innerRatio) * (points - 2.0)
    local en  = PI / m
    local acs = FORMAT("vec2(%.8f, %.8f)", math.cos(an), math.sin(an))
    local ecs = FORMAT("vec2(%.8f, %.8f)", math.cos(en), math.sin(en))
    local anStr    = FORMAT("%.8f", an)
    local twoAnStr = FORMAT("%.8f", 2.0 * an)

    return FORMAT([[
        uniform P_DEFAULT float u_UserData0; // radius

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            vec2 p = (uv - 0.5) * 2.0;
            float r = u_UserData0;
            const vec2 acs = %s;
            const vec2 ecs = %s;
            float bn = mod(atan(p.x, p.y), %s) - %s;
            p = length(p) * vec2(cos(bn), abs(sin(bn)));
            p -= r * acs;
            p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
            float dist = length(p) * sign(p.x);
            float alpha = 1.0 - smoothstep(-0.01, 0.01, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]], acs, ecs, twoAnStr, anStr)
end

local function getStarEffectName(points, innerRatio)
    local key = FORMAT("sdf_star_%d_%.4f", points, innerRatio)
    if not _registeredShaders[key] then
        local shader = {
            category    = "filter",
            name        = key,
            uniformData = {
                { name = "radius", type = "scalar", index = 0, default = 0.9 },
            },
            fragment = makeStarShader(points, innerRatio),
        }
        local ok, err = pcall(graphics.defineEffect, shader)
        if not ok then
            print("SDF Shapes: FAILED to register star " .. key .. ": " .. tostring(err))
        else
            _registeredShaders[key] = true
        end
    end
    return "filter.custom." .. key
end

-- ─────────────────────────────────────────────
-- Section 3: Shader Registration
-- ─────────────────────────────────────────────

function M.init()
    if _initialized then return end

    for key, shader in pairs(shaders) do
        local ok, err = pcall(graphics.defineEffect, shader)
        if not ok then
            print("SDF Shapes: FAILED to register " .. key .. ": " .. tostring(err))
        else
            _registeredShaders[key] = true
        end
    end

    _initialized = true
end

-- ─────────────────────────────────────────────
-- Section 4: Proxy Object System
-- ─────────────────────────────────────────────

local proxyMT = {}

proxyMT.__index = function(t, k)
    -- Group passthrough (position, transform, visibility)
    if k == "x" or k == "y" or k == "alpha" or k == "isVisible"
    or k == "rotation" or k == "xScale" or k == "yScale"
    or k == "anchorX" or k == "anchorY" then
        return rawget(t, "_group")[k]
    end

    -- Size from params
    if k == "width"  then return rawget(t, "_params").width  end
    if k == "height" then return rawget(t, "_params").height end

    -- removeSelf
    if k == "removeSelf" then
        return function(self)
            local g = rawget(self, "_group")
            if g then g:removeSelf() end
            rawset(self, "_group",   nil)
            rawset(self, "_fill",    nil)
            rawset(self, "_stroke",  nil)
            rawset(self, "_shadow",  nil)
            rawset(self, "_params",  nil)
        end
    end

    -- setFillColor
    if k == "setFillColor" then
        return function(self, ...)
            local f = rawget(self, "_fill")
            if f then f:setFillColor(...) end
        end
    end

    -- setStrokeColor
    if k == "setStrokeColor" then
        return function(self, ...)
            local s = rawget(self, "_stroke")
            if s then s:setFillColor(...) end
            rawset(self, "_strokeColor", {...})
        end
    end

    return rawget(t, k)
end

proxyMT.__newindex = function(t, k, v)
    -- Group passthrough
    if k == "x" or k == "y" or k == "alpha" or k == "isVisible"
    or k == "rotation" or k == "xScale" or k == "yScale"
    or k == "anchorX" or k == "anchorY" then
        rawget(t, "_group")[k] = v
        return
    end

    -- strokeWidth
    if k == "strokeWidth" then
        rawset(t, "_strokeWidth", v)
        local upd = rawget(t, "_updateStroke")
        if upd then upd(t) end
        return
    end

    -- smoothness: update shader uniform live
    if k == "smoothness" then
        rawset(t, "_smoothness", v)
        local fill = rawget(t, "_fill")
        if fill and fill.fill and fill.fill.effect then
            fill.fill.effect.smoothness = v
        end
        return
    end

    -- shadow
    if k == "shadow" then
        rawset(t, "_shadow", v)
        local upd = rawget(t, "_updateShadow")
        if upd then upd(t) end
        return
    end

    rawset(t, k, v)
end

local function newProxy(group, fill, params, shapeType)
    local proxy = {}
    rawset(proxy, "_group",        group)
    rawset(proxy, "_fill",         fill)
    rawset(proxy, "_stroke",       nil)
    rawset(proxy, "_shadow",       nil)
    rawset(proxy, "_params",       params)
    rawset(proxy, "_type",         shapeType)
    rawset(proxy, "_strokeWidth",  0)
    rawset(proxy, "_strokeColor",  nil)
    rawset(proxy, "_smoothness",   nil)
    -- Hooks for future tasks (stroke/shadow logic not yet implemented)
    rawset(proxy, "_updateStroke", nil)
    rawset(proxy, "_updateShadow", nil)
    setmetatable(proxy, proxyMT)
    return proxy
end

-- ─────────────────────────────────────────────
-- Section 5: Factory Functions
-- ─────────────────────────────────────────────

-- Helper for simple radius-based shapes
local function newRadiusShape(x, y, radius, effectName, shapeType, sdfRadius)
    M.init()
    local size = radius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = effectName
    fill.fill.effect.radius     = sdfRadius
    fill.fill.effect.smoothness = defaultSmoothness(size)

    group.x, group.y = x, y

    local params = { width = size, height = size }
    return newProxy(group, fill, params, shapeType)
end

function M.newCircle(x, y, radius)
    M.init()
    local size = radius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = "filter.custom.sdf_circle"
    fill.fill.effect.radius     = 0.95
    fill.fill.effect.smoothness = defaultSmoothness(size)

    group.x, group.y = x, y

    local params = { width = size, height = size }
    return newProxy(group, fill, params, "circle")
end

function M.newEllipse(x, y, width, height)
    M.init()
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = "filter.custom.sdf_ellipse"
    fill.fill.effect.aspect     = width / height
    fill.fill.effect.radius     = 0.95
    fill.fill.effect.smoothness = defaultSmoothness(MIN(width, height))

    group.x, group.y = x, y

    local params = { width = width, height = height }
    return newProxy(group, fill, params, "ellipse")
end

function M.newRect(x, y, width, height)
    M.init()
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = "filter.custom.sdf_rect"
    fill.fill.effect.aspect     = width / height
    fill.fill.effect.smoothness = defaultSmoothness(MIN(width, height))

    group.x, group.y = x, y

    local params = { width = width, height = height }
    return newProxy(group, fill, params, "rect")
end

function M.newRoundedRect(x, y, width, height, cornerRadius)
    M.init()
    cornerRadius = cornerRadius or 10
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    -- Normalize cornerRadius to [0, 0.45] in SDF space
    local normalizedRadius = MIN(cornerRadius / (MIN(width, height) * 0.5), 0.45)

    fill.fill.effect = "filter.custom.sdf_rounded_rect"
    fill.fill.effect.aspect       = width / height
    fill.fill.effect.cornerRadius = normalizedRadius
    fill.fill.effect.smoothness   = defaultSmoothness(MIN(width, height))

    group.x, group.y = x, y

    local params = { width = width, height = height }
    return newProxy(group, fill, params, "roundedRect")
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

function M.newHeart(x, y, radius)
    return newRadiusShape(x, y, radius, "filter.custom.sdf_heart", "heart", 0.4)
end

function M.newDiamond(x, y, width, height)
    M.init()
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = "filter.custom.sdf_diamond"
    fill.fill.effect.aspect     = width / height
    fill.fill.effect.smoothness = defaultSmoothness(MIN(width, height))

    group.x, group.y = x, y

    local params = { width = width, height = height }
    return newProxy(group, fill, params, "diamond")
end

function M.newCross(x, y, size, thickness)
    M.init()
    thickness = thickness or 0.3
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = "filter.custom.sdf_cross"
    fill.fill.effect.thickness  = thickness
    fill.fill.effect.smoothness = defaultSmoothness(size)

    group.x, group.y = x, y

    local params = { width = size, height = size }
    return newProxy(group, fill, params, "cross")
end

function M.newCrescent(x, y, radius, offset)
    M.init()
    offset = offset or 0.3
    local size  = radius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = "filter.custom.sdf_crescent"
    fill.fill.effect.radius     = 0.9
    fill.fill.effect.offset     = offset
    fill.fill.effect.smoothness = defaultSmoothness(size)

    group.x, group.y = x, y

    local params = { width = size, height = size }
    return newProxy(group, fill, params, "crescent")
end

function M.newRing(x, y, outerRadius, innerRadius, startAngle, endAngle)
    M.init()
    startAngle = startAngle or 0
    endAngle   = endAngle   or 360
    innerRadius = innerRadius or (outerRadius * 0.5)

    local size  = outerRadius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    -- Normalize angles to [0, 2pi) on Lua side
    local startRad = RAD(startAngle) % TWO_PI
    local endRad   = RAD(endAngle)   % TWO_PI
    if endRad <= startRad and endAngle ~= startAngle then
        endRad = endRad + TWO_PI
    end

    fill.fill.effect = "filter.custom.sdf_ring"
    fill.fill.effect.innerRadius = innerRadius / outerRadius
    fill.fill.effect.outerRadius = 0.95
    fill.fill.effect.startAngle  = startRad
    fill.fill.effect.endAngle    = endRad

    group.x, group.y = x, y

    local params = { width = size, height = size }
    return newProxy(group, fill, params, "ring")
end

function M.newStar(x, y, radius, points, innerRadius)
    M.init()
    points      = points      or 5
    innerRadius = innerRadius or (radius * 0.4)

    -- Round innerRatio to 4 decimal places for cache key stability
    local innerRatio = math.floor((innerRadius / radius) * 10000 + 0.5) / 10000
    local effectName = getStarEffectName(points, innerRatio)

    local size  = radius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    fill.fill.effect = effectName
    fill.fill.effect.radius = 0.9

    group.x, group.y = x, y

    local params = { width = size, height = size }
    return newProxy(group, fill, params, "star")
end

function M.newPill(x, y, width, height)
    local proxy = M.newRoundedRect(x, y, width, height, height * 0.5)
    rawset(proxy, "_type", "pill")
    return proxy
end

-- ─────────────────────────────────────────────
-- Auto-initialize and export
-- ─────────────────────────────────────────────

M.init()

return M
