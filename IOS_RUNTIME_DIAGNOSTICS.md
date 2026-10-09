# iOS save and rendering diagnostics

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
