#!/usr/bin/env python3
"""
Record mouse clicks on Corona Simulator window, save as replayable script.

USAGE:
  Record:  python3 record_clicks.py record [output.json] [--timeout N] [--bg] [--pidfile FILE]
  Replay:  python3 record_clicks.py replay [input.json] [--repeat N] [--check-alive PID]

OPTIONS:
  --timeout N     Stop recording automatically after N seconds
  --bg            Run in background (writes PID to file, useful with --pidfile)
  --pidfile FILE  Write process PID to FILE (default: /tmp/record_clicks.pid)
  --repeat N      Replay N times (default: 1)
  --check-alive PID  Check if process PID is still alive after each replay

WORKFLOW EXAMPLES:

  1. Simple recording (Ctrl+C to stop):
     python3 record_clicks.py record

  2. Background recording with timeout (auto-stop after 30s):
     python3 record_clicks.py record --bg --timeout 30 --pidfile /tmp/rec.pid
     # ... do your testing ...
     kill $(cat /tmp/rec.pid)

  3. Replay with crash detection:
     python3 record_clicks.py replay /tmp/clicks.json --check-alive 12345

  4. Full stress test workflow:
     # Terminal 1: Start simulator
     ./Corona Simulator.app/Contents/MacOS/Corona Simulator -project test/main.lua

     # Terminal 2: Record your interactions
     python3 record_clicks.py record --timeout 60

     # Terminal 3: Stress test (replay until crash)
     for i in {1..100}; do
         python3 record_clicks.py replay --repeat 1 --check-alive $(pgrep "Corona Simulator")
         if [ $? -ne 0 ]; then echo "CRASH on attempt $i"; break; fi
     done
"""
import sys, json, time, subprocess, os, argparse, signal
import Quartz
from Cocoa import NSEvent

PIDFILE = "/tmp/record_clicks.pid"

def get_corona_window():
    """Find Corona Simulator window bounds."""
    wl = Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionOnScreenOnly, Quartz.kCGNullWindowID)
    for w in wl:
        name = str(w.get('kCGWindowOwnerName', ''))
        if 'Corona' in name:
            b = w.get('kCGWindowBounds', {})
            return {
                'x': b.get('X', 0), 'y': b.get('Y', 0),
                'w': b.get('Width', 0), 'h': b.get('Height', 0),
                'pid': w.get('kCGWindowOwnerPID', 0)
            }
    return None

def is_process_alive(pid):
    """Check if a process with given PID is still running."""
    if not pid or pid <= 0:
        return False
    try:
        os.kill(int(pid), 0)
        return True
    except (OSError, ValueError, TypeError):
        return False

def record(output_file, timeout=None, background=False, pidfile=None):
    """Record mouse clicks relative to Corona Simulator window."""
    
    # Background mode: fork and exit parent
    if background:
        pid = os.fork()
        if pid > 0:
            # Parent process
            if pidfile:
                with open(pidfile, 'w') as f:
                    f.write(str(pid))
            print(f"[BG] Recording started in background (PID: {pid})")
            return
        # Child process continues
        os.setsid()  # Create new session
        sys.stdout = open('/dev/null', 'w')
        sys.stderr = open('/dev/null', 'w')
    
    # Write our PID to file (for foreground mode too, useful for signaling)
    if pidfile:
        with open(pidfile, 'w') as f:
            f.write(str(os.getpid()))
    
    # Setup signal handlers
    stop_recording = [False]
    def on_signal(signum, frame):
        stop_recording[0] = True
    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)
    
    if not background:
        print("=== CLICK RECORDER ===")
        if timeout:
            print(f"Auto-stop after {timeout} seconds")
        else:
            print("Press Ctrl+C to stop")
        print("Waiting for Corona Simulator window...")

    win = None
    start_wait = time.time()
    while not win:
        win = get_corona_window()
        if not win:
            # Timeout waiting for window
            if time.time() - start_wait > 30:
                if not background:
                    print("ERROR: Timeout waiting for Corona Simulator window")
                sys.exit(1)
            time.sleep(0.5)

    if not background:
        print(f"Found window at ({win['x']}, {win['y']}) size {win['w']}x{win['h']}")
        print("Recording...\n")

    clicks = []
    start_time = time.time()
    last_state = False
    
    # Create output file immediately (even if empty, so it's ready)
    with open(output_file, 'w') as f:
        json.dump({'window_size': [win['w'], win['h']], 'clicks': [], 'recording': True}, f)

    while not stop_recording[0]:
        # Check timeout
        elapsed = time.time() - start_time
        if timeout and elapsed >= timeout:
            if not background:
                print(f"\n[Timeout reached: {timeout}s]")
            break

        # Get current mouse button state
        btn = Quartz.CGEventSourceButtonState(Quartz.kCGEventSourceStateHIDSystemState, 0)

        if btn and not last_state:
            # Mouse just pressed — use CGEvent for absolute screen coordinates
            # This works across ALL displays (not just main screen)
            event = Quartz.CGEventCreate(None)
            cursor = Quartz.CGEventGetLocation(event)
            abs_x = cursor.x
            abs_y = cursor.y  # CGEvent uses top-left origin, same as window bounds

            # Refresh window position (CGWindowBounds also uses top-left origin)
            win = get_corona_window()
            if win:
                rel_x = abs_x - win['x']
                rel_y = abs_y - win['y']

                # Only record clicks inside the window
                if 0 <= rel_x <= win['w'] and 0 <= rel_y <= win['h']:
                    t = round(elapsed, 2)
                    pct_x = round(rel_x / win['w'], 4)
                    pct_y = round(rel_y / win['h'], 4)
                    click = {'t': t, 'pct_x': pct_x, 'pct_y': pct_y, 'abs_x': round(abs_x), 'abs_y': round(abs_y)}
                    clicks.append(click)
                    
                    # Write to file immediately (atomic write for safety)
                    tmp_file = output_file + '.tmp'
                    with open(tmp_file, 'w') as f:
                        json.dump({'window_size': [win['w'], win['h']], 'clicks': clicks, 'recording': True}, f)
                    os.replace(tmp_file, output_file)
                    
                    if not background:
                        print(f"  Click #{len(clicks)}: t={t}s  rel=({rel_x:.0f},{rel_y:.0f})  pct=({pct_x:.1%},{pct_y:.1%})")

        last_state = btn
        time.sleep(0.02)  # 50Hz polling

    # Final write with recording flag set to False
    win = get_corona_window()
    if win and clicks:
        with open(output_file, 'w') as f:
            json.dump({'window_size': [win['w'], win['h']], 'clicks': clicks, 'recording': False}, f, indent=2)
    
    if not background:
        if clicks:
            print(f"\n=== Saved {len(clicks)} clicks to {output_file} ===")
        else:
            print("\n=== No clicks recorded ===")
    
    # Clean up PID file
    if pidfile and os.path.exists(pidfile):
        try:
            os.remove(pidfile)
        except:
            pass

def replay(input_file, repeat=1, check_alive_pid=None):
    """Replay recorded clicks on Corona Simulator window."""
    with open(input_file) as f:
        data = json.load(f)

    clicks = data['clicks']
    
    if not clicks:
        print("ERROR: No clicks to replay")
        return False
    
    print(f"=== REPLAYING {len(clicks)} clicks (repeat={repeat}) ===")
    if check_alive_pid:
        print(f"Will check if PID {check_alive_pid} stays alive after each replay")

    crashed_count = 0
    
    for r in range(repeat):
        if repeat > 1:
            print(f"\n--- Repeat {r+1}/{repeat} ---")

        # Check if target process is alive before replay
        if check_alive_pid and not is_process_alive(check_alive_pid):
            print(f"ERROR: Target process {check_alive_pid} is not running")
            crashed_count += 1
            continue

        win = get_corona_window()
        if not win:
            print("ERROR: Corona Simulator not found")
            return False

        print(f"Window at ({win['x']}, {win['y']}) size {win['w']}x{win['h']}")

        start_time = time.time()
        for i, click in enumerate(clicks):
            # Check if target died mid-replay
            if check_alive_pid and not is_process_alive(check_alive_pid):
                print(f"=== CRASHED during replay #{r+1} at click {i+1}! ===")
                crashed_count += 1
                break

            # Wait for timing
            target_time = start_time + click['t']
            now = time.time()
            if target_time > now:
                time.sleep(target_time - now)

            # Recalculate absolute position from percentage
            win = get_corona_window()
            if not win:
                print("ERROR: Window disappeared (crashed?)")
                crashed_count += 1
                break

            abs_x = win['x'] + click['pct_x'] * win['w']
            abs_y = win['y'] + click['pct_y'] * win['h']

            # Click
            point = Quartz.CGPointMake(abs_x, abs_y)
            evt_down = Quartz.CGEventCreateMouseEvent(None, Quartz.kCGEventLeftMouseDown, point, Quartz.kCGMouseButtonLeft)
            evt_up = Quartz.CGEventCreateMouseEvent(None, Quartz.kCGEventLeftMouseUp, point, Quartz.kCGMouseButtonLeft)
            Quartz.CGEventPost(Quartz.kCGHIDEventTap, evt_down)
            time.sleep(0.05)
            Quartz.CGEventPost(Quartz.kCGHIDEventTap, evt_up)

            print(f"  Click #{i+1}: ({abs_x:.0f},{abs_y:.0f}) pct=({click['pct_x']:.1%},{click['pct_y']:.1%})")

        # Wait a moment then check if still alive
        time.sleep(1)
        
        # Check window still exists
        win = get_corona_window()
        if not win:
            print(f"=== CRASHED after replay #{r+1}! ===")
            crashed_count += 1
            continue
        
        # Check specific PID if requested
        if check_alive_pid:
            if not is_process_alive(check_alive_pid):
                print(f"=== CRASHED (PID {check_alive_pid} gone) after replay #{r+1}! ===")
                crashed_count += 1
                continue
            else:
                print(f"=== Replay #{r+1} complete, PID {check_alive_pid} still alive ===")
        else:
            print(f"=== Replay #{r+1} complete, app still alive ===")

    # Summary
    if repeat > 1:
        print(f"\n=== SUMMARY: {repeat - crashed_count}/{repeat} replays survived, {crashed_count} crashed ===")
    
    return crashed_count == 0

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Record and replay mouse clicks on Corona Simulator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s record -o clicks.json --timeout 60
  %(prog)s record --bg --timeout 30 --pidfile /tmp/rec.pid
  %(prog)s replay clicks.json --repeat 10 --check-alive 12345
        """
    )
    
    subparsers = parser.add_subparsers(dest='mode', help='Mode')
    
    # Record subcommand
    record_parser = subparsers.add_parser('record', help='Record mouse clicks')
    record_parser.add_argument('output', nargs='?', default='/tmp/corona_test_clicks.json',
                               help='Output JSON file (default: /tmp/corona_test_clicks.json)')
    record_parser.add_argument('--timeout', '-t', type=int, default=None,
                               help='Auto-stop after N seconds')
    record_parser.add_argument('--bg', action='store_true',
                               help='Run in background')
    record_parser.add_argument('--pidfile', default=PIDFILE,
                               help=f'PID file path (default: {PIDFILE})')
    
    # Replay subcommand
    replay_parser = subparsers.add_parser('replay', help='Replay recorded clicks')
    replay_parser.add_argument('input', nargs='?', default='/tmp/corona_test_clicks.json',
                               help='Input JSON file (default: /tmp/corona_test_clicks.json)')
    replay_parser.add_argument('--repeat', '-r', type=int, default=1,
                               help='Repeat replay N times (default: 1)')
    replay_parser.add_argument('--check-alive', type=int, default=None, metavar='PID',
                               help='Check if PID is alive after each replay')
    
    args = parser.parse_args()
    
    if args.mode == 'record':
        record(args.output, timeout=args.timeout, background=args.bg, pidfile=args.pidfile)
    elif args.mode == 'replay':
        success = replay(args.input, repeat=args.repeat, check_alive_pid=args.check_alive)
        sys.exit(0 if success else 1)
    else:
        parser.print_help()
        sys.exit(1)
