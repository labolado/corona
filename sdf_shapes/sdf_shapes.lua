------------------------------------------------------------------------------
-- SDF Shapes Library for Solar2D
-- Anti-aliased vector shapes via GLSL signed distance fields
-- Version: 2.0.0
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
        return display.newImageRect("white_pixel.png", width, height)
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float dist = length(p);
            P_UV float alpha = 1.0 - smoothstep(
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_UserData0;
            P_UV float dist = length(p);
            P_UV float alpha = 1.0 - smoothstep(
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float aspect = u_UserData0;
            p.x *= aspect;
            P_UV vec2 d = abs(p) - vec2(aspect, 1.0);
            P_UV float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
            P_UV float smooth = u_UserData1 / max(aspect, 1.0);
            P_UV float alpha = 1.0 - smoothstep(-smooth, smooth, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float aspect = u_UserData0;
            P_UV float radius = u_UserData1;
            p.x *= aspect;
            P_UV vec2 b = vec2(aspect, 1.0) - vec2(radius * 2.0);
            P_UV vec2 q = abs(p) - b;
            P_UV float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
            P_UV float alpha = 1.0 - smoothstep(-u_UserData2, u_UserData2, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            p = abs(p);
            P_UV float dist = max(dot(vec2(1.73205, 1.0), p) / 2.0, p.x) - u_UserData0;
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float r = u_UserData0;
            p.y = -p.y - 0.15;
            P_UV float a = mod(atan(p.x, p.y) + PI, TWO_PI) / 5.0;
            P_UV vec2 q = length(p) * vec2(cos(a), sin(a));
            P_UV float dist = max(q.x - 0.809016994 * r, abs(q.y) - 0.587785252 * r);
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
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
            P_UV vec2 p = abs((uv - 0.5) * 2.0);
            P_UV float r = u_UserData0;
            P_UV float dist = dot(p, vec2(1.0, 0.414213562)) - r * 1.414213562;
            dist = max(dist, p.x - r);
            dist = max(dist, p.y - r);
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            p.y += 0.35;
            P_UV float dist = max(abs(p.x) * 1.73205 + p.y, -p.y) - u_UserData0;
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            p.x *= u_UserData0;
            P_UV float dist = abs(p.x) + abs(p.y) - 1.0;
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float t = u_UserData0;
            P_UV float h1 = length(vec2(max(abs(p.x) - 1.0, 0.0), max(abs(p.y) - t, 0.0)));
            P_UV float h2 = length(vec2(max(abs(p.x) - t, 0.0), max(abs(p.y) - 1.0, 0.0)));
            P_UV float dist = min(h1, h2);
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Heart
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            p.y = -p.y + 0.25;
            P_UV float px = abs(p.x);
            P_UV float py = p.y;
            P_UV float dist = sqrt(max(px * 0.5, 0.0)) + py - u_UserData0;
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float d1 = length(p) - u_UserData0;
            P_UV float d2 = length(p - vec2(u_UserData1, 0.0)) - u_UserData0;
            P_UV float dist = max(d1, -d2);
            P_UV float alpha = 1.0 - smoothstep(-u_UserData2, u_UserData2, dist);
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
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float r = length(p);

            // Radial mask
            P_UV float radialDist = max(u_UserData0 - r, r - u_UserData1);

            // Angle in [0, 2pi)
            P_UV float angle = atan(p.y, p.x);
            angle += step(angle, 0.0) * TWO_PI;

            P_UV float start = u_UserData2;
            P_UV float end   = u_UserData3;
            P_UV float range = (end - start) * 0.5;

            // Full-circle guard: skip angular clipping when range >= PI
            P_UV float isFullCircle = step(PI - 0.001, range);

            P_UV float mid = start + range;
            P_UV float angleDiff = abs(angle - mid);
            angleDiff = min(angleDiff, TWO_PI - angleDiff);
            P_UV float angleDist = max(0.0, angleDiff - range);
            angleDist *= (1.0 - isFullCircle);

            P_UV float dist = max(radialDist, angleDist * r);
            P_UV float alpha = 1.0 - smoothstep(-0.01, 0.01, dist);
            return CoronaColorScale(vec4(alpha, alpha, alpha, alpha));
        }
    ]],
}

-- Composite shaders for gradient masking and boolean operations
local compositeShaders = {}

compositeShaders.gradientMask = {
    category = "composite",
    name     = "sdf_gradient_mask",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            P_COLOR vec4 gradient = texture2D(CoronaSampler0, uv);
            P_COLOR vec4 mask = texture2D(CoronaSampler1, uv);
            return vec4(gradient.rgb * mask.a, mask.a);
        }
    ]],
}

compositeShaders.boolIntersect = {
    category = "composite",
    name     = "sdf_bool_intersect",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            P_COLOR vec4 c1 = texture2D(CoronaSampler0, uv);
            P_COLOR vec4 c2 = texture2D(CoronaSampler1, uv);
            return vec4(c1.rgb, c1.a * c2.a);
        }
    ]],
}

compositeShaders.boolSubtract = {
    category = "composite",
    name     = "sdf_bool_subtract",
    fragment = [[
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            P_COLOR vec4 c1 = texture2D(CoronaSampler0, uv);
            P_COLOR vec4 c2 = texture2D(CoronaSampler1, uv);
            return vec4(c1.rgb, c1.a * (1.0 - c2.a));
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
        uniform P_DEFAULT float u_UserData1; // smoothness

        P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
            P_UV vec2 p = (uv - 0.5) * 2.0;
            P_UV float r = u_UserData0;
            const vec2 acs = %s;
            const vec2 ecs = %s;
            P_UV float bn = mod(atan(p.x, p.y), %s) - %s;
            p = length(p) * vec2(cos(bn), abs(sin(bn)));
            p -= r * acs;
            p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);
            P_UV float dist = length(p) * sign(p.x);
            P_UV float alpha = 1.0 - smoothstep(-u_UserData1, u_UserData1, dist);
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
                { name = "radius",     type = "scalar", index = 0, default = 0.9  },
                { name = "smoothness", type = "scalar", index = 1, default = 0.01 },
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

    for key, shader in pairs(compositeShaders) do
        local ok, err = pcall(graphics.defineEffect, shader)
        if not ok then
            print("SDF Shapes: FAILED to register composite " .. key .. ": " .. tostring(err))
        else
            _registeredShaders["composite_" .. key] = true
        end
    end

    _initialized = true
end

-- Apply SDF shader uniforms to an object based on params
local function applyShaderUniforms(obj, params, overrideAspect, overrideSmoothness)
    local eff = obj.fill.effect
    if not eff then return end

    local aspect     = overrideAspect     or params.aspect
    local smoothness = overrideSmoothness or params.smoothness

    if params.sdfRadius    ~= nil then eff.radius       = params.sdfRadius   end
    if aspect              ~= nil then eff.aspect        = aspect             end
    if smoothness          ~= nil then eff.smoothness    = smoothness         end
    if params.cornerRadius ~= nil then eff.cornerRadius = params.cornerRadius end
    if params.thickness    ~= nil then eff.thickness     = params.thickness   end
    if params.offset       ~= nil then eff.offset        = params.offset      end
    -- Ring-specific uniforms
    if params.innerRadius  ~= nil then eff.innerRadius   = params.innerRadius end
    if params.outerRadius  ~= nil then eff.outerRadius   = params.outerRadius end
    if params.startAngle   ~= nil then eff.startAngle    = params.startAngle  end
    if params.endAngle     ~= nil then eff.endAngle      = params.endAngle    end
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
            rawset(self, "_group",        nil)
            rawset(self, "_fill",         nil)
            rawset(self, "_stroke",       nil)
            rawset(self, "_shadow",       nil)
            rawset(self, "_shadowObj",    nil)
            rawset(self, "_gradientSnap", nil)
            rawset(self, "_params",       nil)
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

    -- setFillGradient: applies a gradient fill masked by the SDF shape
    -- Uses a snapshot with gradient background + SDF mask via multiply blend.
    -- The SDF shader outputs vec4(a,a,a,a) * CoronaColorScale; with white fill
    -- and multiply blend this becomes: gradient.rgb * alpha, gradient.a * alpha.
    if k == "setFillGradient" then
        return function(self, config)
            -- Remove existing gradient
            local oldSnap = rawget(self, "_gradientSnap")
            if oldSnap then
                oldSnap:removeSelf()
                rawset(self, "_gradientSnap", nil)
            end

            if config == nil then
                -- Restore original fill
                local fill = rawget(self, "_fill")
                if fill then fill.isVisible = true end
                return
            end

            local p     = rawget(self, "_params")
            local group = rawget(self, "_group")
            local w, h  = p.width, p.height

            -- Create snapshot containing gradient + SDF mask
            local snap = display.newSnapshot(w, h)

            -- Gradient background
            local gradRect = display.newRect(0, 0, w, h)
            gradRect.fill = {
                type      = "gradient",
                color1    = config.color1    or {1, 0, 0},
                color2    = config.color2    or {0, 0, 1},
                direction = config.direction or "down",
            }
            snap.group:insert(gradRect)

            -- SDF shape as alpha mask (multiply blend against gradient)
            local maskObj = createObject(w, h)
            maskObj.fill.effect = p.effectName
            applyShaderUniforms(maskObj, p)
            maskObj:setFillColor(1, 1, 1)
            maskObj.blendMode = "multiply"
            snap.group:insert(maskObj)

            snap:invalidate()

            -- Hide original fill
            local fill = rawget(self, "_fill")
            if fill then fill.isVisible = false end

            -- Insert gradient snap in group (after shadow if present)
            local insertIdx = 1
            if rawget(self, "_shadowObj") then insertIdx = insertIdx + 1 end
            group:insert(insertIdx, snap)
            rawset(self, "_gradientSnap", snap)
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

-- ─────────────────────────────────────────────
-- Section 4b: Stroke & Shadow helpers
-- ─────────────────────────────────────────────

local function updateStroke(self)
    local params = rawget(self, "_params")
    local group  = rawget(self, "_group")
    local sw     = rawget(self, "_strokeWidth") or 0

    -- Ring stroke is not meaningful; skip silently
    if params.shapeType == "ring" then return end

    if sw > 0 then
        local nw = params.width  + sw * 2
        local nh = params.height + sw * 2

        local stroke = rawget(self, "_stroke")
        if not stroke then
            stroke = createObject(nw, nh)
            stroke.x, stroke.y = 0, 0
            -- Insert at position 1 (or 2 if shadow exists, but shadow is always at 1)
            local shadowObj = rawget(self, "_shadowObj")
            if shadowObj then
                group:insert(2, stroke)
            else
                group:insert(1, stroke)
            end
            rawset(self, "_stroke", stroke)
        else
            stroke.width  = nw
            stroke.height = nh
        end

        stroke.fill.effect = params.effectName

        -- Recalculate aspect for the new dimensions
        local overrideAspect = nil
        if params.aspect ~= nil then
            overrideAspect = nw / nh
        end

        -- Use same smoothness as fill (based on stroke object size)
        local overrideSmoothness = nil
        if params.smoothness ~= nil then
            overrideSmoothness = defaultSmoothness(MIN(nw, nh))
        end

        applyShaderUniforms(stroke, params, overrideAspect, overrideSmoothness)

        local sc = rawget(self, "_strokeColor")
        if sc then
            stroke:setFillColor(unpack(sc))
        else
            stroke:setFillColor(0, 0, 0, 1)
        end

        stroke.isVisible = true
    else
        local stroke = rawget(self, "_stroke")
        if stroke then
            stroke.isVisible = false
        end
    end
end

-- Shadow uses SDF smoothstep falloff, not true Gaussian blur.
-- For non-convex shapes (star, heart, crescent, cross) the shadow follows
-- SDF contours rather than spreading uniformly at concave regions.
-- Use Solar2D snapshot + blur filter for pixel-perfect shadows on those shapes.
local function updateShadow(self)
    local params = rawget(self, "_params")
    local group  = rawget(self, "_group")
    local config = rawget(self, "_shadow")

    local existingShadow = rawget(self, "_shadowObj")

    if config == nil then
        if existingShadow then
            existingShadow:removeSelf()
            rawset(self, "_shadowObj", nil)
        end
        return
    end

    local blur    = config.blur    or 8
    local offsetX = config.offsetX or 4
    local offsetY = config.offsetY or 4
    local color   = config.color   or {0, 0, 0, 0.3}

    local w = params.width  + blur * 2
    local h = params.height + blur * 2

    local shadowObj = existingShadow
    if not shadowObj then
        shadowObj = createObject(w, h)
        shadowObj.x, shadowObj.y = offsetX, offsetY
        group:insert(1, shadowObj)
        rawset(self, "_shadowObj", shadowObj)
    else
        shadowObj.width  = w
        shadowObj.height = h
        shadowObj.x = offsetX
        shadowObj.y = offsetY
    end

    shadowObj.fill.effect = params.effectName

    -- Recalculate aspect for shadow dimensions
    local overrideAspect = nil
    if params.aspect ~= nil then
        overrideAspect = w / h
    end

    -- Shadow blur: larger smoothness = softer edges
    local overrideSmoothness = nil
    if params.shapeType ~= "ring" then
        overrideSmoothness = blur / (MAX(w, h) * 0.5)
    end

    applyShaderUniforms(shadowObj, params, overrideAspect, overrideSmoothness)

    shadowObj:setFillColor(unpack(color))
end

local function newProxy(group, fill, params, shapeType)
    local proxy = {}
    rawset(proxy, "_group",        group)
    rawset(proxy, "_fill",         fill)
    rawset(proxy, "_stroke",       nil)
    rawset(proxy, "_shadow",       nil)
    rawset(proxy, "_shadowObj",    nil)
    rawset(proxy, "_params",       params)
    rawset(proxy, "_type",         shapeType)
    rawset(proxy, "_strokeWidth",  0)
    rawset(proxy, "_strokeColor",  nil)
    rawset(proxy, "_smoothness",   nil)
    rawset(proxy, "_updateStroke", updateStroke)
    rawset(proxy, "_updateShadow", updateShadow)
    setmetatable(proxy, proxyMT)
    return proxy
end

-- ─────────────────────────────────────────────
-- Section 5: Factory Functions
-- ─────────────────────────────────────────────

-- Helper for simple radius-based shapes
local function newRadiusShape(x, y, radius, effectName, shapeType, sdfRadius)
    local size = radius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    local sm = defaultSmoothness(size)
    fill.fill.effect = effectName
    fill.fill.effect.radius     = sdfRadius
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = size,
        height     = size,
        effectName = effectName,
        sdfRadius  = sdfRadius,
        smoothness = sm,
        shapeType  = shapeType,
    }
    return newProxy(group, fill, params, shapeType)
end

function M.newCircle(x, y, radius)
    local size = radius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    local sm = defaultSmoothness(size)
    fill.fill.effect = "filter.custom.sdf_circle"
    fill.fill.effect.radius     = 0.95
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = size,
        height     = size,
        effectName = "filter.custom.sdf_circle",
        sdfRadius  = 0.95,
        smoothness = sm,
        shapeType  = "circle",
    }
    return newProxy(group, fill, params, "circle")
end

function M.newEllipse(x, y, width, height)
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    local sm = defaultSmoothness(MIN(width, height))
    fill.fill.effect = "filter.custom.sdf_ellipse"
    fill.fill.effect.aspect     = width / height
    fill.fill.effect.radius     = 0.95
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = width,
        height     = height,
        effectName = "filter.custom.sdf_ellipse",
        sdfRadius  = 0.95,
        aspect     = width / height,
        smoothness = sm,
        shapeType  = "ellipse",
    }
    return newProxy(group, fill, params, "ellipse")
end

function M.newRect(x, y, width, height)
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    local sm = defaultSmoothness(MIN(width, height))
    fill.fill.effect = "filter.custom.sdf_rect"
    fill.fill.effect.aspect     = width / height
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = width,
        height     = height,
        effectName = "filter.custom.sdf_rect",
        aspect     = width / height,
        smoothness = sm,
        shapeType  = "rect",
    }
    return newProxy(group, fill, params, "rect")
end

function M.newRoundedRect(x, y, width, height, cornerRadius)
    cornerRadius = cornerRadius or 10
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    -- Normalize cornerRadius to [0, 0.45] in SDF space
    local normalizedRadius = MIN(cornerRadius / (MIN(width, height) * 0.5), 0.45)
    local sm = defaultSmoothness(MIN(width, height))

    fill.fill.effect = "filter.custom.sdf_rounded_rect"
    fill.fill.effect.aspect       = width / height
    fill.fill.effect.cornerRadius = normalizedRadius
    fill.fill.effect.smoothness   = sm

    group.x, group.y = x, y

    local params = {
        width        = width,
        height       = height,
        effectName   = "filter.custom.sdf_rounded_rect",
        aspect       = width / height,
        cornerRadius = normalizedRadius,
        smoothness   = sm,
        shapeType    = "roundedRect",
    }
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
    local group = display.newGroup()
    local fill  = createObject(width, height)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    local sm = defaultSmoothness(MIN(width, height))
    fill.fill.effect = "filter.custom.sdf_diamond"
    fill.fill.effect.aspect     = width / height
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = width,
        height     = height,
        effectName = "filter.custom.sdf_diamond",
        aspect     = width / height,
        smoothness = sm,
        shapeType  = "diamond",
    }
    return newProxy(group, fill, params, "diamond")
end

function M.newCross(x, y, size, thickness)
    thickness = thickness or 0.3
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    local sm = defaultSmoothness(size)
    fill.fill.effect = "filter.custom.sdf_cross"
    fill.fill.effect.thickness  = thickness
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = size,
        height     = size,
        effectName = "filter.custom.sdf_cross",
        thickness  = thickness,
        smoothness = sm,
        shapeType  = "cross",
    }
    return newProxy(group, fill, params, "cross")
end

function M.newCrescent(x, y, radius, offset)
    offset = offset or 0.3
    local size  = radius * 2
    local group = display.newGroup()
    local fill  = createObject(size, size)
    group:insert(fill)
    fill.x, fill.y = 0, 0

    local sm = defaultSmoothness(size)
    fill.fill.effect = "filter.custom.sdf_crescent"
    fill.fill.effect.radius     = 0.9
    fill.fill.effect.offset     = offset
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = size,
        height     = size,
        effectName = "filter.custom.sdf_crescent",
        sdfRadius  = 0.9,
        offset     = offset,
        smoothness = sm,
        shapeType  = "crescent",
    }
    return newProxy(group, fill, params, "crescent")
end

function M.newRing(x, y, outerRadius, innerRadius, startAngle, endAngle)
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

    local normInner = innerRadius / outerRadius
    fill.fill.effect = "filter.custom.sdf_ring"
    fill.fill.effect.innerRadius = normInner
    fill.fill.effect.outerRadius = 0.95
    fill.fill.effect.startAngle  = startRad
    fill.fill.effect.endAngle    = endRad

    group.x, group.y = x, y

    local params = {
        width       = size,
        height      = size,
        effectName  = "filter.custom.sdf_ring",
        innerRadius = normInner,
        outerRadius = 0.95,
        startAngle  = startRad,
        endAngle    = endRad,
        shapeType   = "ring",
    }
    return newProxy(group, fill, params, "ring")
end

function M.newStar(x, y, radius, points, innerRadius)
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

    local sm = defaultSmoothness(radius)
    fill.fill.effect = effectName
    fill.fill.effect.radius     = 0.9
    fill.fill.effect.smoothness = sm

    group.x, group.y = x, y

    local params = {
        width      = size,
        height     = size,
        effectName = effectName,
        sdfRadius  = 0.9,
        smoothness = sm,
        shapeType  = "star",
    }
    return newProxy(group, fill, params, "star")
end

function M.newPill(x, y, width, height)
    local proxy = M.newRoundedRect(x, y, width, height, height * 0.5)
    rawset(proxy, "_type", "pill")
    return proxy
end

-- ─────────────────────────────────────────────
-- Section 6: Boolean Operations
-- ─────────────────────────────────────────────

local GROUP_PROPS = {
    x = true, y = true, alpha = true, isVisible = true,
    rotation = true, xScale = true, yScale = true,
    anchorX = true, anchorY = true,
}

local boolProxyMT = {}

boolProxyMT.__index = function(self, key)
    if key == "removeSelf" then
        return function(obj)
            local snap = rawget(obj, "_snapshot")
            if snap then snap:removeSelf() end
            local snaps = rawget(obj, "_snapshots")
            if snaps then
                for _, s in ipairs(snaps) do
                    if s and s.removeSelf then s:removeSelf() end
                end
            end
            rawset(obj, "_snapshot", nil)
            rawset(obj, "_snapshots", nil)
            rawset(obj, "_operands", nil)
        end
    end
    -- Forward group props to snapshot
    local snap = rawget(self, "_snapshot")
    if snap and GROUP_PROPS[key] then
        return snap[key]
    end
    return rawget(self, key)
end

boolProxyMT.__newindex = function(self, key, value)
    local snap = rawget(self, "_snapshot")
    if snap and GROUP_PROPS[key] then
        snap[key] = value
        return
    end
    rawset(self, key, value)
end

-- Union: combines two shapes by placing them both in one snapshot
function M.union(a, b)
    local aw, ah = a._params.width, a._params.height
    local bw, bh = b._params.width, b._params.height
    local w = MAX(aw, bw) + 40
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

-- Intersect: keeps only the overlapping region of two shapes.
-- Uses multiply blend on the second shape so only pixels where both
-- shapes have alpha will remain visible.
function M.intersect(a, b)
    local aw, ah = a._params.width, a._params.height
    local bw, bh = b._params.width, b._params.height
    local w = MAX(aw, bw) + 40
    local h = MAX(ah, bh) + 40

    local snap = display.newSnapshot(w, h)
    snap.group:insert(a._group)
    -- Multiply blend: output = src * dst; keeps color from a, masked by b's alpha
    b._group.blendMode = "multiply"
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

-- Subtract: removes shape b from shape a.
-- Uses custom blend equation: output = dst * (1 - src.alpha), erasing
-- pixels from a wherever b is opaque.
function M.subtract(a, b)
    local aw, ah = a._params.width, a._params.height
    local bw, bh = b._params.width, b._params.height
    local w = MAX(aw, bw) + 40
    local h = MAX(ah, bh) + 40

    local snap = display.newSnapshot(w, h)
    snap.group:insert(a._group)

    -- Custom blend: zero out src contribution, keep dst * (1 - srcAlpha)
    local bFill = rawget(b, "_fill")
    if bFill then bFill:setFillColor(1, 1, 1) end
    b._group.blendMode = {
        srcColor = "zero",
        srcAlpha = "zero",
        dstColor = "oneMinusSrcAlpha",
        dstAlpha = "oneMinusSrcAlpha",
    }
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

-- ─────────────────────────────────────────────
-- Auto-initialize and export
-- ─────────────────────────────────────────────

M.init()

return M
