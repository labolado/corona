#!/bin/bash
# 日志分析工具 — 从模拟器控制台输出中提取关键信息
#
# 用法:
#   bash tools/log_analyze.sh [日志文件]
#   cat simulator.log | bash tools/log_analyze.sh
#
# 功能:
#   - 统计 ERROR/WARNING 数量
#   - 提取 bgfx / GL / 渲染 相关日志
#   - 统计帧率（从 Frame N 日志）
#   - 提取 Lua 错误

set -e

INPUT="${1:--}"
if [ "$INPUT" = "-" ]; then
    LOG=$(cat)
else
    LOG=$(cat "$INPUT")
fi

echo "========== 日志分析结果 =========="
echo

# 1. ERROR / WARNING 统计
ERROR_COUNT=$(echo "$LOG" | grep -ci "ERROR" || true)
WARN_COUNT=$(echo "$LOG" | grep -ci "WARNING" || true)

echo "【错误与警告统计】"
echo "  ERROR 数量:   $ERROR_COUNT"
echo "  WARNING 数量: $WARN_COUNT"
echo

# 2. bgfx / GL / 渲染 相关日志
BGFX_LINES=$(echo "$LOG" | grep -iE "bgfx|GL_|Shader|Render|Texture|Draw|BIND|VERT" || true)
BGFX_COUNT=$(echo "$BGFX_LINES" | grep -c "." || true)

echo "【渲染相关日志】（共 ${BGFX_COUNT} 条）"
if [ -n "$BGFX_LINES" ]; then
    echo "$BGFX_LINES" | tail -n 20 | sed 's/^/  /'
    if [ "$BGFX_COUNT" -gt 20 ]; then
        echo "  ... 省略前 $((BGFX_COUNT - 20)) 条"
    fi
else
    echo "  未找到渲染相关日志"
fi
echo

# 3. 帧率统计
FRAME_COUNT=$(echo "$LOG" | grep -cE "Frame [0-9]+|fps|[0-9]+\s*fps" || true)

echo "【帧率信息】"
if [ "$FRAME_COUNT" -gt 0 ]; then
    echo "  帧相关日志: ${FRAME_COUNT} 条"
    echo "$LOG" | grep -E "Frame [0-9]+|fps|[0-9]+\s*fps" | sed 's/^/  /'
else
    echo "  未找到帧率相关日志"
fi
echo

# 4. Lua 错误
LUA_ERROR=$(echo "$LOG" | grep -iE "stack traceback:|attempt to|lua error|runtime error|bad argument|expected" || true)
LUA_ERR_COUNT=$(echo "$LUA_ERROR" | grep -c "." || true)

echo "【Lua 错误】"
if [ -n "$LUA_ERROR" ]; then
    echo "  发现 ${LUA_ERR_COUNT} 处 Lua 异常:"
    echo "$LUA_ERROR" | sed 's/^/  /'
else
    echo "  未检测到 Lua 错误"
fi
echo

echo "========== 分析结束 =========="
