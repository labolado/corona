# Input Recorder 使用文档

Touch 事件录制回放模块，用于在 Solar2D 引擎层自动录制和回放触摸事件。

---

## 功能特性

- **录制模式**: 自动拦截所有触摸事件并保存为 JSON 文件
- **回放模式**: 按时间线注入触摸事件，与真实触摸走相同路径
- **自动保存**: 每 10 秒自动保存一次，防止 kill/crash 丢失录制数据
- **跨平台**: Mac/Android/iOS 支持

---

## 触发方式

### Mac (环境变量)

```bash
# 录制模式
SOLAR2D_RECORD=1 ./Corona\ Simulator.app/Contents/MacOS/Corona\ Simulator

# 回放模式
SOLAR2D_REPLAY=rec_20250410_120000.json ./Corona\ Simulator.app/Contents/MacOS/Corona\ Simulator
```

### Android (触发文件)

```bash
# 创建录制触发文件
adb shell mkdir -p /sdcard/Android/data/<package>/files/recordings
adb push /dev/null /sdcard/Android/data/<package>/files/recordings/RECORD

# 启动 App 后会自动开始录制，触发文件会被自动删除

# 创建回放触发文件（文件内容为目标录制文件名）
echo "rec_20250410_120000.json" | adb shell cat > /sdcard/Android/data/<package>/files/recordings/REPLAY
```

### iOS (触发文件)

```bash
# 通过 Xcode 或 iTunes 文件共享
# 1. 在 Documents/recordings/ 目录下创建 RECORD 空文件（录制）
# 2. 或在 Documents/recordings/ 目录下创建 REPLAY 文件（文件内容为目标录制文件名）
```

---

## 录制文件位置

### Mac
```
~/Documents/recordings/rec_YYYYMMDD_HHMMSS.json
```

### Android
```
/sdcard/Android/data/<package>/files/recordings/rec_YYYYMMDD_HHMMSS.json
```

### iOS
```
App/Documents/recordings/rec_YYYYMMDD_HHMMSS.json
```

---

## 录制文件格式

```json
{
  "meta": {
    "version": 1,
    "platform": "macOS",
    "backend": "bgfxBackend",
    "screenWidth": 1280,
    "screenHeight": 720,
    "timestamp": "20250410_120000"
  },
  "events": [
    {
      "time": 123.456,
      "phase": "began",
      "x": 100.0,
      "y": 200.0,
      "id": 1
    },
    {
      "time": 234.567,
      "phase": "moved",
      "x": 105.0,
      "y": 205.0,
      "id": 1
    },
    {
      "time": 345.678,
      "phase": "ended",
      "x": 110.0,
      "y": 210.0,
      "id": 1
    }
  ]
}
```

### 字段说明

| 字段 | 说明 |
|------|------|
| `time` | 事件时间戳（毫秒，相对于录制开始） |
| `phase` | 触摸阶段：`began`, `moved`, `stationary`, `ended`, `cancelled` |
| `x` | 触摸 X 坐标（屏幕坐标系） |
| `y` | 触摸 Y 坐标（屏幕坐标系） |
| `id` | 触摸点 ID（支持多点触控） |

---

## 测试步骤

### 测试录制

1. **启动录制**
   ```bash
   cd /Users/yee/data/dev/app/labo/corona
   SOLAR2D_RECORD=1 ./Corona\ Simulator.app/Contents/MacOS/Corona\ Simulator samples/bgfx-demo
   ```

2. **进行触摸操作**
   - 在 App 界面上点击、拖动几下

3. **强制退出**
   - 按 `Cmd+Q` 或 `kill -9` 强制退出

4. **验证录制文件**
   ```bash
   ls ~/Documents/recordings/rec_*.json
   cat ~/Documents/recordings/rec_20250410_120000.json
   ```

### 测试回放

1. **准备录制文件**
   ```bash
   # 确认录制文件存在
   ls ~/Documents/recordings/rec_*.json
   ```

2. **启动回放**
   ```bash
   SOLAR2D_REPLAY=~/Documents/recordings/rec_20250410_120000.json \
     ./Corona\ Simulator.app/Contents/MacOS/Corona\ Simulator samples/bgfx-demo
   ```

3. **观察**
   - App 会自动执行之前录制的触摸操作
   - 控制台会输出 `InputRecorder: Playback finished` 表示回放完成

---

## 实现细节

### 自动保存机制

- 每 10 秒自动保存一次到同一个文件
- 文件在录制开始时创建，后续自动保存覆盖同一个文件
- 即使 kill -9 或 crash，最多丢失最近 10 秒的数据

### 事件分发路径

```
平台触摸事件 -> GLView -> Runtime::DispatchEvent() 
                                     |
                                     v
                            InputRecorder::RecordTouchEvent()
                                     |
                                     v
                            Runtime::DispatchEvent() -> Lua Runtime
```

回放时：
```
Runtime::WillDispatchFrameEvent() -> InputRecorder::Update()
                                             |
                                             v
                                    TouchEvent 注入 -> Runtime::DispatchEvent()
```

---

## 注意事项

1. **录制和回放使用相同的屏幕尺寸**，不同尺寸可能导致坐标不匹配
2. **回放期间禁用真实触摸**，避免冲突
3. **多点触控支持**，回放时会还原原始的触摸 ID
4. **Lua 层完全透明**，无需任何修改

---

## 故障排查

### 录制文件未生成
- 检查 Documents/recordings 目录是否存在写入权限
- 查看控制台日志 `InputRecorder:` 开头的信息

### 回放无反应
- 确认录制文件路径正确
- 检查录制文件的 JSON 格式是否有效
- 查看控制台日志确认回放是否启动

### 坐标偏移
- 录制和回放时使用相同的屏幕尺寸和方向
- 检查 content width/height 是否一致
