# Shader interface regression probe

## Automatic simulator timeline capture

From the repository root, after building the simulator app:

```sh
python3 mgl-ios/iosbuild/probes/capture-simulator.py \
  --app build_ios/code/Debug-iphonesimulator/Simpsons.app \
  --duration 150 --interval 0.5
```

The script boots the selected simulator, installs the app if `--app` is supplied,
records video, launches with `IOS_ATTRIB_PROBE=1`, and captures PNG frames every
half second. It stops the app and finalizes video at the end. Defaults target
the iPhone 17 UUID in the handoff; override with `--device`, `--bundle`, and
`--output` (a new directory). `--no-video` captures frames/logs only.

Outputs under `/tmp/shar-captures/<timestamp>/`:

- `timeline.html`: frame slider/playback, game state jump buttons, nearby logs.
- `boot.mp4`: continuous recording, including frames between screenshots.
- `frames.jsonl`: screenshot paths, UTC time, monotonic elapsed time, and
  screenshot call start/end bounds.
- `events.jsonl`: timed engine/probe/error events.
- `console.log.gz`: every console line with its receipt time.
- `manifest.json`: simulator, bundle, duration, frame/log counts and exit status.

Log timestamps measure receipt time, so buffered engine stdout can arrive late.
Original timestamps remain in each line. Video starts before the app and can
have a small startup offset; use the screenshot bounds for precise comparisons.
Captures are local debug artifacts and are not committed. Xcode 27 presents the
simulator in Device Hub (`Xcode-beta.app/Contents/Applications/DeviceHub.app`).

Mark an input/action while a capture is running (external notes use UTC delta;
screenshots/logs within the capture process use its monotonic clock):

```sh
python3 mgl-ios/iosbuild/probes/capture-simulator.py \
  --output /tmp/shar-captures/<capture-directory> --mark 'Tap New Game confirm'
```

Regenerate an existing report with `--report-only --output <directory>`, including
markers added after capture completion. Capture timing is approximate; `simctl`
console exit code zero does not guarantee the app did not crash. Check logs and
crash reports. `IOS_GAME_STATE` records boot/menu/loading/gameplay transitions;
`IOS_FRAME_STATS` gives frame/draw/pipeline counts once per second. Renderer hot
INFO/DEBUG logging is disabled by default; `MGL_VERBOSE_LOG` enables it for
focused investigations.

`IOS_GAME_STATE uptime_ms` is the engine clock at each state change; subtract
loading-gameplay from the first gameplay marker to measure the New Game
transition without console buffering skew. `IOS_LOAD_COMPLETE` includes each
file's `duration_ms` and engine `uptime_ms`. Some world assets continue loading
after the first gameplay marker, and the tutorial appears a few seconds later.
Renderer `IOS_DRAW_COORD` samples are bounded per texture and draw type.
`SIMCTL_CHILD_MGL_PROBE_DISABLE_CULL=1` enables an opt-in culling comparison;
normal captures and installed builds retain the game's cull settings.

## CPU shader interface check

This CPU-only probe reproduces the shader-linking problem without an iOS
Simulator or Metal device. It uses the exact glslang/SPIRV-Cross checkouts used
by MGL and the same `shader_interface.h` helper as the renderer.

From the repository root, after restoring the vendor checkout:

```sh
cmake -S mgl-ios/iosbuild/probes -B /tmp/shar-shader-probe -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build /tmp/shar-shader-probe -j 8
/tmp/shar-shader-probe/shader_interface_probe /tmp/shar-shader-output
```

The negative control enables `AUTO_MAP_LOCATIONS` but omits the mapping pass,
matching the former MGL link sequence. All three varyings lack locations.
The fixed path checks distinct, matching vertex/fragment locations, including
when the fragment declarations are reordered and when either shader stage is
used to initialize the GLSL resolver. It also checks generated Metal
`[[user(locnN)]]` and `[[color(0)]]` bindings. Failures return nonzero.

The output directory contains both versions of the SPIR-V and Metal shaders.
This verifies compilation and interface mapping; it does **not** verify GPU
pixels or successful game startup.
