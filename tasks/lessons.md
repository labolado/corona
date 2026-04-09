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

### Worker 调 gemma4-ask 不要包 timeout 脚本
**日期**: 2026-04-06
**场景**: Kimi worker 调 gemma4-ask 做截图对比，用 `wait $pid` 包了一层超时逻辑，两个进程卡住互相等，死锁
**根因**: gemma4-ask 内部已有超时处理，外面再包一层 timeout/wait 会导致进程残留和死锁
**修复**: 清掉卡住的进程（`kill`），重新直接调用
**教训**:
- 派活时明确告诉 worker：直接调 `bash gemma4-ask.sh`，不要包 timeout、不要用 subprocess 包装
- gemma4-ask 超时就直接再执行一次同样的命令，不要换分析方式
- gemma4 服务器是 nohup 后台进程，正常不会停，首次调用可能慢（模型加载），重试即可

### gemma4-ask 在非终端环境阻塞（根因：stdin cat）
**日期**: 2026-04-06
**场景**: Kimi worker 调 gemma4-ask 每次都卡住，几分钟无响应
**根因**: 脚本第 76 行 `if [ ! -t 0 ]` 检测到非终端 stdin → `cat` 读 stdin 永远阻塞。Kimi 的 shell 执行环境 stdin 不是 tty 但也没有管道数据
**修复**: 加条件 `&& [ $# -eq 0 ]`，有参数（图片文件）时跳过 stdin 读取。同时修复 Argument list too long（base64 改用 stdin + 临时文件传递）
**教训**: 供 agent 使用的脚本必须考虑非交互式 shell 环境，`[ ! -t 0 ]` 检测不可靠

### Worker 截图必须用标准命令，不要改写
**日期**: 2026-04-06
**场景**: Worker 每次截图都自己"改进"Python 脚本，导致变量名错、路径错、截图失败
**教训**:
- 派活时给完整的截图 shell 命令，worker 只需替换输出文件名
- 不允许 worker 改写截图脚本逻辑
- 标准命令见 CLAUDE.md 的「截图方法」章节

### Kimi worker SAME 汇报不可信（再次验证）
- **场景**：Kimi worker 汇报 test_compare.sh 结果 "SAME（像素完全一致）"
- **实际**：bgfx 渲染仍然严重错乱，GL vs bgfx 差异巨大
- **根因**：Worker 可能没有正确运行测试，或对比了错误的文件
- **教训**：Kimi 的"修复完成"和"测试通过"一律不信。Coordinator 必须自己验证

### tank_test_copy 必须从 src/main 复制
- **场景**：从 labo_tank 根目录复制，测试结果 SAME（假阳性）
- **根因**：Solar2D 项目入口是 src/main/main.lua，根目录没有有效的 main.lua
- **正确路径**：`cp -a /Users/yee/data/dev/app/labo_tank/src/main /tmp/tank_test_copy`
- **教训**：每次重建 tank_test_copy 要用正确路径，并 git init 防止被改坏无法恢复

### bgfx tank 渲染 bug 排除的方向
- samplerFlags 缺失：已修复，但不是根因（修复后仍然错乱）
- index buffer 丢失：排除（index data 始终有效）
- MVP 全零：排除（是 printf 精度问题，实际值非零）
- indexed draw 本身：排除（跳过 ExecuteDrawIndexed 仍然错乱）
- tiling shader：排除（注释掉仍然错乱）
- Lua 文件被改坏：排除（干净副本仍然错乱）
- **未排除方向**：behind[3] 的 display.newMesh + display.fillWithTexture 的 C++ 创建路径对全局渲染状态的影响

### bgfx indexed mesh 导致后续文字消失 bug（已定位，待修复）
- **场景**：任何包含 `display.newMesh{ mode="indexed" }` 的场景
- **现象**：mesh 本身渲染正确（红色方块），但 mesh 之后创建的所有文字（display.newText）完全消失
- **最简复现**：/tmp/test_mesh_project/main.lua（背景+mesh+两行文字，文字全消失）
- **排除**：transient VB 有效（hasVB=1）、MVP 正确、samplerFlags 已修复
- **关键线索**：即使用 bgfx::discard() 替代 submit()，后续文字仍消失 → 问题在 ExecuteDrawIndexed 的 state 设置阶段，不在 submit
- **根因方向**：indexed mesh 的 storedOnGPU geometry 的 Insert/FlushBatch 流程影响了后续 pool geometry 的 batch 或 texture binding
- **测试项目**：/tmp/test_mesh_project（git init 过，可追踪修改）

### bgfx setViewClear 在错误 view ID 上设置（2026-04-09）
- **场景**：坦克 logo 背景黑色，应该是白色
- **根因**：Initialize() 先调 setViewClear(fDefaultView=0)，然后 InitializeFBO() 把 fDefaultView 改成 200。view 200 从未设 clear color → 默认黑
- **修复**：把 setViewClear 移到 InitializeFBO 之后
- **教训**：bgfx view 状态是 per-view 的，改了 view ID 后之前设的状态不会跟过来

### bgfx shader binary interface hash 必须匹配（2026-04-09）
- **场景**：runtime 构造的 shader binary 编译成功但 createProgram 返回 INVALID_HANDLE
- **根因**：bgfx 验证 VS.hashOut == FS.hashIn（bgfx_p.h:5025），runtime 构造的 FS 用 hashIn=0 但预编译 VS 的 hashOut=0x6258d9fe
- **修复**：从预编译 VS binary 读取 hashOut，传给 FS 的 hashIn
- **教训**：bgfx createProgram 静默失败（Release 构建下 BX_TRACE 被 strip），需要自己加详细日志

### defineEffect 在 Android/iOS 上不工作是刚性问题（2026-04-09）
- **场景**：坦克 50+ 自定义 shader 全部回退到默认 shader
- **根因**：BgfxShaderCompiler 依赖外部 shaderc 二进制（只在 macOS 存在），Android/iOS 无法运行时编译
- **修复**：C++ 直接构造 bgfx shader binary（header + uniform + ESSL 源码），绕过 shaderc
- **教训**：bgfx shader binary 格式简单（Magic+Hash+Uniforms+源码），GLES 后端直接取源码调 glCompileShader，不需要 shaderc

### 测试盲区导致 bug 长期隐藏（2026-04-09）
- **场景**：defineEffect 问题从未被发现
- **根因**：默认 10 场景没有一个用 defineEffect；Android 模拟器 swiftshader 全黑掩盖了所有问题；Android 真机只跑了 10 场景
- **修复**：把 custom_shader 测试加入默认 11 场景；Android 真机必须跑完整 26 场景测试
- **教训**：测试通过不代表功能正常——要检查测试是否覆盖了目标功能

### Worker 代码在主 repo 被 coordinator 覆盖（2026-04-09）
- **场景**：shader worker 写了 390 行代码，coordinator 切分支 git stash/pop 全部丢失
- **修复**：Worker 写代码必须用 git worktree 隔离
- **教训**：已加入 CLAUDE.md 铁律。纯分析不需要 worktree，写代码必须用

### bgfx Release 构建吞错误日志（2026-04-09）
- **场景**：program link 失败但看不到原因
- **根因**：bgfx 用 BX_TRACE 输出 link error，Release 构建下被 strip
- **修复**：在 Rtt_BgfxProgram.cpp 加 Rtt_LogException 详细日志（effect 名、handle 状态）
- **教训**：不要依赖 bgfx 内部的错误输出，在我们的代码层加独立日志
