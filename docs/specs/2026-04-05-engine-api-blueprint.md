# Solar2D 新一代引擎 API 蓝图

> 版本：v0.2
> 日期：2026-04-05
> 原则：**兼容现有 API** + 新增能力

## 设计原则

1. **向后兼容** — 现有 Lua 代码零修改即可运行
2. **极简 API** — 一行代码做复杂的事，渐进复杂度
3. **性能透明** — 引擎自动优化（atlas/batch/cache），开发者不用懂 GPU
4. **AI 友好** — 构造函数支持属性表，函数式无状态设计，EmmyLua 类型定义
5. **热重载** — 代码/shader/资源修改实时生效
6. **跨平台零配置** — Metal/Vulkan/WebGPU 自动适配

## 架构分层

```
┌──────────────────────────────────────────────┐
│  Lua API 层（开发者接触的）                     │
│  display.* / graphics.* / gpu.*              │
├──────────────────────────────────────────────┤
│  自动优化层（开发者无感）                        │
│  Runtime Atlas Cache / Auto Batch            │
│  Static Geometry Cache / Transient Buffer    │
├──────────────────────────────────────────────┤
│  bgfx 渲染抽象层                               │
│  Renderer / CommandBuffer / Program          │
├──────────────────────────────────────────────┤
│  bgfx 后端 → Metal / Vulkan / WebGPU         │
└──────────────────────────────────────────────┘
```

---

## 模块 1：Texture Atlas（graphics.newAtlas）

### 极简用法
```lua
local atlas = graphics.newAtlas({ "hero.png", "enemy.png", "bullet.png" })

-- 用 atlas 创建图片（兼容现有 display.newImage 签名）
local hero = display.newImage(atlas, "hero.png")
local enemy = display.newImage(atlas, "enemy.png", 100, 200)
local bullet = display.newImage(atlas, "bullet.png", 50, 300, {
    scaleX = 2, rotation = 45
})
-- 内部：同一 texture，减少 draw call
```

### 签名说明
```lua
-- 现有签名（完全保留）
display.newImage(filename)
display.newImage(filename, x, y)
display.newImage(parent, filename, x, y)
display.newImage(imageSheet, frameIndex)

-- 新增签名（通过第二参数类型 string vs number 区分）
display.newImage(atlas, name)
display.newImage(atlas, name, x, y)
display.newImage(atlas, name, x, y, opts)
```

### 高级用法
```lua
local atlas = graphics.newAtlas({
    images = { "hero.png", "enemy.png", "tiles/*.png" },
    maxSize = 2048,
    padding = 1,
    trimWhitespace = true,
})
```

### Sprite Sheet 兼容
```lua
local sheet = atlas:getImageSheet("hero.png", {
    width = 32, height = 32, numFrames = 8
})
local sprite = display.newSprite(sheet, sequenceData)
```

### 内部行为
- **Runtime 打包 + 持久化缓存**：首次运行打包 atlas → 缓存到 `system.CachesDirectory`
- 后续启动检查源文件**内容 hash**（SHA256），未变化直接加载缓存
- 任一源文件变化 → 整个 atlas 重新打包（保证 UV 一致性）
- 缓存格式：二进制（texture 数据 + 帧元数据 + 版本号）
- 引擎升级导致版本号不匹配 → 自动重新打包
- 热重载：源文件变化 → 自动重新打包
- 同一 atlas 的对象共享 texture，引擎自动减少状态切换
- 异步加载：大 atlas 在后台线程打包，完成后回调（不阻塞主线程）

```lua
-- 异步加载大型 atlas
graphics.newAtlas({
    images = { "sprites/*.png" },  -- 可能有数百张
    async = true,
    onComplete = function(atlas)
        -- atlas 准备好了
    end,
})
```

### 方法
| 方法 | 说明 |
|------|------|
| `atlas:getImageSheet(name, opts)` | 从 atlas 创建 ImageSheet（与 `graphics.newImageSheet` 返回兼容类型） |
| `atlas:getFrame(name)` | 获取单帧信息 {x, y, w, h, u0, v0, u1, v1} |
| `atlas:has(name)` | 检查 atlas 是否包含指定图片 |
| `atlas:list()` | 列出所有图片名 |
| `atlas:reload()` | 强制重新打包（热重载用） |
| `atlas:removeSelf()` | 释放 atlas 和关联的 GPU 纹理 |

### 错误处理
```lua
local atlas, err = graphics.newAtlas({ "missing.png" })
if not atlas then
    print("Atlas error: " .. err)  -- "File not found: missing.png"
end
```

---

## 模块 2：Batch Rendering（display.newBatch）

### 基本用法
```lua
-- 同一 atlas 的多个 sprite 合并为 1 个 draw call
local atlas = graphics.newAtlas({ "hero.png", "enemy.png", "bullet.png" })
local batch = display.newBatch(atlas, 100)  -- 预分配 100 个 slot

batch:add("hero.png", 100, 200)           -- x, y
batch:add("enemy.png", 300, 200, { rotation = 45 })
batch:add("bullet.png", 150, 300, { scaleX = 2 })

-- batch 是 display object，支持标准操作
batch.x = 50
batch.alpha = 0.8
transition.to(batch, { alpha = 0, time = 1000 })
```

### 动态更新
```lua
-- 获取 slot 引用
local heroSlot = batch:add("hero.png", 100, 200)

-- 更新位置（不创建新 draw call）
heroSlot.x = 150
heroSlot.y = 250
heroSlot.rotation = 90

-- 移除
heroSlot:remove()
```

### Batch 溢出处理
```lua
local batch = display.newBatch(atlas, 100)

-- 超过 100 个 slot 时自动扩容（翻倍），不报错
for i = 1, 200 do
    batch:add("hero.png", math.random(0, 320), math.random(0, 480))
end
print(batch:count())  -- 200
```

### 销毁
```lua
batch:removeSelf()  -- 释放所有 slot 和 GPU 资源
batch = nil
```

### 内部行为
- batch 内所有 sprite 共享同一 texture + shader → 1 个 draw call
- 顶点数据用 transient buffer，每帧只上传变化的 slot
- 保持 painter's algorithm：batch 内按添加顺序绘制
- batch 是 DisplayObject，支持 :removeSelf()、transition、事件监听

---

## 模块 3：Particle System（display.newParticles）

### 基本用法
```lua
local emitter = display.newParticles({
    image = "spark.png",        -- 单图
    -- 或 atlas: image = atlas:getFrame("spark.png")
    maxParticles = 1000,
    emitRate = 50,              -- 每秒
    lifetime = { 0.5, 2.0 },   -- 随机范围
    speed = { 50, 200 },
    angle = { 0, 360 },
    gravity = { 0, 300 },
    colors = {
        { 1, 1, 0.3, 1 },      -- 出生色
        { 1, 0.3, 0, 0 },      -- 消亡色（alpha=0 淡出）
    },
    size = { 8, 2 },            -- 从 8 缩到 2
})
emitter.x, emitter.y = 160, 240
```

### 高级：GPU 粒子（未来）
```lua
local emitter = display.newParticles({
    image = "spark.png",
    maxParticles = 100000,      -- 10 万个！
    mode = "gpu",               -- GPU compute 驱动
    computeShader = "particle_physics.effect",
})
```

### 内部行为
- CPU 模式：Lua + instancing，1 个 draw call 画所有粒子
- GPU 模式（未来）：compute shader 更新位置，完全不经过 CPU
- 兼容现有 `display.newEmitter`（保留旧 API）

---

## 模块 4：Effect System（graphics.loadEffect）

### 声明式效果（零 shader 知识）
```lua
-- 单效果（现有语法，完全保留）
rect.fill.effect = "filter.blur"
rect.fill.effect.radius = 5

-- 效果链（新增能力）
-- fill.effectChain 明确区分于 fill.effect（单效果）
rect.fill.effectChain = {
    { name = "filter.grayscale" },
    { name = "filter.blur", params = { radius = 3 } },
    { name = "filter.glow", params = { intensity = 0.5 } },
}
```

### .effect 文件（高级用户）
```yaml
# effects/custom_glow.effect
name: "filter.custom.glow"

uniforms:
  glowRadius: { type: float, default: 5.0 }
  glowColor:  { type: vec3,  default: [1.0, 0.8, 0.3] }

fragment: shaders/glow.sc

passes:
  - name: blur_h
    fragment: shaders/blur_h.sc
  - name: blur_v
    fragment: shaders/blur_v.sc
  - name: composite
    fragment: shaders/glow_composite.sc
```

### 加载自定义效果
```lua
graphics.loadEffect("effects/custom_glow.effect")
rect.fill.effect = "filter.custom.glow"
rect.fill.effect.glowRadius = 8
```

### 兼容性
- 现有 `graphics.defineEffect()` **完全保留**
- `graphics.loadEffect()` 是新增的增强版
- 内置效果（blur、grayscale 等）API 不变

---

## 模块 5：Post-Processing（graphics.addPostEffect）

### 基本用法
```lua
-- 全屏后处理
graphics.addPostEffect("bloom", { threshold = 0.8, intensity = 1.2 })
graphics.addPostEffect("vignette", { radius = 0.7 })

-- 移除
graphics.removePostEffect("bloom")

-- 对指定 group 做后处理（通过 snapshot 实现）
local gameLayer = display.newSnapshot(display.contentWidth, display.contentHeight)
gameLayer.effectChain = {
    { name = "bloom", params = { threshold = 0.6 } },
    { name = "colorGrading", params = { lut = "warm.png" } },
}
```

### 内部行为
- 用 bgfx multi-view 实现：场景渲染到 FBO → 后处理 pass → 最终输出
- 多个后处理按顺序链式执行
- 每个 post effect 是一个全屏 quad + fragment shader

---

## 模块 6：GPU Compute（gpu.*）

### 函数式 API（无状态）
```lua
-- 创建 GPU buffer
local buffer = gpu.newBuffer({
    size = 10000,
    type = "vec4",          -- 每元素 4 float
    usage = "compute",      -- 或 "vertex" / "readback"
})

-- 运行 compute shader
gpu.compute("particle_update.compute", {
    input = { positions = buffer, dt = 0.016 },
    output = { positions = buffer },
    groups = { 256, 1, 1 },
})

-- 异步读回 CPU（不阻塞渲染）
-- callback 签名: function(data, error)
-- data: Lua table（float 数组），error: nil 或 string
gpu.readback(buffer, function(data, err)
    if err then
        print("GPU error: " .. err)
        return
    end
    print("GPU result:", data[1], data[2])
end)
-- 注意：buffer 在 readback 完成前不可销毁，引擎内部持有引用
```

### 内部行为
- 基于 bgfx compute shader API
- buffer 生命周期由引擎管理
- readback 是异步的，通过回调返回

---

## 模块 7：增强型构造函数

### 属性表风格（AI 友好）
```lua
-- 现有方式（保留）
local rect = display.newRect(100, 200, 50, 50)
rect:setFillColor(1, 0, 0)
rect.alpha = 0.8
rect.rotation = 45

-- 新增：属性表方式（一行搞定，AI 更容易生成）
local rect = display.newRect(100, 200, 50, 50, {
    fillColor = { 1, 0, 0 },
    alpha = 0.8,
    rotation = 45,
    strokeWidth = 2,
    strokeColor = { 1, 1, 1 },
})

-- display.newImage 同理
local hero = display.newImage("hero.png", {
    x = 100, y = 200,
    anchorX = 0.5, anchorY = 1.0,
    scaleX = 2,
})
```

### 内部行为
- 检测最后一个参数是否为 table
- 是 → 创建对象后自动应用属性
- 否 → 保持现有行为（完全兼容）

---

## 模块 8：类型定义（AI/IDE 支持）

### EmmyLua 类型文件
```lua
-- solar2d-types.lua（随引擎分发）

---@class Atlas
---@field getImageSheet fun(name: string, opts: table): ImageSheet
---@field getFrame fun(name: string): Frame
---@field has fun(name: string): boolean
---@field list fun(): string[]
---@field reload fun()

---@class Batch : DisplayObject
---@field add fun(name: string, x: number, y: number, opts?: table): BatchSlot
---@field clear fun()
---@field count fun(): number

---@class BatchSlot
---@field x number
---@field y number
---@field rotation number
---@field remove fun()
```

---

## 模块 9：热重载增强

### 现有
- Cmd+R 重启整个 app

### 新增
```lua
-- 资源变化监听
Runtime:addEventListener("assetChanged", function(event)
    print(event.filename)  -- "hero.png"
    print(event.type)      -- "image" / "shader" / "effect"
    -- 引擎自动刷新，无需手动处理
end)

-- Shader 热重载
-- 修改 .sc 文件 → shaderc 自动编译 → 下一帧生效
-- 修改 .effect 文件 → 自动重载
```

### 内部行为
- 文件系统监听（fsevents/inotify）
- 图片变化 → 刷新 texture（atlas 自动重打包）
- Shader 变化 → shaderc 编译 → 替换 program
- Lua 变化 → 保持现有 Cmd+R 行为

---

## 模块 10：3D 扩展（display3d.* — 未来）

预留接口设计，不在当前版本实现。

```lua
-- 3D 场景
local scene3d = display3d.newScene()
local camera = display3d.newCamera({ fov = 60, near = 0.1, far = 1000 })
local mesh = display3d.newMesh("model.gltf", { x = 0, y = 0, z = -5 })
local light = display3d.newLight({
    type = "directional",
    direction = { -0.5, -1, -0.3 },
    color = { 1, 1, 1 },
})

scene3d:insert(camera)
scene3d:insert(mesh)
scene3d:insert(light)

-- 2D UI 叠加在 3D 场景上（2D 始终在最前）
local scoreText = display.newText("Score: 0", 20, 20)
```

---

## 通用约定

### 对象生命周期
所有新增的 display object（batch、particles 等）遵循 Solar2D 现有规则：
```lua
-- 销毁
object:removeSelf()
object = nil

-- 或
display.remove(object)
object = nil
```

资源对象（atlas、buffer）：
```lua
atlas:removeSelf()    -- 释放 GPU 纹理
buffer:removeSelf()   -- 释放 GPU buffer
```

### 错误处理约定
所有 `new*` 构造函数返回 `object, error`：
```lua
local obj, err = graphics.newAtlas({ "missing.png" })
if not obj then print(err) end

local buf, err = gpu.newBuffer({ size = 999999999 })
if not buf then print(err) end  -- "GPU memory allocation failed"
```

全局错误事件（兼容现有 unhandledError）：
```lua
Runtime:addEventListener("unhandledError", function(event)
    print(event.errorMessage)
end)
```

### 内存管理
- Atlas：引擎跟踪所有引用，最后一个使用者销毁时自动释放 GPU 纹理
- Batch：removeSelf 时释放所有 slot 和 transient buffer
- Buffer：removeSelf 释放 GPU 内存，readback 未完成时延迟到完成后释放
- 引擎提供内存查询：`system.getInfo("gpuMemory")` 返回 GPU 显存使用量

### 平台要求
| 模块 | 最低要求 |
|------|----------|
| Atlas/Batch/Effect/PostEffect | Metal（iOS 8+ / macOS 10.11+） |
| gpu.compute | Metal GPU Family 3+（iOS 11+ / macOS 10.13+） |
| display3d | 未定 |

---

## 实现路线

### Phase 2（当前重点）
1. **graphics.newAtlas** — runtime 打包 + 持久化缓存
2. **display.newBatch** — sprite batch rendering
3. **增强型构造函数** — 属性表支持
4. **EmmyLua 类型定义** — types.lua

### Phase 3
5. **graphics.loadEffect** — .effect 文件格式
6. **效果链** — fill.effects 语法
7. **display.newParticles** — CPU 粒子 + instancing
8. **graphics.addPostEffect** — 全屏后处理

### Phase 4
9. **gpu.compute** — GPU 计算
10. **gpu.readback** — 异步数据读回
11. **热重载增强** — shader/资源实时更新

### 未来
12. **display3d.*** — 3D 扩展
13. **GPU 粒子** — compute shader 驱动
14. **跨平台** — Vulkan/WebGPU 后端

---

## 兼容性矩阵

| 现有 API | 新版本行为 |
|----------|-----------|
| `display.newRect/Circle/Line/...` | ✅ 完全保留 |
| `display.newImage(filename)` | ✅ 完全保留 |
| `display.newImage(atlas, name)` | 🆕 新增重载 |
| `display.newSprite(sheet, data)` | ✅ 完全保留 |
| `display.newGroup()` | ✅ 保留，新增 `{batch=true}` 选项 |
| `display.newSnapshot()` | ✅ 完全保留 |
| `display.newEmitter()` | ✅ 保留，新增 `newParticles` |
| `graphics.newImageSheet()` | ✅ 完全保留 |
| `graphics.defineEffect()` | ✅ 完全保留 |
| `graphics.loadEffect()` | 🆕 新增 |
| `graphics.newAtlas()` | 🆕 新增 |
| `display.newBatch()` | 🆕 新增 |
| `display.newParticles()` | 🆕 新增 |
| `gpu.*` | 🆕 新增命名空间 |
| `display3d.*` | 🆕 未来新增 |
| `object.fill.effect = "..."` | ✅ 保留，新增链式语法 |

---
*2026-04-05 v0.1 草案*
