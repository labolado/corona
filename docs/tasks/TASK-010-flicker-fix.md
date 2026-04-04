# TASK-010: Fix bgfx Metal Flickering on macOS

## 问题描述

bgfx Metal 后端在 macOS 上出现严重频闪问题：
- Scene 6（动画场景）帧间差异 54.2%
- GL 后端同场景仅 ~2% 差异
- 静态内容（Scene 1）不闪，只有动画时闪

## 修复尝试记录

### 尝试 1: maximumDrawableCount = 2 (v1)
**配置**:
```cpp
metalLayer.maximumDrawableCount = 2;
metalLayer.presentsWithTransaction = YES;
```

**结果**: 黑屏 ❌
- presentsWithTransaction = YES 导致 bgfx 无法正确呈现（bgfx 内部没有调用 waitUntilScheduled）

### 尝试 2: maximumDrawableCount = 2 (v2)
**配置**:
```cpp
metalLayer.maximumDrawableCount = 2;
// 无 presentsWithTransaction
```

**结果**: 画面正常，但频闪未改善 ❌
- 帧间差异仍 54.2%
- 与未修复状态相同

### 尝试 3: maximumDrawableCount = 2 + displaySyncEnabled = YES
**配置**:
```cpp
metalLayer.maximumDrawableCount = 2;
metalLayer.displaySyncEnabled = YES;
```

**结果**: 画面正常，频闪减少到 26.3% ⚠️
- 从 54.2% 降至 26.3%（13/19 → 5/19 帧间差异）
- 有明显改善但未完全消除

## 当前代码

**文件**: `librtt/Renderer/Rtt_BgfxRenderer.cpp`

```cpp
#if defined(__APPLE__)
// Helper function to configure CAMetalLayer on macOS
// Uses Objective-C runtime to avoid requiring Objective-C++ compilation
static void ConfigureMetalLayerForMacOS(void* nativeWindowHandle)
{
    if (!nativeWindowHandle)
        return;

    bgfx::RendererType::Enum rendererType = bgfx::getRendererType();
    if (rendererType != bgfx::RendererType::Metal)
        return;

    // Get NSView's layer using objc_msgSend
    id view = (id)nativeWindowHandle;
    
    // [view layer]
    SEL layerSel = sel_registerName("layer");
    id (*msgSend)(id, SEL) = (id (*)(id, SEL))objc_msgSend;
    id layer = msgSend(view, layerSel);
    
    if (!layer)
        return;

    // Check if layer is CAMetalLayer using isKindOfClass:
    Class caMetalLayerClass = objc_getClass("CAMetalLayer");
    if (!caMetalLayerClass)
        return;

    SEL isKindOfClassSel = sel_registerName("isKindOfClass:");
    BOOL (*msgSendBOOL)(id, SEL, Class) = (BOOL (*)(id, SEL, Class))objc_msgSend;
    
    if (!msgSendBOOL(layer, isKindOfClassSel, caMetalLayerClass))
        return;

    // Set maximumDrawableCount = 2
    SEL setMaxDrawableSel = sel_registerName("setMaximumDrawableCount:");
    void (*msgSendSetInt)(id, SEL, uint32_t) = (void (*)(id, SEL, uint32_t))objc_msgSend;
    msgSendSetInt(layer, setMaxDrawableSel, 2);

    // Set displaySyncEnabled = YES (enable VSync at layer level)
    SEL setDisplaySyncSel = sel_registerName("setDisplaySyncEnabled:");
    void (*msgSendSetBool)(id, SEL, BOOL) = (void (*)(id, SEL, BOOL))objc_msgSend;
    msgSendSetBool(layer, setDisplaySyncSel, YES);

    fprintf(stderr, "BGFX_METAL_FIX: configured CAMetalLayer drawableCount=2 displaySync=YES\n");
}
#endif
```

## 测试验证

### 测试方法
```bash
# 启动 Scene 6（动画场景）
SOLAR2D_BACKEND=bgfx "/path/to/Corona Simulator" -no-console YES /path/to/tests/bgfx-demo

# 频闪检测
python3 tools/screenshot_analyze.py --flicker --count 20 --interval 0.02
```

### 当前测试结果
| 方案 | 画面状态 | 帧间差异 | 结论 |
|------|----------|----------|------|
| 无修复 | 正常 | 54.2% | 基线 |
| v1 (带 presentsWithTransaction) | 黑屏 | - | ❌ 不可行 |
| v2 (仅 drawableCount=2) | 正常 | 54.2% | ❌ 无改善 |
| v3 (+ displaySyncEnabled) | 正常 | 26.3% | ⚠️ 部分改善 |

## 结论

CAMetalLayer 配置方案**未能完全解决**频闪问题。

- `maximumDrawableCount = 2` 单独使用无效
- `presentsWithTransaction = YES` 导致黑屏（与 bgfx 内部实现冲突）
- `displaySyncEnabled = YES` 有一定效果（54.2% → 26.3%），但未根除

## 后续建议

1. **CVDisplayLink 方案**: 考虑实现 CVDisplayLink 替代 NSTimer 驱动渲染循环，这是最有可能根治的方案
2. **bgfx 内部修改**: 可能需要修改 bgfx 的 Metal 后端，确保与 Solar2D 的 present 时机同步
3. **其他 layer 配置**: 尝试 `allowsNextDrawableTimeout` 等其他 CAMetalLayer 属性

## 尝试 4: bgfx 单线程模式 (根治方案) ✅

**配置**:
```cpp
// external/bgfx/src/config.h
#ifndef BGFX_CONFIG_MULTITHREADED
#	define BGFX_CONFIG_MULTITHREADED 0  // 原为: (0 == BX_PLATFORM_EMSCRIPTEN) ? 1 : 0
#endif // BGFX_CONFIG_MULTITHREADED
```

**原理**:
- bgfx 默认多线程模式下 `frame()` 异步非阻塞，与 Solar2D 的同步双缓冲架构时序不匹配
- 单线程模式下 `frame()` 立即执行渲染并阻塞直到呈现完成，与 GL 的 `flushBuffer` 行为一致

**结果**: 
- Scene 6 (动画): **0% 频闪** (从 54.2% 降至 0%)
- Scene 1 (静态): **0% 频闪** (无回归)
- 画面正常，无黑屏 ✅

## 最终结论

**bgfx 单线程模式完全解决频闪问题**。

单线程模式消除了 bgfx 内部 render 线程与 Solar2D 主线程的时序错乱，使渲染恢复同步语义。

## 相关提交

- Commit: `f2df8a7` Fix bgfx Metal flickering on macOS (CAMetalLayer 方案, 已 revert)
- Commit: (当前) bgfx: Set single-threaded mode (BGFX_CONFIG_MULTITHREADED=0)
