# Shader interface regression probe

## Texture binding/deletion regression

```sh
python3 mgl-ios/iosbuild/probes/texture_binding_probe.py
```

This CPU probe compiles the actual `mglBindTexture` and `mglDeleteTextures`
functions with minimal context/storage stubs and UndefinedBehaviorSanitizer.
It covers all 128 mask bits, unbinding, deletion, inactive target bindings,
image-unit cleanup and preservation of unrelated textures. The old deletion
implementation fails the active-mask assertion. This does not validate Metal
pixels or replace a physical-device New Game/loading-transition test.

## Automatic simulator timeline capture

From the repository root, after building the simulator app:

```sh
python3 mgl-ios/iosbuild/probes/capture-simulator.py \
  --app build_ios/code/Debug-iphonesimulator/Simpsons.app \
  --duration 150 --interval 0.5
```

The script boots the selected simulator, installs the app if `--app` is supplied,
records video, launches with lightweight `IOS_FRAME_STATS=1`, and captures PNG
frames every half second. It stops the app and finalizes video at the end. Defaults target
the iPhone 17 UUID in the handoff; override with `--device`, `--bundle`, and
`--output` (a new directory). `--no-video` captures frames/logs only.

Add `--runtime-diagnostics` to include the bounded save, HUD/minimap, scissor,
depth/stencil, colour-write, material-alpha, and GLES blend-state trace in the
same simulator process. Deep vertex/texture inspection is opt-in with
`--attrib-probe`; this can perturb frame time and should not be used to measure
performance. The per-draw alpha histogram is also opt-in with `--alpha-stats`
and should be omitted from performance runs. Use the frame stats to find slow
intervals, then inspect paired screenshots and nearby renderer events. While capture runs, bring the selected
simulator up in Device Hub and reproduce the issue. Add an action marker at each
visible hitch/artifact so it appears on the capture timeline:

```sh
python3 mgl-ios/iosbuild/probes/capture-simulator.py \
  --app build_ios/code/Release-iphonesimulator/Simpsons.app \
  --duration 180 --interval 0.5 --runtime-diagnostics \
  --output /tmp/shar-captures/render-drive-repro

# In another terminal, while the capture is running:
python3 mgl-ios/iosbuild/probes/capture-simulator.py \
  --output /tmp/shar-captures/render-drive-repro \
  --mark 'Driving: frame hitch at school corner'
```

For frame drops, compare the one-second `IOS_FRAME_STATS` intervals with the
action markers, then use Instruments' Time Profiler on the same simulator run
if the CPU is the bottleneck. Only optimize a measured hot path; compare the
same route and scene before/after. For black spots/overlays, pause capture at
the artifact and compare its nearby blend/depth/scissor events and neighboring
frames before changing renderer state.

Use `--attrib-probe` only when diagnosing wrong vertex attributes, texture
bindings, or Metal pipeline inputs. The capture tool no longer enables it by
default, so performance numbers stay separate from deep render probes.

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
