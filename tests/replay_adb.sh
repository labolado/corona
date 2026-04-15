#!/bin/bash
# ADB-based replay of InputRecorder JSON recordings
# Usage: bash tests/replay_adb.sh /path/to/recording.json [OPTIONS]
#
# Reads a recording JSON file and replays via adb shell input tap/swipe.
# This uses the exact same coordinate path as manual adb input tap.
#
# Options:
#   --speed FACTOR    Playback speed multiplier (default 1.0, 2.0=2x faster)
#   --offset-ms MS    Skip first MS milliseconds of events (default 0)
#   --dry-run         Print commands without executing
#   --verbose         Print each event detail

set -euo pipefail

RECORDING=""
SPEED=1.0
OFFSET_MS=0
DRY_RUN=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --speed) SPEED="$2"; shift 2 ;;
        --offset-ms) OFFSET_MS="$2"; shift 2 ;;
        --dry-run) DRY_RUN=true; shift ;;
        --verbose) VERBOSE=true; shift ;;
        -*) echo "Unknown option: $1"; exit 1 ;;
        *) RECORDING="$1"; shift ;;
    esac
done

if [ -z "$RECORDING" ]; then
    echo "Usage: bash tests/replay_adb.sh <recording.json> [--speed N] [--dry-run] [--verbose]"
    exit 1
fi

# Accept both local files and device paths
TMP_JSON="/tmp/_replay_adb_$$.json"
trap "rm -f '$TMP_JSON'" EXIT

if [[ "$RECORDING" == /sdcard/* ]] || [[ "$RECORDING" == /data/* ]]; then
    echo "[replay] Reading from device: $RECORDING"
    adb shell cat "$RECORDING" > "$TMP_JSON"
else
    if [ ! -f "$RECORDING" ]; then
        echo "ERROR: File not found: $RECORDING"
        exit 1
    fi
    echo "[replay] Reading: $RECORDING"
    cp "$RECORDING" "$TMP_JSON"
fi

# Parse JSON and generate adb commands
python3 - "$TMP_JSON" "$SPEED" "$OFFSET_MS" "$DRY_RUN" "$VERBOSE" << 'PYEOF'
import json, sys, subprocess, time

json_path = sys.argv[1]
speed = float(sys.argv[2])
offset_ms = float(sys.argv[3])
dry_run = sys.argv[4] == "true"
verbose = sys.argv[5] == "true"

with open(json_path) as f:
    data = json.load(f)

events = data["events"]
meta = data.get("meta", {})

print(f"  events: {len(events)}")
if meta:
    print(f"  screen: {meta.get('screenWidth', '?')}x{meta.get('screenHeight', '?')}")
print(f"  speed: {speed}x")
if offset_ms > 0:
    print(f"  offset: skip first {offset_ms}ms")

# Group events into gestures by id
gestures = {}
for e in events:
    eid = e.get("id", 0)
    if eid not in gestures:
        gestures[eid] = []
    gestures[eid].append(e)

# Convert gestures to timed actions
actions = []

for eid, gevents in gestures.items():
    began = None
    moves = []

    for e in gevents:
        phase = e["phase"]
        t = e["time"]
        x = int(round(e["x"]))
        y = int(round(e["y"]))

        if phase == "began":
            began = (t, x, y)
            moves = []
        elif phase == "moved":
            moves.append((t, x, y))
        elif phase in ("ended", "cancelled"):
            if began is None:
                continue
            bt, bx, by = began

            if not moves:
                # Simple tap
                actions.append((bt, "tap", (bx, by)))
            else:
                # Swipe from first to last point
                all_points = [began] + moves + [(t, x, y)]
                ft, fx, fy = all_points[0]
                lt, lx, ly = all_points[-1]
                duration_ms = max(50, int(lt - ft))
                actions.append((ft, "swipe", (fx, fy, lx, ly, duration_ms)))

            began = None
            moves = []

actions.sort(key=lambda a: a[0])

if offset_ms > 0:
    actions = [(t, typ, params) for t, typ, params in actions if t >= offset_ms]

tap_count = sum(1 for _, t, _ in actions if t == "tap")
swipe_count = sum(1 for _, t, _ in actions if t == "swipe")
print(f"  actions: {len(actions)} ({tap_count} taps, {swipe_count} swipes)")

if not actions:
    print("  WARNING: no actions to replay")
    sys.exit(0)

print()
prev_time = None

for i, (t, typ, params) in enumerate(actions):
    if prev_time is not None:
        delay = (t - prev_time) / 1000.0 / speed
        if delay > 0.01:
            time.sleep(delay)

    if typ == "tap":
        x, y = params
        cmd = ["adb", "shell", "input", "tap", str(x), str(y)]
        label = f"tap ({x},{y})"
    elif typ == "swipe":
        x1, y1, x2, y2, dur = params
        scaled_dur = max(50, int(dur / speed))
        cmd = ["adb", "shell", "input", "swipe", str(x1), str(y1), str(x2), str(y2), str(scaled_dur)]
        label = f"swipe ({x1},{y1})->({x2},{y2}) {dur}ms"
    else:
        continue

    if verbose or dry_run:
        prefix = "[dry] " if dry_run else ""
        print(f"  {prefix}[{i+1}/{len(actions)}] {label} @ {t:.0f}ms")

    if not dry_run:
        subprocess.run(cmd, capture_output=not verbose)

    prev_time = t

print(f"\n  Done: {tap_count} taps, {swipe_count} swipes")
PYEOF

echo "[replay] Complete"
