-- test_outline4.lua: 复现 tank 教程 outline，大图看轮廓线
display.setDefault("background", 0.92, 0.92, 0.88)

-- 固定橙色轮廓（不用时间变化，方便截图对比）
graphics.defineEffect{
    language = "glsl", category = "filter", name = "colored_outline",
    vertex = [[
        varying P_COLOR vec4 outlineColor;
        P_POSITION vec2 VertexKernel(P_POSITION vec2 position)
        {
            outlineColor = vec4(1.0, 0.35, 0.0, 1.0);
            return position;
        }
    ]],
    fragment = [[
        varying P_COLOR vec4 outlineColor;
        P_COLOR vec4 FragmentKernel(P_UV vec2 uv)
        {
            P_COLOR vec4 col = texture2D(CoronaSampler0, fract(uv));
            return CoronaColorScale(outlineColor * col.a);
        }
    ]],
}

local function _getValue(offset, key)
    local value = offset[key]
    if value == nil or value <= 0 then value = 2 end
    return value
end

local function _createMesh(filename, w, h, options, initScale)
    local offset = options.offset
    local l = _getValue(offset, "left") * initScale
    local r = _getValue(offset, "right") * initScale
    local u = _getValue(offset, "up") * initScale
    local d = _getValue(offset, "down") * initScale
    local verts = {
        l, u,  w-r, u,  l, h-d,  w-r, h-d,
        0, 0,  w, 0,  0, u,  w, u,
        0, h-d,  w, h-d,  0, h,  w, h,
        l, 0,  w-r, 0,  l, h,  w-r, h,
    }
    local tris = {
        1,2,3, 3,2,4, 5,13,7, 7,13,1, 2,14,6, 2,6,8,
        7,1,9, 9,1,3, 2,8,4, 4,8,10,
        9,3,11, 11,3,15, 4,10,16, 16,10,12,
        13,14,1, 1,14,2, 3,4,15, 15,4,16,
    }
    local uvs = {}
    for i = 1, #verts, 2 do
        uvs[i] = verts[i] / w
        uvs[i+1] = verts[i+1] / h
        verts[i] = verts[i] - w * 0.5
        verts[i+1] = verts[i+1] - h * 0.5
    end
    local mesh = display.newMesh{
        mode = "indexed",
        vertices = verts,
        indices = tris,
        uvs = uvs,
    }
    mesh.fill = { type = "image", filename = filename }
    return mesh
end

local cx = display.contentCenterX
local cy = display.contentCenterY
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"

local imgFile = "tank_shape-1.png"
local initW, initH = 1768, 510
local scale = 1.0  -- 原始大小 1:1
local w = initW * scale
local h = initH * scale
local outlineWidth = 16
local options = { offset = { left = 4, right = 4, up = 4, down = 4 } }

-- 橙色 outline mesh（稍大，放后面）
local outW = w + outlineWidth
local outH = h + outlineWidth
local outline = _createMesh(imgFile, outW, outH, options, outW / initW)
outline:translate(cx, cy)
outline.fill.effect = "filter.custom.colored_outline"

-- 原件盖上面（只露出边缘的橙色轮廓线）
local orig = _createMesh(imgFile, w, h, options, scale)
orig:translate(cx, cy)

display.newText(backend, cx, 60, native.systemFont, 50):setFillColor(0.8, 0, 0)
