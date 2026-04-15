# laboladoDev API Changes & New Features — Reference for bgfx-solar2d

**来源分支**: `origin/laboladoDev`
**合并到**: `bgfx-solar2d`
**合并日期**: 2026-04-15
**合并 commit**: `d57892d7`
**覆盖范围**: 40 commits，时间跨度 2025-06-06 ~ 2026-03-25

此文档供 bgfx-solar2d 维护者参考，说明合并进来的上游变更中哪些是新 API、哪些是 bug fix、哪些影响构建配置，以及需要注意的兼容性问题。

---

## Section 1：新 API 与功能（开发者可用）

### 1.1 Physics: Circle Body x/y Offset

**来源**: commit `3e0e63ae`, PR #814, Kan, 2025-07-06

circle 物理体形状现在支持 `x` 和 `y` offset 参数，允许将碰撞圆形相对于对象中心偏移。

```lua
-- 新增 x, y 参数
physics.addBody(obj, { shape = "circle", radius = 20, x = 10, y = -5 })
```

之前 circle body 的圆心固定在对象原点，无法偏移。

---

### 1.2 Physics: Raycast Respects maskBits

**来源**: commit `27c48fdc`, PR #827, Kan, 2025-09-21

`physics.rayCast()` 现在正确遵守碰撞过滤的 `maskBits`。此前 raycast 会命中所有物理体，忽略碰撞层设置。

```lua
-- maskBits 过滤现在对 rayCast 生效
local hits = physics.rayCast(x1, y1, x2, y2, "unsorted")
```

---

### 1.3 display.save WebP Format on Android

**来源**: commit `4e61c8a7`, Chen Bin, 2025-12-10（8 文件）

`display.save()` 在 Android 上新增 WebP 格式支持。

```lua
display.save(obj, { filename = "output.webp", baseDir = system.DocumentsDirectory })
```

此前 Android 仅支持 PNG/JPEG。该功能仅影响 Android 平台；iOS/macOS/HTML5 行为不变。

---

### 1.4 HTML5: LetterBox Portrait Fullscreen

**来源**: commit `4d9a8aaf`, PR #821, Kan, 2025-07-09（2 文件）

HTML5 构建现在支持竖屏 letterbox 全屏模式，在宽屏浏览器窗口中以竖屏比例居中显示内容，两侧留黑边。

**同一 commit 还新增 web build settings**，可在 `build.settings` 中配置 HTML5 特定选项（具体键名见 Solar2D 文档）。

---

### 1.5 HTML5: Vsync Support

**来源**: commit `e79d911d`, PR #819, Kan, 2025-07-06（4 文件）

HTML5 构建启用 Vsync，使用 `requestAnimationFrame` 同步刷新率，避免撕裂并降低 CPU 占用。同时优化了移动端 CSS 布局。

---

### 1.6 HTML5: Improved Scaling & More WASM Memory

**来源**: commit `4c1f709c`, PR #818, Kan, 2025-06-30（3 文件）

- HTML5 canvas 缩放逻辑改进，DPI 处理更准确
- WASM heap 内存上限提升，减少大项目 OOM 崩溃

---

### 1.7 Linux Simulator: Open Project from Command Line

**来源**: commit `5bec6eba`, PR #842, vikramvicky13, 2025-09-22（1 文件）

Linux 版模拟器现在可以通过命令行参数直接打开项目目录：

```bash
./Solar2DSimulator /path/to/project
```

---

### 1.8 Linux Simulator: Rotation Support

**来源**: commit `3ac87805`, PR #837, vikramvicky13, 2025-09-20（3 文件）

Linux 模拟器新增设备旋转支持（向左旋转 / 向右旋转），与 macOS 模拟器功能对齐。

---

### 1.9 Linux Simulator: Orientation Validation

**来源**: commit `4bd316e9`, PR #846, vikramvicky13, 2025-09-23（2 文件）

Linux 模拟器现在检查 `build.settings` 中声明的 `supportedOrientations`，只允许切换到应用支持的方向。

---

### 1.10 CoronaBuilder CLI: liveBuild Parameter

**来源**: commit `c7134c2d`, LaboLado, 2026-03-25（1 文件）

CoronaBuilder CLI 新增 `liveBuild` 参数支持，允许从命令行触发 Live Build 流程，无需打开 GUI。

```bash
CoronaBuilder build --liveBuild ...
```

---

### 1.11 iOS: System Gesture Deferral (preferredScreenEdgesDeferringSystemGestures)

**来源**: commit `7dedd167`, PR #864, Usman Mughal, 2026-01-03（3 文件）

正确实现 iOS `preferredScreenEdgesDeferringSystemGestures`，允许游戏在全屏模式下推迟系统边缘手势（如底部上划返回 Home）响应，提升沉浸式体验。

---

## Section 2：Bug Fixes（问题修复）

### 2.1 Box2D Joint Use-After-Free Crash

**来源**: commit `c8c3bd1e`, PR #858, Jeremy, 2025-10-24（1 文件）

**问题**: 当 Box2D world 被销毁时，如果仍有活跃的 joint 对象，会触发 use-after-free 崩溃（UAF）。
**修复**: 在 world 销毁时正确清理 joint 引用，避免悬空指针访问。
**影响**: 所有使用 physics joint 并动态销毁 physics world 的项目。

---

### 2.2 Physics: Wheel Joint Parameter Fix

**来源**: commit `2e181229`, PR #817, Vlad Svoka, 2025-06-30（2 files）

**问题**: wheel joint 的某些参数（频率、阻尼比等）传递不正确，导致关节行为与预期不符。
**修复**: 修正参数传递路径，确保 Lua 侧设置的值正确传入 Box2D。

---

### 2.3 GL Texture Pixel Unpack Alignment

**来源**: commit `1dec8e7b`, PR #803, Kan, 2025-06-06（1 文件）

**问题**: 多种 GL 纹理格式（`GL_ALPHA`, `GL_LUMINANCE`, `GL_RGB`, `GL_RGBA`, `GL_BGRA_EXT`, `GL_ABGR_EXT`）在上传像素数据时未正确设置 `GL_UNPACK_ALIGNMENT`，可能导致纹理数据错位/渲染异常。
**修复**: 对各格式明确设置正确的 unpack alignment。
**与 bgfx 的关联**: bgfx 通过自身 API 管理纹理上传，不经过这条路径，但 GL 后端受益。

---

### 2.4 Android Non-arm64 Platform Builds

**来源**: commit `710a715e`, PR #857, Vlad Svoka, 2025-10-23（15 文件）

**问题**: Android 构建在 `x86`、`x86_64`、`armeabi-v7a` 架构上失败或产生错误产物。
**修复**: 修正 CMake 配置和 ABI 过滤，确保非 arm64 架构正确编译。

---

### 2.5 Android Navigation Bar Behavior

**来源**: commit `3f8d2caf`, PR #828, Scott Harrison, 2025-07-30（1 文件）

**问题**: Android 导航栏（底部系统栏）在某些情况下遮挡内容或无法正确隐藏。
**修复**: 修正导航栏的 inset 处理与全屏模式交互逻辑。

---

### 2.6 HTML5 Audio Pausing/Stopping

**来源**: commit `dbf42bc2`, PR #871, Scott Harrison, 2025-12-01（1 文件）

**问题**: HTML5 构建中音频暂停和停止功能失效，Web Audio API 使用存在多处问题。
**修复**: 大幅重写 Web Audio 播放器实现，修复暂停/停止/恢复逻辑。

---

### 2.7 HTML5 Custom Font Positioning

**来源**: commit `f09b8144`, PR #862, Scott Harrison, 2025-11-04（2 文件）

**问题**: HTML5 构建使用自定义字体时文字位置偏移，与 native 平台显示不一致。
**修复**: 修正字体 metrics 计算和 canvas 文字渲染的基线对齐。

---

### 2.8 macOS Mono Audio

**来源**: commit `ad84ed76`, PR #872, Scott Harrison, 2025-11-23（1 文件）

**问题**: macOS 上播放单声道（mono）音频文件时出现问题（可能表现为静音或崩溃）。
**修复**: 修正 macOS 音频播放路径对 mono 格式的处理。

---

### 2.9 Mac Simulator Console Search

**来源**: commit `4914f7d9`, PR #856, Scott Harrison, 2025-10-24（1 文件）

**问题**: Mac 模拟器内置控制台的搜索功能存在缺陷（搜索结果不准确或 UI 响应异常）。
**修复**: 改进控制台搜索逻辑和 UI 交互。

---

### 2.10 iOS/tvOS Logging (syslog)

**来源**: commit `c866ec5b`, PR #861, Scott Harrison, 2025-10-24（3 文件）

**问题**: iOS/tvOS 的 syslog 相关脚本存在错误，影响设备日志收集。
**修复**: 修正 syslog 脚本逻辑。

---

### 2.11 Linux Simulator Skin Change

**来源**: commit `5f55ab8e`, PR #816, Kan, 2025-06-30（1 文件）

**问题**: Linux 模拟器切换设备外观（skin）时出现异常行为（可能是 crash 或显示错误）。
**修复**: 修正 skin 切换流程中的资源管理逻辑。

---

## Section 3：平台与构建变更（Platform & Build Changes）

### 3.1 Xcode 26.1 / iOS 26 / macOS 26 Support

**来源**:
- commit `e6ff87cd`, PR #839, Scott Harrison, 2025-09-20 — 主体支持（4 文件）
- commit `ff34cdee`, PR #873, Scott Harrison, 2025-12-02 — CI 矩阵添加 Xcode 26.1（1 文件）

iOS 26 / macOS 26 是 Apple 2025 年发布的新系统版本（对应 Xcode 26.x）。新增了必要的 SDK 适配和构建配置。**bgfx-solar2d 分支使用同一 Xcode 项目，此变更自动生效。**

---

### 3.2 Windows Toolset Migration to v141

**来源**:
- commit `f1aeef4e`, PR #125, Vasyl Shcherban, 2025-08-12 — 迁移（28 文件）
- commit `fb4b583b`, PR #831, Vlad Svoka, 2025-08-24 — 后续修复（27 文件）

Windows 构建工具链从旧版迁移至 Visual Studio 2017 toolset v141，目标 SDK 10.0.18362.0。涉及约 55 个文件（`.vcxproj`, `.props` 等）。**如需在 Windows 构建 Solar2D，需要安装对应 MSVC 组件。**

---

### 3.3 iOS arm64 Simulator Support

**来源**: commit `51a347cb`, PR #874, Scott Harrison, 2026-01-03（9 文件）

CoronaBuilder 现在支持基于 Apple Silicon 的 iOS 模拟器（arm64 simulator slice）。在 M 系列 Mac 上，可直接运行 arm64 iOS 模拟器构建，无需 Rosetta。

---

### 3.4 iOS/tvOS Packaging Simplification

**来源**: commit `3b738fa8`, PR #850, Denis Claros, 2025-10-16（2 文件）

iOS/tvOS 打包脚本简化，减少冗余步骤。对最终产物无影响，仅影响构建流程脚本。

---

### 3.5 Certificate & Provisioning Profile Updates

**来源**: commit `1c077630`, PR #830, Vlad Svoka, 2025-08-12（4 文件）

更新了内部使用的证书和 provisioning profiles。**bgfx-solar2d 使用自己的签名配置，此变更不直接影响。**

---

### 3.6 Box2D Submodule Update (b2FakeJoint)

**来源**: commit `8eac16d3`, Vlad Svoka, 2025-09-21（1 文件 — submodule pointer）

更新 Box2D submodule，包含 `b2FakeJoint` 支持。`b2FakeJoint` 是 Solar2D 为实现某些 Lua 侧 joint 类型（如 `"touch"` joint）引入的非标准 joint 类型，此次更新将其纳入 Box2D submodule 而非散落在主仓库代码中。

**bgfx-solar2d 注意**: `external/bgfx` 和 Box2D submodule 是独立的，但合并时需确认 Box2D submodule 指针正确更新（`git submodule update --init`）。

---

### 3.7 Android SDK & Runtime Lifecycle Updates

**来源**: commit `664d2724`, PR #826, Scott Harrison, 2025-07-16（8 文件）

- Android target/compile SDK 版本更新
- `CoronaActivity` 改进
- runtime lifecycle（`onPause`/`onResume`/`onDestroy`）处理优化

**bgfx-solar2d 注意**: bgfx Android 路径涉及 `CoronaActivity` 的 GL context 管理，需检查 lifecycle 变更是否与 bgfx 初始化/销毁逻辑冲突。

---

### 3.8 Android CMakeLists Cleanup

**来源**: commit `32a11782`, PR #825, zero-meta, 2025-07-11（1 文件）

Android `CMakeLists.txt` 清理冗余配置，无功能变更。

---

## Section 4：Breaking Changes / Migration Notes

以下变更涉及 C++ 内部 API 签名或行为变化。bgfx-solar2d 通过自己的子类/override 保持兼容，但合并时需注意冲突点。

---

### 4.1 TextureFactory API 签名变更

**影响文件**: `librtt/Display/Rtt_TextureFactory.h` / `.cpp`

`TextureFactory::FindOrCreate` 和 `TextureFactory::CreateAndAdd` 新增了 `onlyForHitTests` 参数：

```cpp
// 旧签名（示意）
SharedPtr<TextureResource> FindOrCreate(const char* filename, ...);

// 新签名（示意）
SharedPtr<TextureResource> FindOrCreate(const char* filename, ..., bool onlyForHitTests);
```

**bgfx-solar2d 影响**: 如果 bgfx 侧有自定义的纹理加载路径，需检查调用点是否传递了新参数。合并时若有冲突，优先保留 bgfx 的纹理管理逻辑，新增参数传 `false` 作为默认值。

---

### 4.2 BitmapPaint::NewBitmap 新重载

**影响文件**: `librtt/Display/Rtt_BitmapPaint.h` / `.cpp`

`BitmapPaint::NewBitmap` 新增带 `onlyForHitTests` 参数的重载：

```cpp
static BitmapPaint* NewBitmap(Runtime& runtime, const char* filename,
                               MPlatform::Directory baseDir,
                               bool isMask, bool onlyForHitTests);
```

**bgfx-solar2d 影响**: bgfx 路径通常不直接调用此方法，低风险。若有冲突，新重载保留，旧重载可 `onlyForHitTests = false` 转发。

---

### 4.3 BitmapMask::Create 参数变更

**影响文件**: `librtt/Display/Rtt_BitmapMask.h` / `.cpp`

`BitmapMask::Create` 方法现在接受 3 个参数（新增 `onlyForHitTests`）：

```cpp
// 新签名（示意）
static BitmapMask* Create(Runtime& runtime, const FilePath& maskData, bool onlyForHitTests);
```

**bgfx-solar2d 影响**: mask 渲染在 bgfx 路径中有独立实现。需检查 `BitmapMask::Create` 的调用点，补充 `false` 参数。

---

### 4.4 Renderer::Insert 内部变更

**影响文件**: `librtt/Renderer/Rtt_Renderer.cpp` / `.h`

`Renderer::Insert` 内部引入了 `fOffsetCorrection` 和 `fVertexExtra` 字段，用于支持新的 geometry offset 特性（关联 circle body x/y offset 的渲染侧支持）。

**bgfx-solar2d 关键注意**:
- `Rtt_Renderer.cpp` 是 GL 和 bgfx **共享代码**，改动同时影响两个后端
- `fOffsetCorrection` / `fVertexExtra` 的语义需要在 bgfx CommandBuffer 中正确处理
- 建议：在 `Rtt_BgfxCommandBuffer.cpp` 中检查 vertex 数据组装路径，确认新字段被正确读取和传递
- **不确定时先加日志验证，不要盲目假设行为一致**

---

### 4.5 GLTexture Refactor（GL 专属，bgfx 无影响）

**来源**: commit `8774c8b5`, PR #812, Scott Harrison, 2025-06-06

GLTexture 代码做了清理和重构。该文件是 GL 后端专属（`Rtt_GLTexture.cpp`），不影响 bgfx 路径。

**bgfx-solar2d 影响**: 无，bgfx 使用 `bgfx::TextureHandle` 管理纹理，不经过 GLTexture。

---

### 4.6 HTML5 Context & Platform Changes（HTML5 专属）

**来源**: commit `9e6813a2`, PR #852, Kan, 2025-11-24（2 文件）

HTML5 platform context 改进，属于 `platform/html5/` 专属代码。bgfx-solar2d 当前不支持 HTML5 后端，无直接影响。

---

## 合并建议

1. **优先验证的区域**（bgfx 路径可能受影响）:
   - `Renderer::Insert` / vertex 数据路径（`fOffsetCorrection`, `fVertexExtra`）
   - `CoronaActivity` lifecycle（`onPause`/`onResume` 与 bgfx context 恢复）
   - Android CMake 配置（确认 bgfx 相关 target 不受影响）

2. **无需额外验证**（GL/HTML5/Linux 专属）:
   - GLTexture 重构
   - HTML5 audio/font/scaling 修复
   - Linux 模拟器功能

3. **submodule 更新**（合并后必须执行）:
   ```bash
   git submodule update --init --recursive
   ```

4. **回归测试**（合并后必须跑）:
   ```bash
   bash tests/run_all_tests.sh debug
   ```
   重点关注 physics joint、纹理渲染、Android 构建三个方向。
