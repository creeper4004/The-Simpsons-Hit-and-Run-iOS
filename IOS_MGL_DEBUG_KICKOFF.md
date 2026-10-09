# iOS MGL Debug Kickoff

Continue the 2026-10-08 rendering/input repair. Read the latest result in
`IOS_MGL_RENDER_HANDOFF.md` first and preserve the working tree.

## Verified progress

- Pause Settings crash repaired: iOS uses the PC asset rows rather than absent
  console inversion/vibration rows. User confirms Sound and Settings work.
- Latest phone initial-menu uptime 9.837 s; New Game load 17.860 s (earlier
  builds about 79–88 s). Boot and level loading get bounded extra IO service.
- HUD map viewport/scissor origin conversion is in live verification; see
  latest render handoff before changing the renderer.

- Legal artwork and the full living-room menu render on the simulator. The
  user also confirmed readable menus on the signed phone build.
- Shader mapping, program-owned uniforms, exact/orphaned immediate buffers,
  retained vertex layouts, texture upload, blend invalidation, and depth/stencil
  preservation are repaired.
- OpenGL-to-Metal clip depth conversion restores the touch HUD sprites.
- Phone New Game crash `Simpsons-2026-10-08-212352.ips` points to buffer deletion:
  GL target enum 34962 was used as a binding/VAO array index. Cleanup now scans
  bounded binding/attribute slots and clears references.
- iOS creates the existing virtual touch controller and flushes queued input.
  Simulator touch confirm accepts New Game and reaches `loading-gameplay`.
- iOS touch movement axes are enabled; user confirms phone movement works.
- Ground, house, garage, hedges and paths render on simulator and phone.
  `pglPrimBuffer::populatedVertices` preserves the vertex count across separate
  attribute and index locks. The old final-lock cursor was zero and iOS uploaded
  zeroed static meshes. User confirms the phone world looks complete.
- Hot renderer logs are quiet by default. `IOS_FRAME_STATS` and `IOS_GAME_STATE`
  provide summaries under `IOS_ATTRIB_PROBE`. Status-file writes are bounded.

Do not claim full game correctness from the menu alone. Inspect the loading
transition, world geometry, character controls, camera, and new crash reports.

## Capture and build

Simulator UUID: `61AB94C5-4A22-4BBD-8250-F97EDA345549` (iPhone 17, iOS 27).
Bundle: `com.c4rlox.simpsons.ios`.
CoreSimulator access works. Xcode 27 displays it in Device Hub:
`/Applications/Xcode-beta.app/Contents/Applications/DeviceHub.app`.

```sh
FORCE=1 JOBS=8 mgl-ios/iosbuild/build.sh
xcodebuild -project build_ios/SRR2.xcodeproj -scheme SimpsonsIOS \
  -configuration Debug -sdk iphonesimulator \
  -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build
python3 mgl-ios/iosbuild/probes/capture-simulator.py \
  --app build_ios/code/Debug-iphonesimulator/Simpsons.app \
  --duration 240 --interval 0.5
```

Capture installs/launches, records video plus half-second PNGs, timestamps logs,
and creates `timeline.html` with game state jump buttons. It stops the app at
the end. See `mgl-ios/iosbuild/probes/README.md` for markers, report regeneration,
timing limitations, and CPU shader probe commands.

Captures under `/tmp/shar-captures/` include:

- `stream-fix-20261008`: first recovered menu, with black effect rectangles.
- `final-render-20261008`: clean menu, 299 frames across 150 seconds.
- `touch-fallback-fix-20261008`: touch/input and New Game transition.
- `world-count-fix-20261008`: restored world, 340 frames across 180 seconds.
- `loading-budget-20261008`: follow-up with bounded level-loading service budget.

Slow loading is specifically the New Game level transition: prior phone runs
took about 79–88 s, while the world-count simulator run took 7.6 s. Measure the
new 8-pass/4-ms loading-only budget with `IOS_GAME_STATE uptime_ms` and
`IOS_LOAD_COMPLETE duration_ms`; do not infer phone performance from simulator
speed. The depth-attachment experiment in `depth-world-probe-20261008` crashed
the simulator Metal service and was reverted. Culling disable did not restore
terrain. These are investigation artifacts, not fixes to ship.

Device archives are isolated:
`MGL_SDK=iphoneos FORCE=1 JOBS=8 mgl-ios/iosbuild/build.sh`.
Build `build_ios_dev/SRR2.xcodeproj`, scheme SimpsonsIOS, Release/iphoneos with
automatic signing. Do not mix simulator and device archive slices.

Vendor source is ignored at `mgl-ios/src/mgl`; the durable source is
`mgl-ios/mgl-ios.patch` against pinned MGL `de0ded0`, plus the wrapper and
`mgl-ios/iosbuild/shader_interface.h`. After vendor changes regenerate:

```sh
git -C mgl-ios/src/mgl diff --binary -- MGL > mgl-ios/mgl-ios.patch
```
