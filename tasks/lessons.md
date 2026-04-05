# 经验教训记录

每次踩坑后更新此文件。新会话开始时回顾。

---

## bgfx 渲染迁移

### Lua::CheckUserdata 对自定义 metatable 不兼容
**日期**: 2026-04-05
**场景**: Atlas Lua 绑定的 has()/getFrame()/removeSelf() 全部崩溃
**根因**: `Lua::CheckUserdata` 是 Solar2D 包装，对自定义注册的 metatable（如 TextureAtlas）不兼容，返回 NULL
**修复**: 直接用 Lua 标准 API `luaL_checkudata` / `lua_touserdata`
**教训**: 新建 Lua userdata 绑定时，优先用标准 Lua API，不用 Solar2D 包装函数

### bgfx FBO Y-axis 翻转
**日期**: 2026-04-04
**场景**: bgfx 模式 masks 场景 "RTT" 文字上下颠倒
**根因**: GL framebuffer origin 在左下，bgfx/Metal origin 在左上
**修复**: 检查 `renderer.GetCaps().originBottomLeft`，为 false 时交换 ortho yMin/yMax
**教训**: 所有 FBO 渲染（Snapshot, FrameBuffer）都要处理 Y-flip

### bgfx uniform 数据格式
**日期**: 2026-04-03
**场景**: mat3 uniform 传给 bgfx 后 shader 收到错误值
**根因**: bgfx setUniform mat3 必须用 9 float 紧凑格式，不能有 padding（GL 用 3x4 列 padding）
**修复**: 传 mat3 时检测并转为 9 float 紧凑格式
**教训**: bgfx uniform 格式严格，不同于 GL 的 padding 约定

### Composer "show" 事件不发给 Runtime
**日期**: 2026-04-04
**场景**: 导航栏在 scene 切换后消失
**根因**: `Runtime:addEventListener("show", ...)` 永远不触发，composer 只发给 scene 对象
**修复**: 用 `enterFrame` 持续 `navGroup:toFront()`
**教训**: Solar2D composer 事件只在 scene 内传播，不会到 Runtime

### NSTask 重启必须用 -project 参数
**日期**: 2026-04-04
**场景**: Cmd+R 重启后窗口不显示
**根因**: NSTask 传裸路径不触发 `application:openFile:`
**修复**: 用 `-project self.fAppPath` 参数
**教训**: macOS AppKit 的 NSTask 子进程不继承 open-file 事件

## Atlas + Batch 实现

### GetSelfBounds 必须考虑旋转
**日期**: 2026-04-05
**场景**: 旋转 45° 的 batch slot 超出 bounds，可能被错误裁剪
**根因**: AABB 计算用了未旋转的 halfW/halfH
**修复**: 用 `sqrt(halfW² + halfH²)` 计算包围半径
**教训**: 所有 bounds 计算都要考虑 rotation，不能只用 axis-aligned 尺寸

### DisplayObject removeSelf 后必须标记无效
**日期**: 2026-04-05
**场景**: batch:removeSelf() 后 Lua 仍可访问属性
**根因**: 没有在 removeSelf 时清除 userdata 中的 C++ 指针
**修复**: removeSelf 时将内部指针设 NULL，所有方法检查 NULL
**教训**: 自定义 DisplayObject 的 Lua 绑定必须处理生命周期结束后的访问

### 测试中的 pcall/skip 要及时清理
**日期**: 2026-04-05
**场景**: Bug 已修复但测试仍 skip，导致回归测试假阳性
**根因**: 写测试时 C++ 有 bug，用 pcall 包裹避免崩溃，修复后忘记去掉
**教训**: Bug 修复后必须同步去掉测试中的 skip/pcall 包裹

## Worker 协作

### Kimi 的修改必须 review
**日期**: 2026-04-05
**经验**: Kimi 能正确执行明确的机械任务（改代码、编译、测试），但深层分析不可靠
**教训**: Kimi 的每次 C++ 修改，coordinator 都要 `git show` review diff

### 模拟器测试必须看日志
**日期**: 2026-04-05
**场景**: Worker 运行模拟器后等截图，没注意模拟器已报错
**教训**: 派活时强制要求 `2>&1 | tee log && grep Error log`，日志是第一手证据

### 测试文件不能随意修改
**日期**: 2026-04-05
**经验**: 测试是验证标准，修 bug 时改测试 = 自欺欺人
**教训**: 修 C++ bug 的 worker 不许改测试文件；更新测试必须单独 commit

---
*持续更新*

### 回退路径必须有强制开关和测试
**日期**: 2026-04-05
**场景**: Instancing 实现了 getCaps 自动检测回退，但没有 runtime 强制关闭开关
**问题**: 在支持 instancing 的设备上无法测试回退路径
**教训**: 每个有降级的功能必须实现 `graphics.setXxx(false)` 强制开关，测试中开/关两条路径都跑
**遗留**: SDF 和 Instancing 都需要补回退路径强制测试
