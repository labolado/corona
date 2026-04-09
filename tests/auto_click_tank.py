#!/usr/bin/env python3
"""Auto-click tank card in course list to reproduce crash."""
import Quartz, time, sys, os

def get_corona_window():
    wl = Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID)
    for w in wl:
        if 'Corona' in str(w.get('kCGWindowOwnerName', '')):
            b = w.get('kCGWindowBounds', {})
            return float(b['X']), float(b['Y']), float(b['Width']), float(b['Height']), w['kCGWindowOwnerPID']
    return None

def click(x, y):
    point = Quartz.CGPointMake(x, y)
    down = Quartz.CGEventCreateMouseEvent(None, Quartz.kCGEventLeftMouseDown, point, Quartz.kCGMouseButtonLeft)
    up = Quartz.CGEventCreateMouseEvent(None, Quartz.kCGEventLeftMouseUp, point, Quartz.kCGMouseButtonLeft)
    Quartz.CGEventPost(Quartz.kCGHIDEventTap, down)
    time.sleep(0.05)
    Quartz.CGEventPost(Quartz.kCGHIDEventTap, up)

def is_alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except:
        return False

win = get_corona_window()
if not win:
    print("No Corona Simulator window found")
    sys.exit(1)

wx, wy, ww, wh, pid = win
print(f"Window: x={wx} y={wy} w={ww} h={wh} pid={pid}")

# The tank cards are roughly at these positions (percentage of content area):
# Content area starts after phone bezel (~13% from left, ~7% from top)
# Card 2 (middle top) is at roughly 50% x, 30% y of content
# We'll click the second tank card (known to trigger crash)
content_x = wx + ww * 0.48  # Middle tank card
content_y = wy + wh * 0.33  # Top row

print(f"Clicking tank card at ({content_x:.0f}, {content_y:.0f})")
click(content_x, content_y)

# Wait and check
time.sleep(3)
if is_alive(pid):
    print("Still alive after 3s")
    # Try clicking again (maybe we missed)
    click(content_x, content_y)
    time.sleep(5)
    if is_alive(pid):
        print("ALIVE - no crash")
    else:
        print("CRASHED after second click!")
        sys.exit(1)
else:
    print("CRASHED!")
    sys.exit(1)
