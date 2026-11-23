#!/bin/bash
# Responsiveness, startup and memory benchmark on a 1,000-tab synthetic
# workspace (SCORECARD: Space switch <=100 ms p95, Command bar interactive
# <=100 ms p95, at least 100 samples; workspace usable <=2 s warm / <=5 s cold).
#
# Launches myArc in the background with a 1,000-item synthetic workspace
# (10 Spaces x 100 dormant pins/tabs), measures startup + RSS memory, then runs
# 100 Ctrl+1/Ctrl+2 Space switches and 100 Cmd+T/Escape command-bar cycles
# while recording a Chromium trace and workspace_service VLOG timings.
#
# Usage:
#   tools/check_responsiveness.sh
# Env:
#   APP=<myArc.app>   default: /Applications/myArc Nightly.app
#   SAMPLES=100       number of Space-switch and Command-bar samples
#   OUTDIR=<dir>      default: /tmp/myarc-responsiveness-<timestamp>
#   KEEP=1            keeps the disposable profile directory
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:-/Applications/myArc Nightly.app}"
BIN="$APP/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP/Contents/Info.plist" 2>/dev/null || echo myArc)"
SAMPLES="${SAMPLES:-100}"

if [ "${1:-}" = "--inner" ]; then
  [ -x "$BIN" ] || { echo "native binary missing: $BIN" >&2; exit 1; }
  OUTDIR="${OUTDIR:-/tmp/myarc-responsiveness-$(date -u +%Y%m%dT%H%M%SZ)}"
  mkdir -p "$OUTDIR"

  WORK=$(mktemp -d /tmp/myarc-resp.XXXXXX)
  printf 'Owned synthetic 1000-tab responsiveness profile\n' > "$WORK/.myarc-m0-test-profile"
  PROFILE="$WORK/profile"
  mkdir -p "$PROFILE/Default"
  printf 'Owned synthetic 1000-tab responsiveness profile\n' > "$PROFILE/.myarc-m0-test-profile"
  cat > "$PROFILE/Default/Preferences" <<'JSON'
{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}
JSON

  # Compile bgcapture.swift once so 200+ key posts take milliseconds each.
  BGCAP="$WORK/bgcapture"
  swiftc -O "$ROOT/tools/bgcapture.swift" -o "$BGCAP"

  # Generate a valid 1,000-item synthetic workspace: 10 Spaces x 100 items
  # (8 Favorites + 592 Pins + 400 Today dormant tabs = 1,000 items).
  /usr/bin/python3 - "$PROFILE/Default/myarc-workspace-v1.json" <<'PY'
import json, sys
spaces = [{"id": str(s), "profile_id": "1", "name": f"Space {s}"} for s in range(1, 11)]
items = []
item_id = 1
for fav in range(1, 9):
    items.append({
        "id": str(item_id),
        "placement": 2,
        "owner_id": "1",
        "has_url": True,
        "canonical_url": f"http://127.0.0.1:9/fav-{fav}",
        "title": f"Favorite {fav}",
    })
    item_id += 1

# Remaining 992 items spread across the 10 Spaces (~99-100 each)
remaining = 1000 - len(items)
for idx in range(remaining):
    space_id = (idx % 10) + 1
    placement = 1 if (idx // 10) < 59 else 0  # 1=Pin, 0=Today
    items.append({
        "id": str(item_id),
        "placement": placement,
        "owner_id": str(space_id),
        "has_url": True,
        "canonical_url": f"http://127.0.0.1:9/s{space_id}/item-{item_id}",
        "title": f"Space {space_id} Item {item_id}",
    })
    item_id += 1

assert len(items) == 1000
ws = {
    "version": 1,
    "profile_id": "1",
    "next_space_id": "11",
    "next_item_id": str(item_id),
    "next_window_id": "2",
    "spaces": spaces,
    "items": items,
    "windows": [{"id": "1", "space_id": "1", "selected_item_id": "0"}],
}
with open(sys.argv[1], "w") as f:
    json.dump(ws, f)
print(f"seeded 1,000-item workspace: {len(spaces)} spaces, {len(items)} items")
PY

  PID=""
  cleanup() {
    pkill -KILL -f -- "--user-data-dir=$PROFILE" 2>/dev/null || true
    if [ -z "${KEEP:-}" ] && [ -f "$WORK/.myarc-m0-test-profile" ]; then
      rm -rf "$WORK"
    else
      echo "kept $WORK"
    fi
  }
  trap cleanup EXIT

  VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$APP/Contents/Info.plist" 2>/dev/null || echo unknown)
  HW_MODEL=$(sysctl -n hw.model 2>/dev/null || echo unknown)
  CPU=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo unknown)
  OS_VER=$(sw_vers -productVersion 2>/dev/null || echo unknown)
  echo "machine: $HW_MODEL ($CPU), macOS $OS_VER, myArc $VERSION"

  TRACE="$OUTDIR/trace.json"
  LOG="$OUTDIR/chrome.log"

  # Launch and measure time until first on-screen window appears.
  LAUNCH_START=$(/usr/bin/python3 -c 'import time; print(f"{time.time():.6f}")')
  open -g -n -a "$APP" --args \
    --user-data-dir="$PROFILE" \
    --no-first-run --no-default-browser-check --use-mock-keychain \
    --disable-backgrounding-occluded-windows \
    --enable-logging --log-file="$LOG" --vmodule=workspace_service=1 \
    --trace-startup=toplevel,views,ui,input \
    --trace-startup-format=json \
    --trace-startup-duration=45 \
    --trace-startup-file="$TRACE" \
    --window-size=1280,800 --window-position=40,60 \
    about:blank

  for _ in $(seq 1 120); do
    for q in $(pgrep -f -- "--user-data-dir=$PROFILE" || true); do
      ps -o command= -p "$q" | grep -q -- '--type=' && continue
      [[ "$(ps -o command= -p "$q")" == "$BIN "* ]] && { PID=$q; break; }
    done
    [ -n "$PID" ] && break
    sleep 0.1
  done
  [ -n "$PID" ] || { echo "native process did not start" >&2; exit 1; }

  # Wait for first on-screen window of PID.
  for _ in $(seq 1 100); do
    if "$BGCAP" windows "$PID" list 2>/dev/null | grep -q .; then
      break
    fi
    sleep 0.05
  done
  WINDOW_READY=$(/usr/bin/python3 -c 'import time; print(f"{time.time():.6f}")')
  STARTUP_MS=$(/usr/bin/python3 - "$LAUNCH_START" "$WINDOW_READY" <<'PY'
import sys
print(f"{(float(sys.argv[2]) - float(sys.argv[1])) * 1000:.1f}")
PY
)
  echo "browser PID=$PID, first on-screen window in ${STARTUP_MS} ms"

  sleep 2
  # Capture sidebar populated with 1,000-item workspace.
  "$BGCAP" shot "$PID" "$OUTDIR/1-1000-tabs-sidebar.png" > /dev/null 2>&1 || true

  # Measure total RSS across browser + helpers for this profile.
  RSS_MB=$(/usr/bin/python3 - "$PROFILE" <<'PY'
import subprocess, sys
prof = sys.argv[1]
out = subprocess.check_output(["ps", "-axo", "pid,rss,command"], text=True)
total_kb = 0
procs = 0
for line in out.splitlines()[1:]:
    if prof in line:
        parts = line.split(None, 2)
        total_kb += int(parts[1])
        procs += 1
print(f"{total_kb / 1024:.1f} MB ({procs} processes)")
PY
)
  echo "steady-state RSS (1,000 dormant items): $RSS_MB"

  # Run SAMPLES Space switches (Ctrl+1 / Ctrl+2) and measure end-to-end
  # latency from key post until Workspace saved log line appears in chrome.log.
  /usr/bin/python3 - "$BGCAP" "$PID" "$LOG" "$SAMPLES" "$OUTDIR/space-switch.json" <<'PY'
import json, re, subprocess, sys, time

bgcap, pid, log_path, n_str, out_path = sys.argv[1:]
n = int(n_str)

def count_saves():
    try:
        with open(log_path, "r", errors="replace") as f:
            return len(re.findall(r"Workspace saved in (\d+) us", f.read()))
    except FileNotFoundError:
        return 0

def read_save_us():
    with open(log_path, "r", errors="replace") as f:
        return [int(m) for m in re.findall(r"Workspace saved in (\d+) us", f.read())]

# Warmup 2 switches so initial layout caches are hot
for combo in ("ctrl+2", "ctrl+1"):
    subprocess.run([bgcap, "key", pid, combo], check=True)
    time.sleep(0.15)

base_saves = count_saves()
wall_ms = []
for i in range(n):
    combo = "ctrl+2" if (i % 2 == 0) else "ctrl+1"
    target = base_saves + i + 1
    t0 = time.monotonic()
    subprocess.run([bgcap, "key", pid, combo], check=True)
    deadline = t0 + 2.0
    while count_saves() < target and time.monotonic() < deadline:
        time.sleep(0.002)
    t1 = time.monotonic()
    if count_saves() < target:
        raise AssertionError(f"Space switch {i+1} ({combo}) did not save within 2s (saves={count_saves()} < {target})")
    # bgcapture.swift sleeps 30ms between keyDown and keyUp + 30ms after keyUp;
    # subtract the synthetic 60ms CGEvent helper sleep to measure actual browser latency?
    # Keep raw wall_ms AND note bgcapture's 60ms helper sleep transparently!
    wall_ms.append((t1 - t0) * 1000.0)

all_save_us = read_save_us()[base_saves:base_saves + n]
save_ms = [u / 1000.0 for u in all_save_us]

def pct(vals, p):
    s = sorted(vals)
    idx = min(len(s) - 1, max(0, int(round((p / 100.0) * (len(s) - 1)))))
    return s[idx]

# Subtract bgcapture's 30ms keyDown->keyUp + 30ms post-keyUp sleep ONLY in a
# separate adjusted metric; report raw wall time too.
adj_ms = [max(0.0, v - 60.0) for v in wall_ms]
res = {
    "samples": len(wall_ms),
    "raw_wall_ms": {"p50": round(pct(wall_ms, 50), 2), "p95": round(pct(wall_ms, 95), 2), "max": round(max(wall_ms), 2)},
    "net_browser_ms": {"p50": round(pct(adj_ms, 50), 2), "p95": round(pct(adj_ms, 95), 2), "max": round(max(adj_ms), 2)},
    "workspace_save_ms": {"p50": round(pct(save_ms, 50), 2), "p95": round(pct(save_ms, 95), 2), "max": round(max(save_ms), 2)},
}
with open(out_path, "w") as f:
    json.dump(res, f, indent=2)
print("Space switch (100 samples, 1,000-item workspace):", json.dumps(res))
PY

  # Capture command bar open on 1,000-item workspace, then run SAMPLES
  # Cmd+T -> Escape cycles while Chromium trace captures UI-thread latency.
  "$BGCAP" key "$PID" cmd+t
  sleep 0.4
  "$BGCAP" shot "$PID" "$OUTDIR/2-1000-tabs-commandbar.png" > /dev/null 2>&1 || true
  "$BGCAP" key "$PID" escape
  sleep 0.2

  /usr/bin/python3 - "$BGCAP" "$PID" "$SAMPLES" "$OUTDIR/command-bar.json" <<'PY'
import json, subprocess, sys, time
bgcap, pid, n, out_path = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]

def win_count():
    p = subprocess.run([bgcap, "windows", pid, "list"], capture_output=True, text=True)
    return len([ln for ln in p.stdout.splitlines() if ln.strip()])

base_wins = win_count()
open_raw = []
for i in range(n):
    t0 = time.monotonic()
    subprocess.run([bgcap, "key", pid, "cmd+t"], check=True)
    deadline = t0 + 2.0
    while win_count() <= base_wins and time.monotonic() < deadline:
        time.sleep(0.005)
    t1 = time.monotonic()
    if win_count() <= base_wins:
        raise AssertionError(f"Command bar did not open on sample {i+1}")
    open_raw.append((t1 - t0) * 1000.0)
    subprocess.run([bgcap, "key", pid, "escape"], check=True)
    deadline = time.monotonic() + 2.0
    while win_count() > base_wins and time.monotonic() < deadline:
        time.sleep(0.005)

def pct(vals, p):
    s = sorted(vals)
    return round(s[min(len(s) - 1, int(round((p / 100.0) * (len(s) - 1))))], 2)

# Subtract bgcapture's 60ms CGEvent keyDown->keyUp->post sleep for net estimate.
open_net = [max(0.0, v - 60.0) for v in open_raw]
res = {
    "samples": len(open_raw),
    "raw_wall_ms": {"p50": pct(open_raw, 50), "p95": pct(open_raw, 95), "max": round(max(open_raw), 2)},
    "net_browser_ms": {"p50": pct(open_net, 50), "p95": pct(open_net, 95), "max": round(max(open_net), 2)},
}
with open(out_path, "w") as f:
    json.dump(res, f, indent=2)
print("Command bar open (100 samples, 1,000-item workspace):", json.dumps(res))
PY

  # Wait up to 15s for startup trace to finish flushing to $TRACE before quitting.
  for _ in $(seq 1 60); do [ -s "$TRACE" ] && break; sleep 0.25; done

  # Clean quit so Chromium flushes everything cleanly.
  kill -TERM "$PID" 2>/dev/null || true
  for _ in $(seq 1 100); do kill -0 "$PID" 2>/dev/null || break; sleep 0.25; done
  kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null || true

  # Parse Chromium trace for CrBrowserMain input / key event durations.
  /usr/bin/python3 - "$TRACE" "$OUTDIR/trace-summary.json" <<'PY'
import json, os, sys

trace_path, out_path = sys.argv[1:]
if not os.path.exists(trace_path) or os.path.getsize(trace_path) == 0:
    print("trace.json not written or empty")
    sys.exit(0)

data = json.load(open(trace_path))
events = data.get("traceEvents", data if isinstance(data, list) else [])

# Find CrBrowserMain thread (pid, tid)
browser_threads = set()
for ev in events:
    if ev.get("ph") == "M" and ev.get("name") == "thread_name":
        if ev.get("args", {}).get("name") == "CrBrowserMain":
            browser_threads.add((ev.get("pid"), ev.get("tid")))

key_durs_ms = []
ui_task_durs_ms = []
for ev in events:
    if ev.get("ph") != "X" or "dur" not in ev:
        continue
    if (ev.get("pid"), ev.get("tid")) not in browser_threads:
        continue
    dur_ms = ev["dur"] / 1000.0
    name = ev.get("name", "")
    cat = ev.get("cat", "")
    if "Key" in name or "Accelerator" in name or "Event" in name:
        key_durs_ms.append(dur_ms)
    if "toplevel" in cat and dur_ms >= 0.5:
        ui_task_durs_ms.append(dur_ms)

def stats(vals):
    if not vals:
        return {"count": 0}
    s = sorted(vals)
    def pct(p): return round(s[min(len(s)-1, int(round((p/100.0)*(len(s)-1))))], 2)
    return {"count": len(s), "p50": pct(50), "p95": pct(95), "p99": pct(99), "max": round(s[-1], 2)}

summary = {
    "trace_events": len(events),
    "browser_main_key_event_ms": stats(key_durs_ms),
    "browser_main_tasks_ge_0_5ms": stats(ui_task_durs_ms),
}
with open(out_path, "w") as f:
    json.dump(summary, f, indent=2)
print("Trace summary:", json.dumps(summary))
PY

  echo "RESPONSIVENESS BENCHMARK COMPLETE! Evidence in: $OUTDIR"
  ls -la "$OUTDIR"
  exit 0
fi

exec /usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner "$@"
