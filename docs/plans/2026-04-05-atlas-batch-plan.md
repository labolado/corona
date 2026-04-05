# Texture Atlas + Sprite Batch 实现计划

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan.

**Goal:** 实现 `graphics.newAtlas()` 和 `display.newBatch()` 两个新 Lua API，让开发者用一行代码实现纹理合批，大幅减少 draw call。

**Architecture:** Atlas 在运行时将多张图片打包到一张大 texture，返回 Atlas 对象。Batch 利用 Atlas 的共享 texture，将多个 sprite 的顶点数据合并到一个 transient buffer，一次 draw call 绘制所有 sprite。两者都基于现有的 TextureResource 和 DisplayObject 架构扩展。

**Tech Stack:** C++ (Solar2D engine core), Lua (API layer), bgfx (GPU rendering)

---

## 文件结构

### 新建文件
| 文件 | 职责 |
|------|------|
| `librtt/Display/Rtt_TextureAtlas.h/.cpp` | Atlas 核心：图片打包、帧查找、缓存管理 |
| `librtt/Display/Rtt_TextureAtlas_Lua.h/.cpp` | Atlas Lua 绑定：方法注册、参数解析 |
| `librtt/Display/Rtt_BatchObject.h/.cpp` | Batch DisplayObject：slot 管理、合并顶点 |
| `librtt/Display/Rtt_BatchObject_Lua.h/.cpp` | Batch Lua 绑定：add/remove/count |
| `tests/bgfx-demo/test_atlas.lua` | Atlas 功能测试 |
| `tests/bgfx-demo/test_batch.lua` | Batch 功能+性能测试 |

### 修改文件
| 文件 | 修改内容 |
|------|----------|
| `librtt/Display/Rtt_LuaLibGraphics.cpp` | 注册 `graphics.newAtlas()` |
| `librtt/Display/Rtt_LuaLibDisplay.cpp` | 注册 `display.newBatch()`，扩展 `display.newImage()` 支持 atlas 参数 |
| `platform/mac/ratatouille.xcodeproj/project.pbxproj` | 添加新文件到 Xcode 项目 |

---

## Chunk 1: Texture Atlas

### Task 1: TextureAtlas 核心类

**Files:**
- Create: `librtt/Display/Rtt_TextureAtlas.h`
- Create: `librtt/Display/Rtt_TextureAtlas.cpp`

**设计：**
```cpp
class TextureAtlas {
public:
    struct Frame {
        const char* name;       // "hero.png"
        S32 x, y, w, h;        // 在 atlas texture 中的位置
        Real u0, v0, u1, v1;   // UV 坐标
        S32 srcW, srcH;        // 原始图片尺寸
    };

    // 工厂方法
    static TextureAtlas* Create(
        Rtt_Allocator* allocator,
        Runtime& runtime,
        const char** imageNames,
        int numImages,
        int maxSize = 2048,
        int padding = 1
    );

    // 查询
    const Frame* GetFrame(const char* name) const;
    bool HasFrame(const char* name) const;
    int GetFrameCount() const;
    SharedPtr<TextureResource> GetTextureResource() const;

    // ImageSheet 兼容
    ImageSheet* CreateImageSheet(
        lua_State* L,
        const char* name,
        int frameWidth, int frameHeight, int numFrames
    );

    void Destroy();

private:
    // 矩形打包算法（shelf packing）
    bool PackRects(PlatformBitmap** bitmaps, int count);

    SharedPtr<TextureResource> fTexture;  // 合并后的大 texture
    Array<Frame> fFrames;                  // 帧列表
    // name → frame index 的哈希表
};
```

- [ ] Step 1: 创建头文件 Rtt_TextureAtlas.h，定义 Frame 结构和类接口
- [ ] Step 2: 实现矩形打包算法（shelf-based，简单高效）
- [ ] Step 3: 实现 Create()：加载多张图片 → 打包到一张 PlatformBitmap → 创建 TextureResource
- [ ] Step 4: 实现 GetFrame() 和 HasFrame()（名字哈希查找）
- [ ] Step 5: 实现 CreateImageSheet()（从 atlas 帧生成 ImageSheet）
- [ ] Step 6: 编译通过
- [ ] Step 7: Commit

### Task 2: TextureAtlas Lua 绑定

**Files:**
- Create: `librtt/Display/Rtt_TextureAtlas_Lua.h`
- Create: `librtt/Display/Rtt_TextureAtlas_Lua.cpp`
- Modify: `librtt/Display/Rtt_LuaLibGraphics.cpp` — 注册 graphics.newAtlas

**Lua 方法表：**
```cpp
static const luaL_Reg kAtlasMethods[] = {
    { "getFrame", getFrame },
    { "getImageSheet", getImageSheet },
    { "has", has },
    { "list", list },
    { "reload", reload },
    { "removeSelf", removeSelf },
    { "__gc", gc },
    { NULL, NULL }
};
```

- [ ] Step 1: 创建 Lua userdata 包装（参考 ImageSheetUserdata 模式）
- [ ] Step 2: 实现 `graphics.newAtlas()` 参数解析（简单数组或高级表）
- [ ] Step 3: 实现各方法的 Lua 绑定
- [ ] Step 4: 在 Rtt_LuaLibGraphics.cpp kVTable 中注册 `{ "newAtlas", newAtlas }`
- [ ] Step 5: 编译通过
- [ ] Step 6: Commit

### Task 3: display.newImage 扩展

**Files:**
- Modify: `librtt/Display/Rtt_LuaLibDisplay.cpp` — newImage 函数

**逻辑：**
```cpp
// 在 newImage 函数开头，检查第一个参数是否为 Atlas userdata
if (TextureAtlasUserdata::IsAtlas(L, 1)) {
    const char* frameName = luaL_checkstring(L, 2);
    TextureAtlas* atlas = TextureAtlasUserdata::ToAtlas(L, 1);
    const TextureAtlas::Frame* frame = atlas->GetFrame(frameName);
    // 用 frame 的 UV 和 atlas 的 texture 创建 RectObject
    // 类似 ImageSheet 路径
}
```

- [ ] Step 1: 在 newImage 开头加 Atlas 类型检测分支
- [ ] Step 2: 实现 Atlas frame → BitmapPaint 的转换
- [ ] Step 3: 支持 x,y 位置参数和 opts 属性表
- [ ] Step 4: 编译通过
- [ ] Step 5: Commit

### Task 4: Atlas 测试

**Files:**
- Create: `tests/bgfx-demo/test_atlas.lua`

```lua
-- test_atlas.lua
display.setStatusBar(display.HiddenStatusBar)
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Atlas Test (" .. backend .. ") ===")

-- 需要准备测试图片（用 snapshot 生成）
-- 创建几张程序化小图保存到临时目录
-- 然后用 graphics.newAtlas 加载

-- Test 1: 创建 atlas
-- Test 2: display.newImage(atlas, name)
-- Test 3: atlas:has() / atlas:list()
-- Test 4: atlas:getFrame() UV 正确性
-- Test 5: atlas:getImageSheet() 兼容性
-- Test 6: atlas:removeSelf()
-- Test 7: 错误处理（missing file）
```

- [ ] Step 1: 创建测试用的程序化图片（用 snapshot 生成 PNG）
- [ ] Step 2: 编写 atlas 创建和查询测试
- [ ] Step 3: 编写 display.newImage(atlas, name) 渲染测试
- [ ] Step 4: 编写错误处理测试
- [ ] Step 5: 运行验证 GL 和 bgfx 都通过
- [ ] Step 6: Commit

---

## Chunk 2: Sprite Batch

### Task 5: BatchObject 核心类

**Files:**
- Create: `librtt/Display/Rtt_BatchObject.h`
- Create: `librtt/Display/Rtt_BatchObject.cpp`

**设计：**
```cpp
class BatchObject : public DisplayObject {
public:
    struct Slot {
        S32 frameIndex;     // atlas 中的帧索引
        Real x, y;          // 位置
        Real scaleX, scaleY;
        Real rotation;
        Real alpha;
        bool isVisible;
        bool isDirty;       // 需要更新顶点
    };

    static BatchObject* New(
        Rtt_Allocator* allocator,
        TextureAtlas* atlas,
        int initialCapacity
    );

    // Slot 管理
    int AddSlot(int frameIndex, Real x, Real y);  // 返回 slot ID
    void RemoveSlot(int slotId);
    Slot* GetSlot(int slotId);
    int GetCount() const;
    void Clear();

    // DisplayObject 覆写
    virtual void Draw(Renderer& renderer) const override;
    virtual void GetSelfBounds(Rect& rect) const override;

private:
    TextureAtlas* fAtlas;
    Array<Slot> fSlots;
    int fCapacity;
    mutable bool fVerticesDirty;

    // 生成合并顶点数据
    void RebuildVertices() const;
    mutable Array<Vertex2D> fVertices;  // 合并后的顶点（6 per slot: 2 triangles）
};
```

- [ ] Step 1: 创建头文件，定义 Slot 和 BatchObject
- [ ] Step 2: 实现 New()、AddSlot()、RemoveSlot()、Clear()
- [ ] Step 3: 实现 RebuildVertices()：遍历 slots，为每个生成 6 个顶点（2 三角形），UV 从 atlas frame 获取
- [ ] Step 4: 实现 Draw()：用 transient buffer 提交合并顶点，1 个 draw call
- [ ] Step 5: 实现 GetSelfBounds()
- [ ] Step 6: 实现自动扩容（超出 capacity 时翻倍）
- [ ] Step 7: 编译通过
- [ ] Step 8: Commit

### Task 6: BatchObject Lua 绑定

**Files:**
- Create: `librtt/Display/Rtt_BatchObject_Lua.h`
- Create: `librtt/Display/Rtt_BatchObject_Lua.cpp`
- Modify: `librtt/Display/Rtt_LuaLibDisplay.cpp` — 注册 display.newBatch

**Lua 方法表：**
```cpp
static const luaL_Reg kBatchMethods[] = {
    { "add", add },         // batch:add("hero.png", x, y, opts) → slotProxy
    { "clear", clear },
    { "count", count },
    { NULL, NULL }
};

// SlotProxy 方法（batch:add 返回的 slot 引用）
static const luaL_Reg kSlotMethods[] = {
    { "remove", slotRemove },
    { "__index", slotGetProperty },    // slot.x, slot.y, slot.rotation
    { "__newindex", slotSetProperty },  // slot.x = 100
    { NULL, NULL }
};
```

- [ ] Step 1: 创建 BatchObject proxy vtable（参考 SpriteObject_Lua）
- [ ] Step 2: 实现 `display.newBatch(atlas, capacity)` 注册和参数解析
- [ ] Step 3: 实现 `batch:add(name, x, y, opts)` — 返回 SlotProxy userdata
- [ ] Step 4: 实现 SlotProxy 的属性读写（x, y, rotation, scaleX, alpha）
- [ ] Step 5: 实现 `slot:remove()`
- [ ] Step 6: 实现 `batch:clear()` 和 `batch:count()`
- [ ] Step 7: 编译通过
- [ ] Step 8: Commit

### Task 7: Batch 测试

**Files:**
- Create: `tests/bgfx-demo/test_batch.lua`

```lua
-- test_batch.lua — 功能 + 性能测试
-- Test 1: 创建 batch，add 100 个 sprite，验证 count
-- Test 2: 修改 slot 属性（x, y, rotation），验证渲染
-- Test 3: remove slot，验证 count 减少
-- Test 4: clear，验证 count=0
-- Test 5: 自动扩容（add 超过 capacity）
-- Test 6: 性能对比 — 1000 个独立 newImage vs 1000 个 batch:add
```

- [ ] Step 1: 编写基本功能测试（add/remove/clear/count）
- [ ] Step 2: 编写渲染正确性测试（截图对比）
- [ ] Step 3: 编写性能对比测试（独立 image vs batch，输出 FPS 和 draw call 数）
- [ ] Step 4: 运行验证
- [ ] Step 5: Commit

---

## Chunk 3: 集成与 Xcode 项目

### Task 8: Xcode 项目配置

**Files:**
- Modify: `platform/mac/ratatouille.xcodeproj/project.pbxproj`

- [ ] Step 1: 将 6 个新文件添加到 rttplayer target 的 Compile Sources
- [ ] Step 2: 编译验证 Debug + Release
- [ ] Step 3: Commit

### Task 9: 全量回归测试

- [ ] Step 1: `SOLAR2D_TEST=regression` GL + bgfx 全部 PASS
- [ ] Step 2: `SOLAR2D_TEST=atlas` 验证 atlas 功能
- [ ] Step 3: `SOLAR2D_TEST=batch` 验证 batch 功能和性能
- [ ] Step 4: `SOLAR2D_TEST=bench` 性能基准无退化
- [ ] Step 5: 最终 commit + push

---

## 工作分配建议

| Task | 分配 | 原因 |
|------|------|------|
| Task 1-3 (Atlas C++) | Claude | 需要理解 TextureResource 架构 |
| Task 4 (Atlas 测试) | Kimi | 机械编写 Lua 测试代码 |
| Task 5-6 (Batch C++) | Claude | 需要理解 DisplayObject 渲染管线 |
| Task 7 (Batch 测试) | Kimi | 机械编写 Lua 测试代码 |
| Task 8 (Xcode 配置) | Kimi | 机械修改项目文件 |
| Task 9 (回归测试) | 脚本自动 | run_all_tests.sh |

---
*2026-04-05*
