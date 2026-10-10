# iOS UI and persistent diagnostics tests

## What is exercised

The real draw path is RenderManager → FrontEndRenderLayer → Scrooby DrawFrame →
FeScreen/FeOwner (authored child order) → GLES/PDDI → MGL → Metal.
Embedded Pure3D objects temporarily change viewport, color writes and depth.
The radar draws colorless depth geometry before its map.

The opt-in `IOS_UI_GPU_TEST=1` scene draws through the game's actual GL/MGL
renderer at EndFrame. Eight screenshot assertions cover opaque drawing, alpha
composition, disabling blending/depth/scissor, colorless depth masking (inside
and outside), and clearing depth while testing is disabled. This exercises
renderer state transitions; it does not validate every authored UI screen.

## Run

```sh
clang++ -std=c++17 -pthread tests/ios/persistent_log_test.cpp -o /tmp/shar-persistent-log-test
/tmp/shar-persistent-log-test
python3 -m venv /tmp/shar-ui-tests-env
/tmp/shar-ui-tests-env/bin/pip install -r tests/ios/requirements.txt
/tmp/shar-ui-tests-env/bin/python tests/ios/run_ui_gpu_test.py \
  --device 61AB94C5-4A22-4BBD-8250-F97EDA345549 \
  --app build_ios/code/Release-iphonesimulator/Simpsons.app \
  --output /tmp/shar-ui-new-results
python3 tests/ios/check_health_log.py /path/to/srr2_health.log
```

Build the app separately first. Choose a booted simulator and a new output
directory. The runner installs/launches the app, captures three frames, copies
the persistent health log, writes result.json and terminates the app.
`--soak-seconds 60` extends its session. Normal gameplay should be measured
separately because this artificial scene replaces the displayed frame.

The logger test checks 1,000 concurrent records, unique sequence numbers,
newline sanitizing, truncation, preceding-session retention and bounded rotation.
The health checker requires a settled, continuous context interval; short runs
fail rather than being reported as memory-stable. Its default growth limit is
64 MiB over at least 30 measured seconds, after 15 seconds of settling.

## Results: 2026-10-09

- Before renderer fixes: opaque, alpha and depth-clear-disabled assertions failed.
  A simultaneous uniform update prevented render-state changes from applying.
  A pending depth clear did not start a pass/attach depth when testing was off.
- After fixes: **24/24 pixel checks passed** (eight checks in three captures).
  Artifacts: `/tmp/shar-ui-final-regression-20261009/`.
- Persistent logger test: **PASS**. Final simulator and signed iPhone Release
  builds: **BUILD SUCCEEDED**. No physical-phone execution in this test pass.
- Normal simulator gameplay before per-frame autorelease cleanup grew from
  approximately 733 MB to 2.48 GB over 40 seconds. After cleanup, growth remains:
  one continuous context-10 interval grew from 990,907,608 to 1,140,641,080 bytes
  over 30 seconds. These are different sessions, not a controlled performance
  comparison. Cleanup alone has **not** resolved the memory growth or established
  why the phone exits. The automated health checker also correctly fails the
  frontend context-2 interval: **+547.19 MiB over 60 settled seconds**.
  Captured log: `/tmp/shar-ui-memory-evidence/after.log`.
- Normal gameplay still shows duplicate Start Mission/Cancel labels. These
  authored-screen defects are not covered by the synthetic pixel pass.

Always-on logs live in the app's Documents directory: `srr2_health.log`,
`srr2_health.previous.log`, device console logs and `srr2_crash.txt`.
Health logs preserve evidence before abrupt exits; an abrupt ending alone is
not proof of a memory-pressure kill. No signal handler can log a SIGKILL.
