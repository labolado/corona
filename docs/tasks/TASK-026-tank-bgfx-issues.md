# TASK-026: Tank 项目 bgfx 模式渲染问题

## 现象
1. bgfx 模式跳过首页，直接进入游戏内场景（GL 正常显示首页）
2. 颜色偏暗
3. 部分图片可能仍有渲染问题

## 已排查
- 基础 API 隔离测试（newImage/newRect/newText）：bgfx vs GL 一致 ✅
- bswap32 纹理字节反转：已修复
- setViewMode(Sequential)：已修复

## 待排查
- 场景跳转差异：为什么 bgfx 模式跳过首页？
  - touch 事件差异？
  - timer 精度差异？
  - 帧率差异导致 timer 触发时机不同？
  - 场景加载回调时序差异？
- 颜色偏暗：可能是 premultiplied alpha 或 blend mode 差异
- 复杂场景组合：多层 group + physics + Spine + 大量图片

## Tank 项目使用的 API（已分析）
- display.newImageRect（大量）
- display.newImage（车厢 body）
- display.newGroup/newSubGroup
- graphics.newTexture（预加载）
- graphics.newImageSheet + display.newSprite
- physics（火车轨道）
- Spine 动画（车轮）
- ui.newButton（封装的图片按钮）

## 下一步
1. 在 tank 项目中加 bgfx 日志，对比 GL 的场景加载序列
2. 确认是"渲染问题"还是"逻辑流程差异"
3. 逐个 API 组合测试

## 2026-04-06 根因确认

### 根因：graphics.defineEffect 自定义 GLSL shader 在 bgfx/Metal 下静默失败

**排查路径：**
1. 基础 API 隔离测试 → 全部正常
2. ImageSheet + 嵌套 Group → 正常
3. display.newMesh → 正常
4. 逐步还原 tank 首页 → Step 3 LevelHelper 加载时出问题
5. 深入分析 → LHBezierTrack 用了 tiling shader
6. **tiling shader 隔离测试 → bgfx 下 shader 完全不执行**

**表现：**
- graphics.defineEffect(kernel) 不报错
- obj.fill.effect = "filter.custom.xxx" 不报错
- shader 代码完全不执行，回退到默认着色器
- fill.scaleX=0.07 被当成普通缩放 → 纹理极度放大

**影响：**
所有使用 graphics.defineEffect 自定义 shader 的功能在 bgfx 下不工作。
包括但不限于：tiling、自定义颜色效果、自定义后处理等。

**修复方案：**
1. 方案 A：bgfx 引擎支持运行时 GLSL→Metal 编译（复杂）
2. 方案 B：graphics.defineEffect 支持同时提供 GLSL 和 Metal shader（需 API 扩展）
3. 方案 C：预编译自定义 shader 到 Metal（构建时处理）
4. 临时方案：在 bgfx 模式下对不支持的 shader 打 warning，用 Lua 层 workaround

**教训：**
- 测试必须覆盖 graphics.defineEffect 自定义 shader
- 静默失败是最危险的 — 不报错但结果完全错误
