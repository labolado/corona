-- test_mesh_dynamic.lua: 动态 Mesh 更新回归测试
-- 手动三角化凹多边形（L形），通过 mesh.path:setVertex 每帧动态更新顶点，测试 UV 纹理映射
-- earcut.lua 已复制到 lib/，但因 plugin.bit 依赖未在 build.settings 声明，使用手动三角化作为主路径
-- 运行: SOLAR2D_TEST=mesh_dynamic SOLAR2D_BACKEND=bgfx

display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Mesh Dynamic Test (" .. backend .. ") ===")

local pass, fail = 0, 0
local function check(name, cond)
    if cond then
        pass = pass + 1; print("[PASS] " .. name)
    else
        fail = fail + 1; print("[FAIL] " .. name)
    end
end

-- 背景
display.newRect(W/2, H/2, W, H):setFillColor(0.08, 0.08, 0.15)

-- ===== L 形凹多边形（手动三角化） =====
-- 顶点（局部坐标，L 形）:
-- 1=(-50,-50), 2=(50,-50), 3=(50,0), 4=(0,0), 5=(0,50), 6=(-50,50)
--
-- 形状（逆时针视角）:
--   +----------+
--   |          |   <- top half (full width)
--   |    +-----+
--   |    |        <- lower-right part removed = concave
--   +----+
--
-- 三角化索引（4个三角形覆盖 L 形）:
--   (1,2,3), (1,3,4), (1,4,6), (4,5,6)

local BASE_VERTS = {
    -50, -50,  -- 1: top-left
     50, -50,  -- 2: top-right
     50,   0,  -- 3: mid-right
      0,   0,  -- 4: mid-inner corner
      0,  50,  -- 5: bottom-inner
    -50,  50,  -- 6: bottom-left
}

-- UV 映射：将顶点坐标归一化到 [0,1]
-- 包围盒 x:[-50,50], y:[-50,50]
local function uvFor(vx, vy)
    return (vx + 50) / 100, (vy + 50) / 100
end

local u1,v1 = uvFor(-50,-50)
local u2,v2 = uvFor( 50,-50)
local u3,v3 = uvFor( 50,  0)
local u4,v4 = uvFor(  0,  0)
local u5,v5 = uvFor(  0, 50)
local u6,v6 = uvFor(-50, 50)

-- 展开为 indexed mesh（6顶点，12个索引）
local vertices = {
    BASE_VERTS[1], BASE_VERTS[2],
    BASE_VERTS[3], BASE_VERTS[4],
    BASE_VERTS[5], BASE_VERTS[6],
    BASE_VERTS[7], BASE_VERTS[8],
    BASE_VERTS[9], BASE_VERTS[10],
    BASE_VERTS[11], BASE_VERTS[12],
}
local indices  = { 1,2,3, 1,3,4, 1,4,6, 4,5,6 }
local uvs      = { u1,v1, u2,v2, u3,v3, u4,v4, u5,v5, u6,v6 }

-- 创建 Mesh A（纹理填充，居中）
local meshA
local okA, errA = pcall(function()
    meshA = display.newMesh{
        x = W * 0.35, y = H * 0.42,
        mode = "indexed",
        vertices = vertices,
        indices  = indices,
        uvs      = uvs,
    }
    meshA.fill = { type = "image", filename = "t1.jpg" }
end)
check("mesh A created (textured L)", okA and meshA ~= nil)
if errA then print("  err: " .. tostring(errA)) end

-- 创建 Mesh B（纯色填充，右侧，用于对比动态顶点更新）
local meshB
local okB = pcall(function()
    meshB = display.newMesh{
        x = W * 0.72, y = H * 0.42,
        mode = "indexed",
        vertices = {table.unpack(vertices)},
        indices  = {table.unpack(indices)},
        uvs      = {table.unpack(uvs)},
    }
    meshB:setFillColor(0.3, 0.8, 0.4)
end)
check("mesh B created (solid L)", okB and meshB ~= nil)

-- 验证 path 对象存在（用于 setVertex）
if meshA then
    check("mesh.path exists", meshA.path ~= nil)
end

-- ===== 动态顶点更新测试 =====
-- 每帧更新 vertex 2（top-right corner）y 坐标，制造波动效果
local t0 = system.getTimer()
local initialY2 = BASE_VERTS[4]   -- vertex 2 的 y = -50

local frameCount = 0
local vertexUpdateOk = true

Runtime:addEventListener("enterFrame", function()
    local elapsed = (system.getTimer() - t0) / 1000
    local dy = math.sin(elapsed * 3) * 20  -- ±20px 振幅

    frameCount = frameCount + 1

    -- 更新 meshA vertex 2 的 y（top-right 上下波动）
    if meshA and meshA.path then
        local ok = pcall(function()
            meshA.path:setVertex(2, 50, initialY2 + dy)
        end)
        if not ok then vertexUpdateOk = false end
    end

    -- 同步更新 meshB vertex 2
    if meshB and meshB.path then
        pcall(function()
            meshB.path:setVertex(2, 50, initialY2 + dy * 1.5)
        end)
    end
end)

-- ===== 标签 =====
local function label(txt, x, y)
    local t = display.newText(txt, x, y, native.systemFont, 12)
    t:setFillColor(1, 1, 0.7)
    return t
end
label("Textured L-mesh (dynamic)", W * 0.35, H * 0.42 + 75)
label("Solid L-mesh (animated)",   W * 0.72, H * 0.42 + 75)
label("Vertex 2 (top-right) y animates ±20px", W/2, H * 0.75)

local title = display.newText("Mesh Dynamic Test - " .. backend, W/2, 22, native.systemFontBold, 15)
title:setFillColor(1, 1, 1)

-- ===== 延迟验证 =====
timer.performWithDelay(600, function()
    check("vertex update no crash", vertexUpdateOk)
    check("frames rendered > 10", frameCount > 10)

    -- 验证 meshA vertex 2 y 已变化（不再是初始值）
    if meshA and meshA.path then
        local ok2, x2, y2 = pcall(function()
            return meshA.path:getVertex(2)
        end)
        if ok2 and y2 ~= nil then
            check("vertex 2 y changed from initial", math.abs(y2 - initialY2) > 0.5)
        else
            -- getVertex 不支持时，只验证 setVertex 不崩溃
            check("vertex 2 accessible (setVertex OK)", vertexUpdateOk)
        end
    end

    print(string.format("\n=== MESH DYNAMIC TEST RESULTS (%s): Pass %d | Fail %d ===", backend, pass, fail))
    if fail == 0 then
        print("TEST PASS: mesh_dynamic")
    else
        print("TEST FAIL: mesh_dynamic")
    end
end)
