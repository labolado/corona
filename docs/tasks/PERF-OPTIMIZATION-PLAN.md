# bgfx 引擎优化与扩展计划

## 基线数据（M3 Max）

### Debug 构建
| 对象数 | GL FPS | bgfx FPS | 差异 |
|--------|--------|----------|------|
| 500 | 63.2 | 63.2 | 持平 |
| 1000 | 62.9 | 62.5 | 持平 |
| 2000 | 31.6 | 31.3 | 持平 |
| 3000 | 28.7 | 31.2 | bgfx +9% |
| 5000 | 20.7 | 20.7 | 持平 |

### Release 构建
| 对象数 | GL FPS | bgfx FPS | 差异 |
|--------|--------|----------|------|
| 500 | 63.3 | 62.9 | 持平 |
| 1000 | 63.0 | 62.9 | 持平 |
| 2000 | 62.8 | 62.6 | 持平 |
| 3000 | 62.4 | 62.3 | 持平（都满帧） |
| 5000 | 31.3 | 31.3 | 完全持平 |

## Phase 1：内部优化（不改 Lua API，用户无感）

### 1.1 Release 构建修复
- **状态**: ✅ 已完成
- **修复**: build phase 重排序（Skins 拷贝先于 codesign）+ 还原优化级别

### 1.2 Transient Buffer
- **状态**: 待开始
- **当前**: 每个 pool geometry 用 dynamic buffer，每帧 CPU→GPU 拷贝
- **优化**: 改用 bgfx transient buffer（`bgfx::allocTransientVertexBuffer`）
- **原理**: transient buffer 从环形缓冲区分配，不需要 create/destroy，写入后自动提交
- **预期收益**: 20-30% FPS 提升（减少 buffer 管理开销）
- **影响范围**: `Rtt_BgfxGeometry.cpp`、`Rtt_BgfxCommandBuffer.cpp`
- **测试入口**: `SOLAR2D_TEST=bench`

## Phase 2：中级优化

### 2.1 Static Geometry Cache
- **当前**: 所有 geometry 每帧重新上传
- **优化**: 不变的 geometry（静态 UI、背景）用 static buffer，只上传一次
- **原理**: `bgfx::createVertexBuffer`（static）vs `bgfx::createDynamicVertexBuffer`
- **预期收益**: 10-20%（减少不必要的 GPU 上传）
- **判断条件**: geometry dirty flag 未设置 → 跳过上传

### 2.2 Draw Call Batching
- **当前**: 每个 display object 一个 draw call
- **优化**: 相同 shader + texture + blend mode 的对象合并为一个 draw call
- **原理**: bgfx 的 view 内自动排序 + encoder 合批
- **预期收益**: 30-50%（draw call 数量级下降）
- **复杂度**: 高 — 需要重写渲染提交逻辑
- **测试入口**: 需要新建 `test_drawcall.lua`（大量不同 texture 的对象）

## Phase 3：高级优化

### 3.1 Instancing
- **当前**: 同一 mesh 画 N 次 = N 个 draw call
- **优化**: `bgfx::setInstanceDataBuffer` 一次提交所有实例
- **预期收益**: 5-10x（粒子系统、弹幕等场景）
- **前提**: 需要实例化 shader 支持
- **测试入口**: 需要新建 `test_instancing.lua`

### 3.2 Texture Atlas
- **当前**: 每个图片一个 texture，切换 texture = 打断批次
- **优化**: 多图合并到 atlas，UV 重映射
- **预期收益**: 10-20%（减少 texture bind 切换）

### 3.3 Compute Shader
- **场景**: 粒子系统位置更新、物理模拟
- **当前**: CPU Lua 循环更新
- **优化**: GPU compute shader 并行计算
- **预期收益**: 场景依赖，粒子系统可达 10-100x

## 性能测试规范

```bash
# 运行基准测试
SOLAR2D_TEST=bench SOLAR2D_BACKEND=bgfx ./Corona\ Simulator -no-console YES tests/bgfx-demo

# GL 对照组
SOLAR2D_TEST=bench SOLAR2D_BACKEND=gl ./Corona\ Simulator -no-console YES tests/bgfx-demo
```

每次优化前后都跑 bench，记录数据到本文档。

## 测试入口列表

| 入口 | 环境变量 | 测试内容 |
|------|----------|----------|
| test_bench.lua | `SOLAR2D_TEST=bench` | 综合基准（500-5000对象） |
| test_drawcall.lua | `SOLAR2D_TEST=drawcall` | draw call 密集场景 |
| test_instancing.lua | `SOLAR2D_TEST=instancing` | 实例化渲染 |
| test_gpu_heavy.lua | `SOLAR2D_TEST=gpu_heavy` | GPU-bound 场景（shader密集） |

## Phase 4：扩展 API（新 Lua 接口，解锁 bgfx 独有能力）

### 4.1 Instancing API
- `display.newInstanceBatch(mesh, count)` — 同一 mesh 画上万个实例
- 场景：弹幕、粒子、森林树木、星空
- 1 万个对象 = 1 个 draw call（GL 需要 1 万个）

### 4.2 后处理链
- `graphics.addPostEffect("bloom")` / `graphics.addPostEffect("blur", {radius=5})`
- 全屏泛光、动态模糊、色调映射、景深
- 基于 bgfx multi-view 和 FBO 链式渲染

### 4.3 Compute Shader
- `graphics.compute(shader, inputData, outputData)`
- GPU 并行计算：粒子位置更新、布料模拟、流体、AI 推理前处理
- 比 Lua CPU 循环快 100-1000 倍

### 4.4 自定义顶点格式
- `graphics.newVertexFormat({pos=2, color=4, uv=2, normal=3, custom=4})`
- 支持骨骼动画权重、法线贴图、自定义属性
- GL 的固定 Vertex 结构无法扩展

### 4.5 GPU Readback（异步）
- `display.captureAsync(function(bitmap) ... end)`
- 异步截图不阻塞渲染，用于 AI 视觉输入、录屏

## Phase 5：新 Shader 体系（保留旧体系兼容）

### 5.1 设计目标
Solar2D 旧 shader 体系痛点：
- GLSL 代码嵌在 Lua 字符串里，没有语法高亮、没有编译期检查
- `graphics.defineEffect` 的 vertex/fragment 格式笨重
- uniform 传参靠约定（u_UserData0/1/2/3），语义不清
- 不支持 include、不支持宏、不支持多 pass

### 5.2 新体系：.effect 文件格式
```yaml
# effects/my_glow.effect
name: "filter.custom.glow"
version: 2

uniforms:
  glowRadius: { type: float, default: 5.0 }
  glowColor:  { type: vec3,  default: [1.0, 0.8, 0.3] }
  intensity:  { type: float, default: 0.7, range: [0, 2] }

vertex: shaders/passthrough.sc    # 引用独立 .sc 文件
fragment: shaders/glow.sc

passes:                           # 多 pass 支持
  - name: horizontal_blur
    fragment: shaders/blur_h.sc
  - name: vertical_blur
    fragment: shaders/blur_v.sc
  - name: composite
    fragment: shaders/glow_composite.sc
```

### 5.3 Lua 使用对比

**旧体系（保留兼容）**：
```lua
graphics.defineEffect({
  category = "filter", name = "custom.glow",
  vertex = "..." ,  -- GLSL 字符串
  fragment = "...",  -- GLSL 字符串
})
obj.fill.effect = "filter.custom.glow"
obj.fill.effect.glowRadius = 5
```

**新体系**：
```lua
-- 从 .effect 文件加载（自动编译到当前平台）
local glow = graphics.loadEffect("effects/my_glow.effect")

-- 应用
obj.fill.effect = glow
obj.fill.effect.glowRadius = 8.0
obj.fill.effect.glowColor = {1, 0.5, 0}

-- 或者内联定义（简单效果）
obj.fill.effect = graphics.newEffect({
  fragment = "shaders/sepia.sc",
  uniforms = { intensity = 0.8 }
})
```

### 5.4 新体系优势
| 特性 | 旧体系 | 新体系 |
|------|--------|--------|
| Shader 代码位置 | Lua 字符串内 | 独立 .sc 文件 |
| 语法高亮/检查 | ❌ | ✅（标准 GLSL-like） |
| 编译时机 | 运行时 | 构建时预编译 |
| Uniform 定义 | 隐式约定 | 显式声明+类型+默认值+范围 |
| 多 Pass | ❌ | ✅ |
| Include/宏 | ❌ | ✅（bgfx shaderc 原生支持） |
| 跨平台 | GLSL only | Metal/Vulkan/DX/WebGPU |
| IDE 支持 | ❌ | ✅（.sc 文件独立编辑） |
| 热重载 | ❌ | ✅（开发时文件变化自动重编译） |

### 5.5 实现路线
1. 定义 .effect YAML 格式规范
2. 构建工具：.effect → 预编译 shader bundle（各平台二进制）
3. 运行时加载器：`graphics.loadEffect()` 读取 bundle
4. Uniform 注册：从 .effect 声明自动创建 bgfx uniform
5. 多 Pass 渲染：基于 bgfx view 系统
6. 旧 `graphics.defineEffect` 保持兼容（内部转换为新格式）

## 性能测试规范

```bash
# 运行基准测试
SOLAR2D_TEST=bench SOLAR2D_BACKEND=bgfx ./Corona\ Simulator -no-console YES tests/bgfx-demo

# GL 对照组
SOLAR2D_TEST=bench SOLAR2D_BACKEND=gl ./Corona\ Simulator -no-console YES tests/bgfx-demo
```

每次优化前后都跑 bench，记录数据到本文档。

## 测试入口列表

| 入口 | 环境变量 | 测试内容 |
|------|----------|----------|
| test_bench.lua | `SOLAR2D_TEST=bench` | 综合基准（500-5000对象） |
| test_scene.lua | `SOLAR2D_TEST=scene SOLAR2D_SCENE=xxx` | 指定场景验证 |
| test_capture.lua | `SOLAR2D_TEST=capture` | CaptureRect 测试 |
| test_drawcall.lua | `SOLAR2D_TEST=drawcall` | draw call 密集场景（待创建） |
| test_instancing.lua | `SOLAR2D_TEST=instancing` | 实例化渲染（待创建） |
| test_gpu_heavy.lua | `SOLAR2D_TEST=gpu_heavy` | GPU-bound 场景（待创建） |
| test_effects.lua | `SOLAR2D_TEST=effects` | Effect shader 逐个验证（待创建） |

---
*2026-04-05 创建，持续更新*
