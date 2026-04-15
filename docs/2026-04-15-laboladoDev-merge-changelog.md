# laboladoDev Merge Changelog — Merged into bgfx-solar2d on 2026-04-15

## Merge Summary

- **Merge commit**: `d57892d7`
- **Source branch**: `origin/laboladoDev`
- **Target branch**: `bgfx-solar2d`
- **Merge date**: 2026-04-15
- **Unique commits merged**: 40
- **Note on deduplication**: The laboladoDev branch underwent a rebase ("ugh" rebase) at some point. The 40 commits listed here are the deduplicated canonical set after that rebase — earlier duplicate SHAs from before the rebase are excluded.

---

## Solar2D 上游贡献（Solar2D Core Team & Contributors）

### Scott Harrison (scottrules44@gmail.com) — Solar2D Core Maintainer

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-06-06 | `8774c8b5` | #812 | Maintenance | GLTexture 代码清理与重构 |
| 2025-07-16 | `664d2724` | #826 | Android: Maintenance | Android SDK 升级，CoronaActivity 改进，runtime lifecycle 更新（8 文件）|
| 2025-07-30 | `3f8d2caf` | #828 | Android: NavBar Fixes | 修复 Android 导航栏行为异常 |
| 2025-09-20 | `e6ff87cd` | #839 | Apple: iOS and MacOS 26 support | 支持 Xcode 26 / iOS 26 / macOS 26 构建 |
| 2025-09-21 | `0a281f6a` | #840 | iOS: Maintenance | iOS 维护，构建配置更新（4 文件）|
| 2025-09-24 | `ff6dfb29` | #845 | iOS/TvOS: disable beta-reports-active by default | 默认关闭 beta-reports-active 设置 |
| 2025-10-20 | `fe82c21a` | #851 | MacOS: Maintenance | Mac 原生构建脚本改进 |
| 2025-10-24 | `c866ec5b` | #861 | iOS/tvOS: Logging fixes | 修复 iOS/tvOS syslog 脚本 |
| 2025-10-24 | `4914f7d9` | #856 | Mac Sim: Fixes for Search on Console | 改进模拟器控制台搜索功能 |
| 2025-11-04 | `f09b8144` | #862 | HTML: Fixes to Custom Font Positioning | 修复 HTML5 构建中自定义字体定位问题（2 文件）|
| 2025-11-23 | `ad84ed76` | #872 | MacOS: Fixes for mono audio | 修复 macOS 单声道音频播放 |
| 2025-12-01 | `dbf42bc2` | #871 | HTML5: Fixes for Audio Pausing/Stopping | 大幅重写 Web Audio 播放器，修复音频暂停/停止 |
| 2025-12-02 | `ff34cdee` | #873 | Apple: Xcode 26.1 | 将 Xcode 26.1 加入 CI 构建矩阵 |
| 2026-01-03 | `51a347cb` | #874 | Native: Add support for Arm Based Sims for iOS | CoronaBuilder 支持 arm64 iOS 模拟器（9 文件）|

### Vlad Svoka / Vasyl Shcherban (Shchvova) — Solar2D Core Maintainer

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-06-30 | `2e181229` | #817 | Core/Physics: fixing Wheel Joint | 修复 wheel joint 参数处理（2 文件）|
| 2025-08-12 | `1c077630` | #830 | Maintenance | 更新证书和 provisioning profiles（4 文件）|
| 2025-08-24 | `fb4b583b` | #831 | Fixing windows build | 修复 toolset 迁移后的 Windows 构建（27 文件）|
| 2025-09-21 | `8eac16d3` | — | Maintenance | 更新 Box2D submodule，包含 b2FakeJoint |
| 2025-10-23 | `710a715e` | #857 | Android: fix for non arm64 platforms | 修复 x86/x86_64/armeabi-v7a Android 构建（15 文件）|

### Vasyl Shcherban (独立条目)

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-08-12 | `f1aeef4e` | #125 | Maintenance | Windows toolset 迁移至 v141，SDK 10.0.18362.0（28 文件）|

### Kan (kan6868) — Solar2D Contributor

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-06-06 | `1dec8e7b` | #803 | Core: Fixing unpack with alignment | 修复多种 GL 纹理格式的 pixel unpack alignment（1 文件）|
| 2025-06-30 | `e65e3198` | #815 | Linux: Maintenance | LinuxContainer.h 补充缺失 include（1 文件）|
| 2025-06-30 | `5f55ab8e` | #816 | Linux/Simulator: Fix bad behavior when changing skin | 修复模拟器切换 skin 时的异常行为（1 文件）|
| 2025-06-30 | `4c1f709c` | #818 | HTML5: Improved scaling, more memory allocated | 改进 scaling，增加 WASM 内存分配（3 文件）|
| 2025-07-06 | `e79d911d` | #819 | HTML5: Enable Vsync, optimize CSS for mobile | 启用 Vsync，优化移动端 CSS（4 文件）|
| 2025-07-06 | `3e0e63ae` | #814 | Core/Physics: Add x, y offset to circle physics body | circle 物理体支持 x/y offset 参数（1 文件）|
| 2025-07-09 | `4d9a8aaf` | #821 | HTML5: LetterBox portrait fullscreen, web build settings | 竖屏 letterbox 全屏，新增 web build 设置（2 文件）|
| 2025-09-21 | `27c48fdc` | #827 | Physics/Core: Raycast works with the maskbits | Raycast 现在正确遵守 collision maskBits（1 文件）|
| 2025-11-24 | `9e6813a2` | #852 | HTML5: Maintenance | HTML5 context 和平台层改进（2 文件）|

### Jeremy (clang-clang-clang) — Solar2D Contributor

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-10-24 | `c8c3bd1e` | #858 | Core: fix box2d joint use after free when world deleted | 修复 Box2D world 销毁时活跃 joint 引发的 UAF 崩溃（1 文件）|

### vikramvicky13 — Solar2D Contributor (Linux)

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-09-17 | `fde680a1` | #834 | Linux: maintenance | Linux 模拟器小修（1 文件）|
| 2025-09-20 | `3ac87805` | #837 | Linux/Sim: Rotate left and right added | Linux 模拟器新增旋转支持（3 文件）|
| 2025-09-22 | `5bec6eba` | #842 | Linux: Open project from command line | 支持从命令行打开 Solar2D 项目（1 文件）|
| 2025-09-23 | `4bd316e9` | #846 | Linux/Sim: check supported orientation | Linux 模拟器中添加方向验证（2 文件）|

### Denis Claros — Solar2D Contributor

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-10-16 | `3b738fa8` | #850 | iOS/tvOS: packaging | 简化 iOS/tvOS 打包脚本（2 文件）|

### Usman Mughal — Solar2D Contributor

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2026-01-03 | `7dedd167` | #864 | iOS: system gesture deferral for preferredScreenEdge | 正确处理边缘滑动系统手势（3 文件）|

---

## labolado / zero-meta 团队贡献

| 日期 | Hash | PR | Subject | 说明 |
|------|------|----|---------|------|
| 2025-07-11 | `32a11782` | #825 | Android: Maintenance | Android CMakeLists 清理（1 文件）|
| 2025-09-30 | `4aefa0a5` | — | Maintenance | 更新 enterprise submodule（1 文件）|
| 2025-12-10 | `4e61c8a7` | — | Support API 'display.save' WebP on Android | Android 上 display.save 新增 WebP 格式支持（8 文件）|
| 2026-03-25 | `c7134c2d` | — | feat: add liveBuild parameter support to CoronaBuilder CLI | CoronaBuilder CLI 支持 liveBuild 参数（1 文件）|

---

## 统计

| 贡献者 | Commits | 主要领域 |
|--------|---------|---------|
| Scott Harrison | 14 | iOS, macOS, Android, HTML5 |
| Kan | 9 | HTML5, Physics, Linux |
| Vlad Svoka / Vasyl Shcherban | 6 | Physics, Windows, Android, Box2D |
| vikramvicky13 | 4 | Linux |
| labolado / zero-meta / LaboLado / Chen Bin | 4 | Android, enterprise, CoronaBuilder |
| Jeremy | 1 | Physics/Core |
| Denis Claros | 1 | iOS/tvOS |
| Usman Mughal | 1 | iOS |
| **总计** | **40** | |
