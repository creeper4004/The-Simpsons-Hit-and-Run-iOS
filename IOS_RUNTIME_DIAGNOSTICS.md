# iOS save and rendering diagnostics

## Current phone build: save startup and shareable logs

The iOS app now exposes Documents through Files → On My iPhone → Simpsons Hit &
Run. Share `srr2_health.log` and `srr2_console.log` after an exit, before relaunching;
after a relaunch, share the matching `.previous.log` files as well. The crash text
file is append-only and may describe an older run; compare its time with the
`session-start` and health records. Logs need no environment flags.

At the first running main menu per launch, iOS waits for its save drive, locates
the most recent save with a valid header, and asynchronously loads it. The menu
then defaults to Resume Game. With no valid save, New Game remains the default.
A failed load returns to the menu and is recorded as a `startup-save` event.
Verified a simulator launch with existing saves: slot 1 (the second slot) was
selected, the callback returned error 0, and a screenshot shows Resume Game as
the initial menu choice. Both simulator and signed iPhone Release builds succeed.
Returning to the frontend later in the same process does not reload an older
save over current progress. This loads saved progress, not unsaved mission state.

Health records now include internal/external/compressed/reusable VM bytes and
malloc bytes/block count. `mission` events bracket initialization and cleanup
with immediate health snapshots. Session headers include the compiled build time.

## Mission-transition memory investigation (2026-10-09)

Retrieved the latest phone logs directly from the app container. The five-minute
session ended while loading level01/m1 assets: last footprint 3,523,104,936 bytes,
while reported Metal allocations were 96,354,304 bytes. No crash report for this
session was available; the accessible JetsamEvent predates it. Memory pressure is
plausible, but this evidence does not prove the termination reason.

Fixed a dynamic-buffer ownership error: a small buffer copied into Metal retained
its original VM page, which subsequent buffer orphaning did not free. The CPU
pointer now follows Metal's storage after releasing the original allocation.
Also corrected VM deallocation task/size, released CPU mip data and samplers on
texture deletion, and released old mip storage on same-size redefinitions.

A normal simulator run with the buffer correction passed the existing health
checker: only +1.73 MiB across 45 settled seconds in context 10, peak 253 MiB.
The full renderer changes pass all 24 pixel assertions and a 60-second app soak.
This has not yet reproduced completing mission one and starting mission two on
the phone. Evidence is in `/tmp/shar-phone-mission-crash-20261009/` and
`/tmp/shar-memory-ui-regression-20261009/`.

Set `IOS_RUNTIME_DIAG=1` in the simulator process to write a bounded trace to
`Documents/srr2_runtime_diag.txt`. The trace covers save-drive readiness,
save UI selection/callbacks, HUD/map visibility, touch-HUD profile changes,
and selected GLES/PDDI scissor, alpha, depth, stencil, and colour-write state.
Renderer and touch-context events are capped so they cannot indefinitely grow
the file.

For a simulator-only persistence check, also set `IOS_SAVE_SELF_TEST=1`. On the
front-end or pause screen, once a writable drive is ready, the game chooses the
first empty slot, runs the normal asynchronous save/open/write/commit path, then
reads that slot back and validates `SaveGameInfo`. It never overwrites an
existing slot. A passing trace ends with `self-test write callback error=0`
and `self-test readback exists=1 corrupt=0 valid=1`.

Example for the configured simulator:

```sh
SIMCTL_CHILD_IOS_RUNTIME_DIAG=1 \
SIMCTL_CHILD_IOS_SAVE_SELF_TEST=1 \
xcrun simctl launch --terminate-running-process \
  61AB94C5-4A22-4BBD-8250-F97EDA345549 com.c4rlox.simpsons.ios
```

The self-test is deliberately opt-in and is compiled only for iOS. Keep it
disabled for normal launches; it creates one save in the first empty slot.

## Always-on device exit diagnostics (2026-10-09)

Normal physical-device iOS startup now captures stdout/stderr in `Documents/srr2_console.log`.
The preceding session is retained as `srr2_console.previous.log`; the active
log rolls after 8 MiB at the next five-second health sample. This also captures
renderer errors during ordinary sideloaded launches without environment flags.
Simulator launches preserve the host capture pipe; `IOS_PERSIST_CONSOLE=1`
opts into file redirection there.
`Documents/srr2_health.log` records game context, input state, physical memory
footprint, resident/peak/virtual bytes, sampling status and live-thread CPU time
every five seconds on an independent utility queue, including game-loop stalls.
It is enabled on device and simulator without environment flags. Records are
unbuffered and health samples are fsynced. The file rotates at 4 MiB and keeps
`srr2_health.previous.log`; startup preserves the preceding session. An abrupt end
alone does not establish the termination cause.

Fatal signals append their signal number and fault address to
`Documents/srr2_crash.txt` through a descriptor opened before gameplay. The
handler uses an alternate stack and avoids Foundation, allocation, stdio and
backtrace unwinding. Native iOS crash reports remain the source for thread
backtraces. C++ termination still records its exception detail and backtrace
outside the signal handler.

A system memory-pressure kill cannot execute the application's signal handler.
On the phone, check Settings → Privacy & Security → Analytics & Improvements →
Analytics Data for a `JetsamEvent` at the exit time. Preserve both console files
before repeatedly launching again. The legacy runtime trace remains opt-in. The health file also records UIKit
memory warnings and lifecycle transitions, once-per-second frame statistics,
and sampled UI draw order, transforms, alpha and Metal depth/blend/scissor state.
Release assets omit names, so drawable UIDs identify objects in those builds.

## Screenshot-driven UI corrections

The iOS mission-load screen now activates/deactivates the mobile briefing input
profile. Scrooby text and accept/back sprites now apply the PC asset scaling on
iOS, including matching text wrapping and right alignment. Visual confirmation
on the phone is pending. Normal simulator gameplay still shows duplicate mission labels. GPU tests validate
colorless depth masking, but actual radar appearance and phone rendering still
need confirmation. Cinematic letterbox bars are intentional. See
[the regression tests](tests/ios/README.md) for measured results and commands.
