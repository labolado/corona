#!/usr/bin/env python3
"""
本地截图分析工具 — Worker 用这个代替 Read 截图，零 API token。

用法：
    python3 tools/screenshot_analyze.py /tmp/screenshot.png
    python3 tools/screenshot_analyze.py /tmp/screenshot.png --region 50,50,200,200
    python3 tools/screenshot_analyze.py /tmp/a.png --diff /tmp/b.png
    python3 tools/screenshot_analyze.py /tmp/screenshot.png --check-color 160,120 "red"
    python3 tools/screenshot_analyze.py /tmp/screenshot.png --sample-grid 5x5
"""

import sys
import argparse
from pathlib import Path

try:
    from PIL import Image
    import numpy as np
    HAS_NP = True
except ImportError:
    from PIL import Image
    HAS_NP = False


def analyze_basic(img):
    """基本分析：尺寸、主色、是否空白"""
    w, h = img.size
    pixels = list(img.getdata())
    total = len(pixels)

    # 平均颜色
    if img.mode == 'RGBA':
        r = sum(p[0] for p in pixels) // total
        g = sum(p[1] for p in pixels) // total
        b = sum(p[2] for p in pixels) // total
        a = sum(p[3] for p in pixels) // total
        avg = (r, g, b, a)
    else:
        r = sum(p[0] for p in pixels) // total
        g = sum(p[1] for p in pixels) // total
        b = sum(p[2] for p in pixels) // total
        avg = (r, g, b)

    # 判断是否全白/全黑/单色
    is_white = r > 240 and g > 240 and b > 240
    is_black = r < 15 and g < 15 and b < 15
    is_uniform = max(
        max(p[0] for p in pixels) - min(p[0] for p in pixels),
        max(p[1] for p in pixels) - min(p[1] for p in pixels),
        max(p[2] for p in pixels) - min(p[2] for p in pixels)
    ) < 20

    # 唯一颜色数
    unique = len(set((p[0], p[1], p[2]) for p in pixels))

    print(f"尺寸: {w}x{h}")
    print(f"平均颜色: RGB({avg[0]},{avg[1]},{avg[2]})")
    print(f"唯一颜色数: {unique}")
    if is_white:
        print("判断: ⚠️ 全白（可能空白/未渲染）")
    elif is_black:
        print("判断: ⚠️ 全黑（可能未渲染）")
    elif is_uniform:
        print(f"判断: ⚠️ 单色填充 RGB({r},{g},{b})")
    elif unique < 10:
        print(f"判断: ⚠️ 颜色很少（{unique}种），可能渲染异常")
    else:
        print(f"判断: ✅ 有内容（{unique}种颜色）")


def analyze_region(img, region_str):
    """分析指定区域"""
    x1, y1, x2, y2 = map(int, region_str.split(','))
    crop = img.crop((x1, y1, x2, y2))
    print(f"区域: ({x1},{y1})-({x2},{y2})")
    analyze_basic(crop)


def sample_grid(img, grid_str):
    """网格采样：在图片上均匀取样颜色"""
    cols, rows = map(int, grid_str.lower().split('x'))
    w, h = img.size
    print(f"网格采样 {cols}x{rows}:")
    for r in range(rows):
        row_str = ""
        for c in range(cols):
            x = int(w * (c + 0.5) / cols)
            y = int(h * (r + 0.5) / rows)
            px = img.getpixel((x, y))
            # 简化颜色名
            color = classify_color(px[0], px[1], px[2])
            row_str += f"  ({x:3d},{y:3d})={color:8s}"
        print(row_str)


def classify_color(r, g, b):
    """简化颜色分类"""
    if r > 200 and g > 200 and b > 200:
        return "white"
    if r < 30 and g < 30 and b < 30:
        return "black"
    if r > 200 and g < 80 and b < 80:
        return "red"
    if r < 80 and g > 200 and b < 80:
        return "green"
    if r < 80 and g < 80 and b > 200:
        return "blue"
    if r > 200 and g > 200 and b < 80:
        return "yellow"
    if r > 200 and g > 100 and b < 80:
        return "orange"
    if r > 100 and g < 80 and b > 100:
        return "purple"
    if r < 50 and g < 50 and b < 80:
        return "dark"
    if r > 150 and g > 150 and b > 150:
        return "gray-l"
    if r > 80 and g > 80 and b > 80:
        return "gray"
    return f"{r},{g},{b}"


def check_color(img, pos_str, expected):
    """检查指定像素的颜色"""
    x, y = map(int, pos_str.split(','))
    px = img.getpixel((x, y))
    color = classify_color(px[0], px[1], px[2])
    match = color == expected.lower()
    print(f"像素({x},{y}): RGB({px[0]},{px[1]},{px[2]}) = {color}")
    print(f"期望: {expected} → {'✅ 匹配' if match else '❌ 不匹配'}")
    return match


def diff_images(img1, img2):
    """对比两张截图"""
    w1, h1 = img1.size
    w2, h2 = img2.size
    print(f"图1: {w1}x{h1}, 图2: {w2}x{h2}")

    if (w1, h1) != (w2, h2):
        print("❌ 尺寸不同，无法像素对比")
        return

    pixels1 = list(img1.getdata())
    pixels2 = list(img2.getdata())
    total = len(pixels1)
    diff_count = 0
    max_diff = 0

    for p1, p2 in zip(pixels1, pixels2):
        d = abs(p1[0] - p2[0]) + abs(p1[1] - p2[1]) + abs(p1[2] - p2[2])
        if d > 10:
            diff_count += 1
        max_diff = max(max_diff, d)

    pct = diff_count / total * 100
    print(f"差异像素: {diff_count}/{total} ({pct:.1f}%)")
    print(f"最大差异: {max_diff}")
    if pct < 0.1:
        print("判断: ✅ 几乎完全一致")
    elif pct < 5:
        print("判断: ⚠️ 轻微差异")
    else:
        print(f"判断: ❌ 明显差异 ({pct:.1f}%)")


def flicker_detect(pattern, count=20, interval=0.05):
    """连续快速截图并检测频闪"""
    import subprocess, time, os

    # 获取窗口 ID（通过 Quartz，比 AppleScript 更稳定）
    try:
        from Quartz import CGWindowListCopyWindowInfo, kCGWindowListExcludeDesktopElements, kCGNullWindowID
        windows = CGWindowListCopyWindowInfo(kCGWindowListExcludeDesktopElements, kCGNullWindowID)
        wid = None
        for w in windows:
            owner = w.get('kCGWindowOwnerName', '')
            name = w.get('kCGWindowName', '')
            if owner == 'Corona Simulator' and 'iPhone' in str(name):
                wid = w.get('kCGWindowNumber')
                break
        if wid is None:
            raise RuntimeError("No Corona Simulator window found")
        wid = str(wid)
    except Exception:
        print("❌ 找不到 Corona Simulator 窗口")
        return

    print(f"窗口 ID: {wid}")
    print(f"连续截图 {count} 张，间隔 {interval}s ...")

    # 快速连续截图
    paths = []
    for i in range(count):
        p = f"/tmp/flicker-{i+1:02d}.png"
        subprocess.run(['screencapture', '-l', wid, p],
                      capture_output=True)
        paths.append(p)
        if i < count - 1:
            time.sleep(interval)

    print(f"截图完成，开始分析...\n")

    # 逐帧对比
    flicker_frames = []
    for i in range(len(paths) - 1):
        if not os.path.exists(paths[i]) or not os.path.exists(paths[i+1]):
            continue
        img1 = Image.open(paths[i]).convert('RGBA')
        img2 = Image.open(paths[i+1]).convert('RGBA')

        if img1.size != img2.size:
            continue

        pixels1 = list(img1.getdata())
        pixels2 = list(img2.getdata())
        total = len(pixels1)
        diff_count = 0
        max_diff = 0

        for p1, p2 in zip(pixels1, pixels2):
            d = abs(p1[0]-p2[0]) + abs(p1[1]-p2[1]) + abs(p1[2]-p2[2])
            if d > 10:
                diff_count += 1
            max_diff = max(max_diff, d)

        pct = diff_count / total * 100

        status = "✅" if pct < 0.5 else ("⚠️" if pct < 5 else "❌")
        print(f"  帧{i+1:02d}→{i+2:02d}: 差异 {pct:5.1f}% (max={max_diff:3d}) {status}")

        if pct > 1.0:
            flicker_frames.append((i+1, i+2, pct, max_diff))

    print()
    if flicker_frames:
        print(f"🔴 检测到频闪！{len(flicker_frames)}/{count-1} 帧间有明显差异：")
        for f1, f2, pct, md in flicker_frames:
            print(f"   帧{f1:02d}→{f2:02d}: {pct:.1f}% 差异, max={md}")

        # 分析频闪帧的特征
        worst = max(flicker_frames, key=lambda x: x[2])
        print(f"\n最严重: 帧{worst[0]:02d}→{worst[1]:02d} ({worst[2]:.1f}%)")
        print(f"对比截图: /tmp/flicker-{worst[0]:02d}.png vs /tmp/flicker-{worst[1]:02d}.png")

        # 分析两帧差异区域
        img_a = Image.open(f"/tmp/flicker-{worst[0]:02d}.png").convert('RGBA')
        img_b = Image.open(f"/tmp/flicker-{worst[1]:02d}.png").convert('RGBA')
        w, h = img_a.size
        # 按 4x4 网格看哪个区域差异最大
        print(f"\n差异热力图 (4x4):")
        for row in range(4):
            line = ""
            for col in range(4):
                x1 = w * col // 4
                y1 = h * row // 4
                x2 = w * (col+1) // 4
                y2 = h * (row+1) // 4
                crop_a = img_a.crop((x1,y1,x2,y2))
                crop_b = img_b.crop((x1,y1,x2,y2))
                pa = list(crop_a.getdata())
                pb = list(crop_b.getdata())
                region_diff = sum(1 for p1,p2 in zip(pa,pb)
                                 if abs(p1[0]-p2[0])+abs(p1[1]-p2[1])+abs(p1[2]-p2[2])>10)
                rpct = region_diff / len(pa) * 100
                if rpct < 1:
                    line += "  ·   "
                elif rpct < 10:
                    line += f" {rpct:4.1f}%"
                else:
                    line += f" {rpct:4.0f}%"
            print(f"  {line}")
    else:
        print("✅ 未检测到频闪（所有相邻帧差异 < 1%）")

    # 清理
    print(f"\n截图保留在 /tmp/flicker-*.png")


def main():
    parser = argparse.ArgumentParser(description='本地截图分析（零 API token）')
    parser.add_argument('image', nargs='?', help='截图路径')
    parser.add_argument('--region', help='分析区域 x1,y1,x2,y2')
    parser.add_argument('--diff', help='对比另一张截图')
    parser.add_argument('--check-color', nargs=2, metavar=('POS', 'COLOR'),
                        help='检查指定位置颜色 x,y color')
    parser.add_argument('--sample-grid', default=None,
                        help='网格采样 如 5x5')
    parser.add_argument('--flicker', action='store_true',
                        help='频闪检测：连续快速截图并对比')
    parser.add_argument('--count', type=int, default=20,
                        help='频闪检测截图数量（默认20）')
    parser.add_argument('--interval', type=float, default=0.05,
                        help='频闪检测截图间隔秒数（默认0.05）')

    args = parser.parse_args()

    if args.flicker:
        flicker_detect(args.image, args.count, args.interval)
        return

    if not args.image:
        parser.print_help()
        return

    img = Image.open(args.image).convert('RGBA')

    if args.diff:
        img2 = Image.open(args.diff).convert('RGBA')
        diff_images(img, img2)
    elif args.region:
        analyze_region(img, args.region)
    elif args.check_color:
        check_color(img, args.check_color[0], args.check_color[1])
    elif args.sample_grid:
        sample_grid(img, args.sample_grid)
    else:
        analyze_basic(img)
        if img.size[0] > 100 and img.size[1] > 100:
            print("\n--- 网格采样 4x4 ---")
            sample_grid(img, "4x4")


if __name__ == '__main__':
    main()
