#!/usr/bin/env python3
"""Capture simulator startup frames and logs on one monotonic timeline."""
import argparse
import datetime
import gzip
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import threading
import time

DEVICE = "61AB94C5-4A22-4BBD-8250-F97EDA345549"
BUNDLE = "com.c4rlox.simpsons.ios"
INTERESTING = re.compile(r"\[SRR2\]|IOS_|ENDPRIMSPROBE|ALPHASTAT|ERROR|WARNING|failed", re.I)


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="milliseconds")


def run(*args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, timeout=60)


def report(directory, frames, events):
    payload = json.dumps({"frames": frames, "events": events}).replace("</", "<\\/")
    page = """<!doctype html><meta charset="utf-8"><title>Rendering capture</title>
<style>body{font:15px system-ui;margin:24px;background:#f5f7fa;color:#192435}
header{display:flex;gap:12px;align-items:center;flex-wrap:wrap}input{flex:1;min-width:200px}
main{display:grid;grid-template-columns:1.3fr 1fr;gap:20px;margin-top:16px}
img{width:100%;max-height:78vh;object-fit:contain;background:#172131;border-radius:8px}
pre{white-space:pre-wrap;overflow:auto;height:70vh;background:white;padding:16px;font-size:12px}
small{color:#526171}button{padding:7px 12px}@media(max-width:900px){main{grid-template-columns:1fr}}
</style><h1>Simulator rendering timeline</h1>
<p>Each frame and log line has seconds since launch. Logs are timestamped when received;
original engine timestamps remain in the text. Screenshot bounds are saved in frames.jsonl.</p>
<header><button id="prev">Previous</button><button id="play">Play</button><button id="next">Next</button>
<input id="slider" type="range" min="0" step="1"><strong id="stamp"></strong></header>
<p id="states"></p><strong id="state"></strong>
<main><section><a id="link"><img id="frame"></a><small id="wall"></small>
<p><a href="boot.mp4">Full video</a> · <a href="events.jsonl">Timed events</a> ·
<a href="console.log.gz">Full compressed log</a> · <a href="manifest.json">Capture details</a></p>
</section><section><h3>Log events near this frame (±0.75 seconds)</h3><pre id="logs"></pre></section></main>
<script>const data=PAYLOAD,slider=document.querySelector('#slider');let timer;
slider.max=data.frames.length-1;function show(){let f=data.frames[Number(slider.value)];if(!f)return;
document.querySelector('#frame').src=f.path;document.querySelector('#link').href=f.path;
document.querySelector('#stamp').textContent=f.elapsed.toFixed(2)+' s';
document.querySelector('#wall').textContent=f.utc;
let state=data.events.filter(e=>e.text.includes('IOS_GAME_STATE')&&e.elapsed<=f.elapsed).pop();
document.querySelector('#state').textContent=state ? state.text : 'Before first game state';
let lines=data.events.filter(e=>Math.abs(e.elapsed-f.elapsed)<=.75);
document.querySelector('#logs').textContent=lines.map(e=>e.elapsed.toFixed(3)+' s  '+e.text).join('\n')||'No selected events in this interval. See full log for every line.';}
for(let e of data.events.filter(e=>e.text.includes('IOS_GAME_STATE')||e.text.startsWith('ACTION:'))){
let b=document.createElement('button');b.textContent=e.elapsed.toFixed(1)+' s '+(e.text.match(/name=([^ ]+)/)?.[1]||e.text);
b.onclick=()=>{let i=data.frames.findIndex(f=>f.elapsed>=e.elapsed);slider.value=i<0?data.frames.length-1:i;show()};
document.querySelector('#states').append(b)}
slider.oninput=show;document.querySelector('#prev').onclick=()=>{slider.value=Number(slider.value)-1;show()};
document.querySelector('#next').onclick=()=>{slider.value=Number(slider.value)+1;show()};
document.querySelector('#play').onclick=()=>{if(timer){clearInterval(timer);timer=null;return}
timer=setInterval(()=>{if(Number(slider.value)>=data.frames.length-1){clearInterval(timer);timer=null;return}
slider.value=Number(slider.value)+1;show()},500)};show();</script>"""
    (directory / "timeline.html").write_text(page.replace("PAYLOAD", payload))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default=DEVICE)
    parser.add_argument("--bundle", default=BUNDLE)
    parser.add_argument("--duration", type=float, default=150)
    parser.add_argument("--interval", type=float, default=0.5)
    parser.add_argument("--app", type=Path, help="Install this simulator .app before launch")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--no-video", action="store_true")
    parser.add_argument("--runtime-diagnostics", action="store_true",
                        help="Enable bounded iOS save/render state trace")
    parser.add_argument("--attrib-probe", action="store_true",
                        help="Enable deep MGL vertex/texture probes; avoid for performance runs")
    parser.add_argument("--alpha-stats", action="store_true",
                        help="Enable per-draw alpha histogram logging; avoid for performance runs")
    parser.add_argument("--mark", help="Add a timed input/action note to an existing --output capture")
    parser.add_argument("--report-only", action="store_true", help="Regenerate timeline for an existing --output capture")
    args = parser.parse_args()
    if args.report_only:
        if not args.output:
            parser.error("--report-only requires --output")
        frames = [json.loads(line) for line in (args.output / "frames.jsonl").read_text().splitlines()]
        events = [json.loads(line) for line in (args.output / "events.jsonl").read_text().splitlines()][:20000]
        if (args.output / "actions.jsonl").exists():
            events.extend(json.loads(line) for line in (args.output / "actions.jsonl").read_text().splitlines())
        report(args.output, frames, sorted(events, key=lambda e: e["elapsed"]))
        return
    if args.mark:
        if not args.output:
            parser.error("--mark requires --output")
        start_file = args.output / "capture-start.json"
        if not start_file.is_file():
            parser.error(
                f"no capture metadata at {start_file}; start a simulator capture first, "
                "then pass the same --output directory to --mark. This helper records "
                "simulator captures only; for a phone test, send the event description "
                "here and its time will be matched against the device runtime log."
            )
        start = json.loads(start_file.read_text())
        wall = utc()
        elapsed = (datetime.datetime.fromisoformat(wall) - datetime.datetime.fromisoformat(start["started_utc"])).total_seconds()
        # Python 3.9 on macOS does not share the monotonic clock origin between
        # processes. External action notes therefore use the shared UTC clock.
        event = {"elapsed": elapsed, "utc": wall, "text": "ACTION: " + args.mark}
        with (args.output / "actions.jsonl").open("a") as stream:
            stream.write(json.dumps(event) + "\n")
        print(json.dumps(event))
        return
    if args.duration <= 0 or args.interval <= 0:
        parser.error("duration and interval must be positive")
    directory = args.output or Path("/tmp/shar-captures") / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    directory.mkdir(parents=True, exist_ok=False)
    (directory / "frames").mkdir()
    devices = json.loads(run("xcrun", "simctl", "list", "devices", "--json").stdout)
    target = next((d for group in devices["devices"].values() for d in group if d["udid"] == args.device), None)
    if not target:
        parser.error("simulator UUID was not found")
    if target["state"] != "Booted":
        run("xcrun", "simctl", "boot", args.device)
    run("xcrun", "simctl", "bootstatus", args.device, "-b")
    if args.app:
        run("xcrun", "simctl", "install", args.device, str(args.app.resolve()))

    frames, events = [], []
    started_utc, started = utc(), time.monotonic()
    (directory / "capture-start.json").write_text(json.dumps({"started_utc": started_utc, "started_monotonic": started}))
    video_log = (directory / "video.log").open("w")
    video = None if args.no_video else subprocess.Popen(
        ["xcrun", "simctl", "io", args.device, "recordVideo", "--codec=h264", str(directory / "boot.mp4")],
        stdout=video_log, stderr=video_log)
    environment = dict(os.environ, SIMCTL_CHILD_IOS_FRAME_STATS="1")
    if args.attrib_probe:
        environment["SIMCTL_CHILD_IOS_ATTRIB_PROBE"] = "1"
    if args.runtime_diagnostics:
        environment["SIMCTL_CHILD_IOS_RUNTIME_DIAG"] = "1"
    if args.alpha_stats:
        environment["SIMCTL_CHILD_IOS_ALPHA_STATS"] = "1"
    app = subprocess.Popen(["xcrun", "simctl", "launch", "--console", "--terminate-running-process",
                            args.device, args.bundle], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, bufsize=1, env=environment)
    log_stats = {"lines": 0, "selected_events": 0}

    def collect():
        with gzip.open(directory / "console.log.gz", "wt") as full, (directory / "events.jsonl").open("w") as selected:
            for line in app.stdout:
                elapsed = time.monotonic() - started
                full.write(f"{elapsed:.6f}\t{utc()}\t{line}")
                log_stats["lines"] += 1
                if INTERESTING.search(line):
                    event = {"elapsed": round(elapsed, 6), "utc": utc(), "text": line.rstrip()}
                    selected.write(json.dumps(event) + "\n")
                    selected.flush()
                    if len(events) < 20000:
                        events.append(event)
                    log_stats["selected_events"] += 1

    reader = threading.Thread(target=collect, daemon=True)
    reader.start()
    print(f"Capturing {args.duration:g}s every {args.interval:g}s: {directory}", flush=True)
    try:
        with (directory / "frames.jsonl").open("w") as index:
            deadline = started
            while time.monotonic() - started < args.duration:
                before, wall = time.monotonic() - started, utc()
                relative = f"frames/{len(frames):04d}-{before:08.3f}s.png"
                result = subprocess.run(["xcrun", "simctl", "io", args.device, "screenshot", str(directory / relative)],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=20)
                after = time.monotonic() - started
                frame = {"path": relative, "elapsed": round((before + after) / 2, 6),
                         "start": round(before, 6), "end": round(after, 6), "utc": wall}
                if result.returncode == 0:
                    frames.append(frame)
                    index.write(json.dumps(frame) + "\n")
                    index.flush()
                else:
                    events.append({"elapsed": after, "utc": utc(), "text": result.stderr.strip()})
                if app.poll() is not None:
                    print(f"Launch/console exited: {app.returncode}", flush=True)
                    break
                deadline = max(deadline + args.interval, time.monotonic())
                time.sleep(max(0, deadline - time.monotonic()))
    except KeyboardInterrupt:
        print("Finishing capture…", flush=True)
    finally:
        # simctl forwards SIGINT to the app; stop it so the log is complete.
        if app.poll() is None:
            app.send_signal(signal.SIGINT)
            try:
                app.wait(timeout=10)
            except subprocess.TimeoutExpired:
                run("xcrun", "simctl", "terminate", args.device, args.bundle)
                app.wait(timeout=10)
        reader.join(timeout=10)
        if video and video.poll() is None:
            video.send_signal(signal.SIGINT)
            try:
                video.wait(timeout=20)
            except subprocess.TimeoutExpired:
                video.kill()
                video.wait()
        video_log.close()
        manifest = {"device": args.device, "bundle": args.bundle, "started_utc": started_utc,
                    "finished_utc": utc(), "duration": time.monotonic() - started,
                    "requested_duration": args.duration, "ended_early": time.monotonic() - started < args.duration,
                    "interval": args.interval, "frames": len(frames), "logs": log_stats,
                    "console_exit": app.returncode, "video_exit": video.returncode if video else None,
                    "timing_note": "Frame elapsed is the midpoint of the screenshot call; log elapsed is receipt time. Video begins before launch and can have a small startup offset."}
        (directory / "manifest.json").write_text(json.dumps(manifest, indent=2))
        if (directory / "actions.jsonl").exists():
            events.extend(json.loads(line) for line in (directory / "actions.jsonl").read_text().splitlines())
            events.sort(key=lambda event: event["elapsed"])
        report(directory, frames, events)
        print(f"Saved {len(frames)} frames and {log_stats['lines']} log lines. Open {directory / 'timeline.html'}", flush=True)


if __name__ == "__main__":
    main()
