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

## Lesson: GL packed integer vs bgfx byte-wise texture format mismatch

**日期**: 2026-04-06
**场景**: Mac CoreGraphics outputs kCGImageAlphaPremultipliedFirst (ARGB component order), GL reads via GL_BGRA + GL_UNSIGNED_INT_8_8_8_8 (packed integer), bgfx uses byte-wise BGRA8
**问题**: GL的packed integer格式和bgfx的byte-wise格式对同一字节数组有不同解释。LE系统上bytes [A,R,G,B] 被GL正确解读为BGRA(通过32-bit整数反转)，但bgfx直接按字节读取导致通道错乱
**教训**: GL_UNSIGNED_INT_8_8_8_8是packed integer格式(component在32-bit整数中按MSB→LSB排列)，而Metal/bgfx/Vulkan都是byte-wise格式(byte[0]直接是第一个component)。在LE系统上两者差4字节反转。迁移GL代码到现代API时必须注意packed vs byte-wise格式差异
**修复**: BgfxTexture::Create/Update中对kBGRA格式做__builtin_bswap32逐像素字节反转

### 测试必须包含真实图片文件加载
**日期**: 2026-04-06
**场景**: bgfx 纹理渲染从迁移之初就白屏，但所有测试只用程序化形状，从未发现
**根因**: Mac CoreGraphics BGRA packed integer vs bgfx byte-wise 格式不匹配（bswap32 修复）
**教训**: 
- 测试不能只用程序化对象，必须包含真实文件加载（PNG/JPG）
- 用真实项目做集成验证（labo_tank 等）
- 测试覆盖盲区 = 生产 bug

### 不能在 worker 有未提交改动时杀掉
**日期**: 2026-04-06
**场景**: w-tex-fix2 在加诊断日志，有未提交改动，被 coordinator 直接 kill 重开新 worker
**问题**: 丢失工作进度，新 worker 不知道前一个做了什么
**教训**: 
- 杀 worker 前必须 `git diff --stat` 检查有无改动
- 有改动 → 催它继续，不杀
- 必须重启 → 先 `git stash` 保存，重启后 `git stash pop` 恢复
- 绝不直接 kill 正在工作的 worker

### 自定义 GLSL shader 在 bgfx 下静默失败 → 已解决
**日期**: 2026-04-06
**场景**: Tank 项目用 graphics.defineEffect 的 tiling shader，bgfx 下地面纹理极度放大
**根因**: BgfxProgram::LoadShaderBinary() 只查嵌入式 Metal 二进制表，自定义 effect 不在表中→静默回退默认 shader
**修复**: 实现运行时 GLSL→Metal 编译：
1. 在 NewShaderBuiltin 检测 bgfx 模式下的自定义 effect（name 含 `.`）
2. 用 BgfxShaderCompiler::TransformFragmentKernel 把 GLSL kernel 转为 bgfx .sc 格式
3. 调外部 shaderc 二进制编译 .sc → Metal binary（glslang→SPIR-V→spirv-cross→MSL）
4. 缓存编译结果，LoadShaderBinary 先查缓存
5. 编译失败→CORONA_LOG_ERROR 明确报错（不再静默）
**教训**:
- 静默失败最危险，必须有明确 ERROR 而非 warning
- shaderc 可作为外部进程使用，不需要链接为库
- GLSL kernel → bgfx .sc 转换的关键：替换 FragmentKernel→main、texCoord→v_TexCoord.xy、return→gl_FragColor
- Metal varying 按 `[[user(locnN)]]` 匹配不按名字，必须用同一个 varying.def.sc 保证 VS/FS 兼容
