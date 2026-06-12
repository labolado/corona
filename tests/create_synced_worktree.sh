#!/usr/bin/env bash
# create_synced_worktree.sh — 创建 worktree 并把 submodule 对齐到主仓库的「实际 checkout 状态」
#
# 背景（2026-06-12 教训，见 game_engine/tasks/lessons.md）：
#   1. 主仓库 submodule 实际 checkout 的 SHA 与 gitlink 记录不一致（bgfx 用 solar2d 分支，
#      gitlink 记录旧 master）。裸跑 `git submodule update --init` 会检出错误版本，
#      还会向 GitHub fetch 不存在/不需要的旧 SHA（浪费 10+ 分钟）。
#   2. 对未初始化的 submodule 目录跑 `git -C <dir>` 会向上命中父仓库，必须先验证
#      git-dir 归属再操作。
#
# 用法: bash tests/create_synced_worktree.sh <worktree_path> <new_branch> [base_ref]
#   例: bash tests/create_synced_worktree.sh /tmp/corona-opt-e1 feature/opt-e1-p0 bgfx-solar2d
set -uo pipefail

MAIN="$(cd "$(dirname "$0")/.." && pwd)"
WT="${1:?用法: create_synced_worktree.sh <worktree_path> <new_branch> [base_ref]}"
BRANCH="${2:?缺少分支名}"
BASE="${3:-bgfx-solar2d}"

[ -e "$WT" ] && { echo "ERROR: $WT 已存在"; exit 1; }

echo "[1/4] git worktree add $WT -b $BRANCH $BASE"
git -C "$MAIN" worktree add "$WT" -b "$BRANCH" "$BASE" || exit 1

echo "[2/4] submodule init（--reference 本地加速；忽略检出 SHA，下一步会纠正）"
git -C "$WT" submodule update --init --reference "$MAIN" 2>&1 | tail -3 || true

echo "[3/4] 对齐到主仓库实际 checkout 状态"
FAIL=0
while read -r p; do
    sha=$(git -C "$MAIN/$p" rev-parse HEAD 2>/dev/null) || { echo "  SKIP $p（主仓库未检出）"; continue; }
    # 安全检查：确认 git-dir 属于 submodule 自己，不是父仓库
    gd=$(git -C "$WT/$p" rev-parse --git-dir 2>/dev/null || echo "")
    case "$gd" in
        *"/modules/"*|*"$p"*) : ;;
        *) echo "  ERROR $p git-dir 异常（$gd），跳过以防误伤父仓库"; FAIL=1; continue ;;
    esac
    cur=$(git -C "$WT/$p" rev-parse HEAD 2>/dev/null || echo none)
    if [ "$cur" != "$sha" ]; then
        git -C "$WT/$p" fetch -q "$MAIN/$p" HEAD 2>/dev/null || true
        git -C "$WT/$p" checkout -qf "$sha" 2>/dev/null || { echo "  ERROR checkout $p $sha"; FAIL=1; continue; }
        echo "  SYNC $p ${cur:0:8} -> ${sha:0:8}"
    fi
    # 工作树空检查（update 被中断/只注册未检出的情况）
    n=$(/bin/ls -A "$WT/$p" 2>/dev/null | grep -cv '^\.git$')
    [ "$n" -le 1 ] && { git -C "$WT/$p" checkout -f HEAD >/dev/null 2>&1; echo "  REPOPULATE $p（工作树为空）"; }
done < <(git -C "$MAIN" submodule status | awk '{print $2}')

echo "[4/4] 校验：SHA 与主仓库一致 + 工作树非空"
if diff <(git -C "$MAIN" submodule status | awk '{sub(/^[+ -]/,"",$1); print $1, $2}') \
        <(git -C "$WT"  submodule status | awk '{sub(/^[+ -]/,"",$1); print $1, $2}') >/dev/null; then
    echo "  SHA MATCH ✅"
else
    echo "  SHA MISMATCH ❌"; FAIL=1
fi
while read -r p; do
    n=$(/bin/ls -A "$WT/$p" 2>/dev/null | grep -cv '^\.git$')
    [ "$n" -le 1 ] && { echo "  EMPTY ❌ $p"; FAIL=1; }
done < <(git -C "$WT" submodule status | awk '{print $2}')

[ "$FAIL" -eq 0 ] && echo "DONE: $WT ($BRANCH) 就绪" || { echo "DONE WITH ERRORS（见上）"; exit 1; }
