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

## Phase 1：内部优化（不改 Lua API，用户无感）✅ 已完成

### 1.1 Release 构建修复 ✅
- build phase 重排序（Skins 拷贝先于 codesign）+ 还原优化级别

### 1.2 Transient Buffer ✅
- pool geometry 改用 `bgfx::allocTransientVertexBuffer`，省 buffer create/destroy 开销

### 1.3 Static Geometry Cache ✅
- fGPUDirty flag，不变的 geometry 跳过 GPU 上传

## Phase 2：新 API（Atlas + Batch）🔄 进行中

### 2.1 Texture Atlas ✅
- `graphics.newAtlas()` — 运行时多图打包到一张 texture
- `display.newImage(atlas, "name")` — 从 atlas 创建图片
- Review 问题已修复（哈希查找、内存泄漏、错误处理）

### 2.2 Sprite Batch ✅
- `display.newBatch(atlas, capacity)` — 合并顶点，1 个 draw call
- GPU Instancing 支持（20000 对象仍 60fps）
- 回退路径测试通过（graphics.setInstancing 开关）

### 2.3 Atlas + Batch 测试 ✅
- test_atlas.lua 14/14 PASS + test_batch.lua 11/11 PASS

## Phase 3：透明优化（不改 API，自动生效，跨平台零风险）

### 3.1 SDF 形状渲染 ✅
- **状态**: 已完成并合并（feature/sdf-rendering → main）
- **覆盖**: circle, rect, roundedRect, line, polygon（所有形状）
- **收益**: 顶点数 -90%，像素完美抗锯齿
- **API**: `graphics.setSDF(true/false)` 开关，默认开启
- **低端机**: >16 顶点多边形自动回退 mesh
- **性能**: VSync 下持平（Debug 1000 shapes ~62 FPS）

### 3.2 自动合批（Auto Batching）✅
- **状态**: 已完成（3 行代码改动，draw call -99.5%）
- **改动**: Renderer::Insert() 扩展 kTriangles 合批条件

### 3.3 Instancing ✅
- **状态**: 已完成（BatchObject GPU instancing + 回退测试）
- **API**: `graphics.setInstancing(true/false)` 开关
- **低端机**: 不支持时自动回退 CPU transient buffer

### 3.4 脏区域渲染（Dirty Rect）✅
- **状态**: 现有 static geometry cache + Scene::IsValid 已覆盖
- **API**: `graphics.getDirtyStats()` 统计接口
- **结论**: 不需要额外实现，已有机制够用（数据证明）

### 3.5 GPU 纹理压缩 ✅
- **状态**: 已完成（自动搜索压缩变体 + 能力检测 API）
- **API**: `graphics.getTextureCapabilities()` 返回设备支持的格式
- **加载**: 自动搜索 .astc/.bc3/.etc2 压缩变体文件
- **Metal**: ASTC/BC/ETC2/PVRTC 全支持

### 3.6 遮挡剔除 ✅
- **状态**: Solar2D 已有完整视口裁剪机制（CullOffscreen + IsOffScreen）
- **验证**: 3000 对象 3x 屏幕范围，89.4% 被裁剪，59.4 FPS
- **结论**: 不需要额外实现

## 低端机兼容性分析

每个优化在低端设备上的风险和降级策略：

| 优化 | 低端机风险 | 降级策略 | 检测方法 |
|------|-----------|---------|---------|
| **SDF 形状** | 旧 GPU 不支持 `fwidth()`/derivatives | 自动回退 mesh 渲染（当前方式） | `bgfx::getCaps()->supported & BGFX_CAPS_FRAGMENT_DEPTH` 或 shader 编译失败检测 |
| **自动合批** | 无风险 | 纯 CPU 逻辑，无设备依赖 | — |
| **Instancing** | 旧 GPU 不支持 instancing | 回退到逐个 draw call | `bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING` |
| **脏区域** | 无风险 | 纯 CPU 逻辑 | — |
| **纹理压缩** | 不同设备支持不同格式 | 运行时检测，回退 RGBA | `bgfx::isTextureValid(ASTC/BC/ETC2)` |
| **遮挡剔除** | 无风险 | 纯 CPU 逻辑 | — |
| **Atlas** | 大纹理尺寸限制（旧设备 2048） | maxSize 参数限制，自动分多张 atlas | `bgfx::getCaps()->limits.maxTextureSize` |
| **Batch** | transient buffer 大小限制 | 超限自动拆分为多次 draw | `bgfx::getAvailTransientVertexBuffer()` |
| **Compute Shader** | 旧 GPU 不支持 | 回退 CPU Lua 实现 | `bgfx::getCaps()->supported & BGFX_CAPS_COMPUTE` |

### 低端机定义（目标最低配置）
- **iOS**: iPhone 6s (A9, Metal)
- **Android**: OpenGL ES 3.0 / Vulkan 1.0
- **Desktop**: 任何支持 OpenGL 3.3 / D3D11 的 GPU
- **Web**: WebGL 2.0

### 自动降级机制设计
```cpp
// 引擎启动时检测设备能力，设置 feature flags
struct EngineFeatures {
    bool sdf;           // SDF 形状渲染
    bool instancing;    // GPU instancing
    bool compute;       // Compute shader
    bool astc;          // ASTC 纹理压缩
    // ... 根据 bgfx::getCaps() 自动填充
};

// 开发者可手动覆盖
// graphics.setFeature("sdf", false)  -- 强制关闭 SDF
```

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

## 每项优化的验收标准（铁律）

每个优化点合入前，必须通过以下 3 项验证：

### 1. 功能测试
- 全量回归：`bash tests/run_all_tests.sh debug` — 10 场景全 PASS
- GL 和 bgfx 双后端均通过
- 专项测试：该优化对应的 test_xxx.lua 全部 PASS

### 2. 性能测试
- 优化前后跑 `SOLAR2D_TEST=bench`，记录 5 级别（500-5000）FPS
- 优化后 FPS ≥ 优化前（不允许退化）
- `SOLAR2D_TEST=realworld` 真实场景基准无退化

### 3. 内存泄漏检查
- 创建→使用→销毁循环 100 次，内存不持续增长
- `removeSelf()` 后确认 C++ 对象释放（析构日志）
- Lua GC 后无残留 userdata
- Instruments Leaks 工具验证（Release 构建）

### 验证命令模板
```bash
cd /Users/yee/data/dev/app/labo/corona

# 功能回归
bash tests/run_all_tests.sh debug

# 性能基准
SOLAR2D_TEST=bench SOLAR2D_BACKEND=bgfx "./platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator" -no-console YES tests/bgfx-demo

# 内存泄漏（专项测试中实现循环创建/销毁）
SOLAR2D_TEST=leak SOLAR2D_BACKEND=bgfx "./platform/mac/build/Debug/Corona Simulator.app/Contents/MacOS/Corona Simulator" -no-console YES tests/bgfx-demo
```

## 测试入口列表

| 入口 | 环境变量 | 测试内容 | 状态 |
|------|----------|----------|------|
| test_bench.lua | `SOLAR2D_TEST=bench` | 综合基准（500-5000对象） | ✅ |
| test_regression.lua | `SOLAR2D_TEST=regression` | 10 场景回归 | ✅ |
| test_realworld.lua | `SOLAR2D_TEST=realworld` | 真实游戏场景基准 | ✅ |
| test_scene.lua | `SOLAR2D_TEST=scene` | 指定场景验证 | ✅ |
| test_capture.lua | `SOLAR2D_TEST=capture` | CaptureRect 测试 | ✅ |
| test_atlas.lua | `SOLAR2D_TEST=atlas` | Atlas 功能测试 | 待创建 |
| test_batch.lua | `SOLAR2D_TEST=batch` | Batch 功能+性能 | 待创建 |
| test_sdf.lua | `SOLAR2D_TEST=sdf` | SDF 渲染对比 | 待创建 |
| test_leak.lua | `SOLAR2D_TEST=leak` | 内存泄漏检测 | 待创建 |

---
*2026-04-05 创建，持续更新*
