# iOS MGL Render Bring-Up — Session Handoff (2026-09-03)

## Latest live result — 2026-10-08 evening

### Streamed terrain and radar follow-up (22:24 local)

- User confirms the streamed world and minimap now work on the phone after
  viewport/scissor conversion and the gameplay/pause loader budget. This
  includes driving beyond the original spawn area; later levels remain unverified.
- `/tmp/shar-phone-map-stream.log`: `l1i02.p3d` 369 ms (previous run
  37,485 ms), `ralph_m.p3d` 66 ms (previous 19,485 ms), `l1z2.p3d`
  3,100 ms and `l1r2.p3d` 802 ms. Individual runs, not averages.
- User requested the radar at the upper-right. The existing Android layout
  and icon-offset compensation are now enabled on iOS. iOS keeps this position
  when input switches to a controller. Simulator shows the aligned frame/map/
  icons at upper-right. Final phone install and circular-mask check pending.
- Passing nil depth state triggered Metal Simulator's "Invalid depth stencil
  state" failure. An explicit Always/no-write state replaces nil. After restarting
  the simulator, this renders frontend and gameplay without that failure.
  Render-pass attachments have not been broadened.
- A remaining square edge on the simulator is being checked with explicit
  depth writes for colourless Scrooby mask geometry, restoring the prior write
  state afterward. Do not mark that follow-up verified until capture review.

### Settings and loading follow-up (22:08 local)

- User's later driving screenshot shows terrain cutting off beyond the spawn
  area; do not interpret the earlier complete-world confirmation as all regions
  verified. Phone loads after gameplay begins include `l1i02.p3d` at 37.485 s
  and gag/character files at 13–32 s each. Gameplay/pause streaming now receives
  a smaller 4-pass/2-ms budget while the loading-screen budget stays 8-pass/4-ms.
  Visual verification of region boundaries is pending.

- Both phone crashes at 21:57:16 and 22:00:23 are in
  `CGuiMenu::SetSelectionValue`, from `CGuiScreenPauseSettings::InitIntro`.
  iOS uses PC assets but requested absent console InvertCamControl/Vibration
  rows. The constructor returned with a partially initialized menu. iOS now
  uses the PC row set consistently in construction, enums and settings updates.
  User confirms Sound and Settings now show and work on the phone; simulator
  capture shows Camera/Jump Cameras/Navigation/Radar/Tutorial with values.
- Bounded loader servicing (up to 8 extra passes / 4 ms between passes) now
  runs during bootup and level loading. Phone initial-menu engine uptime is
  9.837 s versus 55.780 s in the previous measured build. New Game loading
  is 17.860 s (previous budget run 18.935 s; earlier builds about 79–88 s).
  Simulator loading is 1.807 s versus 7.576 s before the budget; bootup-to-menu
  takes 3.002 s. These are individual instrumented runs, not benchmark averages.
- Capture `/tmp/shar-captures/settings-boot-fix-20261008/` contains 474 frames
  over 240 seconds. Phone log is `/tmp/shar-phone-settings-boot.log`.
- User reports a map-sized world patch near Marge. MGL passed bottom-left GL
  viewport/scissor coordinates directly to top-left Metal coordinates. The
  conversion removes the displaced patch in the simulator, but circular mask/
  map content still need verification. Explicitly resetting disabled depth state
  is being checked separately; render-pass attachment behavior is unchanged.

### World and movement recovery (21:57 local)

- User confirms movement works on the phone after enabling the portable touch
  HUD axis update/clear path for iOS. Buttons and New Game already worked.
- **Ground/buildings now render on both simulator and phone.** User confirms
  the phone world looks complete. Simulator capture
  `/tmp/shar-captures/world-count-fix-20261008/` shows lawn, path, house,
  garage and hedges, plus Homer/Marge, minimap and touch controls.
- Root cause: `pglPrimBuffer::Lock()` resets `total` for each attribute chunk.
  The loader locks again for indices/metadata, leaving that cursor at zero.
  iOS's complete-layout upload copied only `total` vertices, zeroing static
  indexed meshes. `populatedVertices` now preserves the high-water vertex
  count on unlock; uploads and non-indexed draws use that count.
- The unlit material shader path is enabled on iOS, matching Android. Small
  Metal buffers no longer receive a duplicate retained reference. Two hot
  command-buffer success logs are now gated by `MGL_VERBOSE_LOG`.
- Disabling culling did not restore terrain. A separate experiment attaching
  depth throughout disabled-depth draws crashed the simulator Metal service;
  it was reverted. Do not reapply that experiment as a verified fix.
- User clarifies the slow phone phase is **after New Game**, not initial boot.
  Earlier phone logs show about 79–88 seconds in that level-loading phase.
  Simulator world-count run took 7.6 s from loading context to gameplay.
  A bounded additional loader-service budget (8 passes, 4 ms, only during
  `CONTEXT_LOADING_GAMEPLAY`) is being measured. `IOS_LOAD_COMPLETE` records
  per-file durations; game-state markers now include engine `uptime_ms`.

The world recovery supersedes the missing-ground status below. Broad gameplay,
driving, later levels and long-session stability have not been verified.

### New Game and touch follow-up (21:36 local)

- User confirms the recovered living-room menu on the real phone.
- Crash report `Simpsons-2026-10-08-212352.ips` is EXC_BAD_ACCESS in
  `mglDeleteBuffers`, called while `FrontEndContext::OnStop` unloads menu meshes.
  The GL target enum 34962 was used as both binding and vertex attribute index.
  Cleanup now visits bounded binding/VAO slots and clears deleted references;
  VM-only buffer backing is released with the task port.
- SPIRV-Cross's `SPVC_COMPILER_OPTION_FIXUP_DEPTH_CONVENTION` converts OpenGL
  clip depth to Metal's range. Without it, the HUD sprites at the near plane
  were clipped even with depth testing disabled. Touch icons now render.
- The existing virtual touch controller and queued-input flush were compiled
  for Android only. Both are enabled for iOS. Simulator touch confirm starts
  New Game and dismisses the tutorial. No controller was needed for this run.
- Simulator capture `/tmp/shar-captures/touch-fallback-fix-20261008/` has 479
  half-second frames over 240 s. Menu appears at 19.5 s, touch New Game reaches
  loading at 38.3 s and gameplay at 46.0 s. Phone logs also reach gameplay.
  No recurrence of the reported menu-unload crash in these runs.
- **Remaining rendering issue:** gameplay shows Homer/Marge, minimap, props,
  tutorial UI and touch controls, but most ground/buildings are missing against
  the sky. Full gameplay rendering is not fixed. Indexed mesh/state inspection
  is in progress; do not regress the now readable frontend.
- HUD diagnostic files are now opt-in and updated once per second, instead of
  rewriting them every frame. The report has game state jump buttons and action
  markers; `--report-only` regenerates an existing capture without relaunching.

**The legal page and complete NEW GAME living-room menu now render.** The
150-second simulator capture at `/tmp/shar-captures/stream-fix-20261008/`
contains 300 half-second screenshots, video, a compressed full console log,
and an interactive `timeline.html`. Legal artwork is readable around 15 s;
the menu is visible around 29 s. Earlier cyan-only screenshots and permission
blockers below are historical. Full gameplay still needs a live check.

Additional fixes found during live investigation:

- Standalone uniforms now have program-owned storage. Vertex/fragment Metal
  binding numbers no longer alias modelview with fragment alpharef, or values
  across different programs. Struct members such as `light0.enabled` resolve
  to native MSL offsets. Sampler locations resolve and retain texture units.
- Uniform updates rebind copied Metal bytes even when other GL state is dirty.
- Immediate streams orphan and exactly size their VBO for each draw. Previously
  later draws overwrote shared GPU memory, and a large mesh permanently moved
  even tiny UI quads onto that unsafe path.
- Buffer reallocation clears released Metal pointers and frees VM-only backing.
- VAO object dirtiness propagates into the renderer's context state; depth and
  stencil survive the many render passes used by the bridge.
- Blend setters invalidate the pipeline's alpha state. Blend/write masks are
  refreshed for opaque and transparent pipelines. This addresses the black
  effect rectangles seen in the first recovered-menu capture.
- A timed follow-up capture exposed a CPU crash in a legacy texture probe:
  `gltex.cpp:137` scanned BC-compressed bytes as RGBA. It now samples the actual
  decompressed image before freeing it, under `IOS_ATTRIB_PROBE`.
- Repetitive renderer INFO/DEBUG logs are opt-in via `MGL_VERBOSE_LOG`.
  `IOS_FRAME_STATS` emits bounded draw/frame/pipeline summaries. Game context
  transitions emit `IOS_GAME_STATE`; the status file updates on state changes.

`mgl-ios/iosbuild/probes/capture-simulator.py` automates this capture and report.
See its README for usage, timing limitations, and action markers. Captures stop
the app at the end and remain outside Git. The full vendor changes are retained
in `mgl-ios/mgl-ios.patch`; simulator and device libraries remain separate.

The paired phone accepted the signed Release app and launched it. Its current
logs reach gameplay after New Game; the user confirmed the menu recovery.
Visual/input verification of the latest phone build is being collected. Do
not claim complete world/device correctness from the frontend screenshot alone.

## 2026-10-08 UPDATE — shader interface repair built; visual validation pending

**Current result:** concrete renderer fixes are implemented and the simulator
Debug app builds. A CPU regression reproduces missing shader bindings and
verifies the correction through SPIR-V and generated Metal source. No loaded
screenshot of this new build was obtained. Do not mark the cyan/white artifacts
or the 1–2 minute startup delay resolved until a live run confirms it.

### Confirmed defects and changes

1. **Missing shader interface mapping.** `AUTO_MAP_LOCATIONS` was enabled in
   `shaders.c`, but `program.c` generated SPIR-V without applying the mapper.
   The CPU reproduction showed `tc`, `cpri`, and `csec` with no Location
   decorations; generated Metal lacked `[[user(locnN)]]`, and the fragment
   output lacked `[[color(0)]]`. The simple default `map_io()` call also failed
   a reordered-declaration test. The fix uses the **GLSL resolver and mapper**
   to match linked stages by name, via `iosbuild/shader_interface.h`.
2. **Retained mesh fallback attributes.** `pglPrimBuffer::Display()` changed
   `GL_ARRAY_BUFFER` to the normal fallback and then configured real UV/color
   pointers without restoring the mesh binding. Its one-element white buffer
   also assumed stride 0 meant constant; GL advances tightly packed vertices.
   The iOS retained path now uploads complete 36-byte interleaved vertices,
   with zero normal/UV and white color defaults, matching the immediate path.
3. **Attribute location assumptions.** iOS translation now maps
   position/normal/texcoord/color to slots 0/1/2/3 by name. It bypasses MGL's
   unimplemented `glBindAttribLocation` and verifies active locations after
   linking. The prior `IOS_ATTRIB_PROBE` instrumentation is preserved.
4. **Pipeline recompilation.** The restored source did not contain the old
   pipeline cache. A cache now keys on shader functions, attachments, blend
   state, and vertex layouts; it holds up to 256 variants. Real pipeline
   errors are reported instead of compiling dummy black fallback shaders.
   This removes redundant compilation; the startup improvement is unmeasured.
5. **Program/uniform lifetime and errors.** Uniform location -1 is ignored.
   Linked glslang programs remain alive until replacement/deletion (the old
   pointer was freed immediately and then freed again on program deletion).
   Link/map failures now stop the link, and `GL_LINK_STATUS` reflects success.

### Validation and durable source

- `FORCE=1 JOBS=8 mgl-ios/iosbuild/build.sh`: succeeds, simulator archives staged.
- `xcodebuild ... -configuration Debug -sdk iphonesimulator
  -destination 'generic/platform=iOS Simulator'`: succeeds. Product:
  `build_ios/code/Debug-iphonesimulator/Simpsons.app`.
- `mgl-ios/iosbuild/probes/`: standalone CPU regression, **PASS**. It shares
  the renderer's mapping helper, checks both stages' bindings with reordered
  declarations and both resolver stages, and checks the emitted MSL semantics.
  This is a compiler/interface check, not a GPU rendering test.
- `mgl-ios/mgl-ios.patch` now captures the restored source's earlier local
  fixes/probes plus these changes. Applying it to pristine upstream `de0ded0`
  reproduces the modified vendor files. The build script pins MGL and the four
  shader-toolchain repos, refreshes its wrapper/helper, and fails on a bad patch.
- Logs/artifacts: `/tmp/shar-render-20261008/{vendor-build.log,
  app-build-final.log,probe-results.txt,probe-output/}`. Probe source and rebuild
  instructions are tracked under `mgl-ios/iosbuild/probes/`; temporary logs
  are not the only record of the fix.

### Runtime limitation and next step

The previous app was launched on simulator `61AB94C5-4A22-4BBD-8250-F97EDA345549`
before session permissions changed. Subsequent `simctl` calls fail with
`Operation not permitted` / CoreSimulator connection invalid. An optional
native offscreen test also reports no accessible Metal device. User permission
was requested and granted conversationally, but the actual session environment
remained restricted. No new loaded baseline or post-fix screenshot was captured.

Once CoreSimulator access is available, install this built app and capture
console output with `simctl launch --console` redirected by the host shell.
Enable `SIMCTL_CHILD_IOS_ATTRIB_PROBE=1`, let the full known asset load finish,
and compare the legal page/logo/UI. Check shader/link/pipeline errors first;
then inspect remaining buffer/texture problems if needed. Measure time to the
first actual game drawing and pipeline compilation count. Device rendering
still needs a separate device archive rebuild and run; current archives are
simulator slices. A renderer replacement was not needed for this repair.


## 2026-09-26 UPDATE — first-draw vertex-location probe

- `IOS_ATTRIB_PROBE` is an opt-in launch environment variable. The iOS GLSL
  translator logs each injected vertex-input declaration/location, and MGL
  logs one draw's reflected SPIR-V input locations plus enabled VAO slots,
  formats, stride, offsets, and Metal-buffer availability.
- Simulator Debug build succeeds with the probe. On the first captured draw
  (`GL_TRIANGLE_STRIP`, program 2, four vertices), SPIR-V locations are
  `0,1,2,3`; enabled VAO slots are `0..3`, with the expected interleaved
  offsets `0,12,24,32` and stride `36`. The translated built-in declarations
  are `position`, `normal`, `texcoord`, `color` at locations `0..3`.
- This verifies the built-in program's translated locations match the engine
  arrays for this draw. The Metal buffer diagnostic reports length 0 and a
  null contents pointer for the attribute buffer, so this run did **not** yet
  verify the bytes Metal fetches. Investigate buffer creation/binding in
  `processGLState` / `bindMTLBuffer` before concluding vertex data is correct.
  Other runtime shader declarations still need comparison against their
  program's reflected locations.
- Reproduce with `SIMCTL_CHILD_IOS_ATTRIB_PROBE=1` set for `simctl launch`;
  filter console output for `IOS_ATTRIB_PROBE`. The probe is off by default.

## 2026-09-26 UPDATE — simulator Debug baseline restored; rendering remains broken

This update supersedes the September 4 run as the current build/runtime
baseline. MGL remains the selected compatibility path; no Vulkan/MoltenVK
rewrite was attempted.

### Build and runtime baseline
- Xcode 27.0, iOS 27.0 Simulator, iPhone 17
  (`61AB94C5-4A22-4BBD-8250-F97EDA345549`), bundle
  `com.c4rlox.simpsons.ios`.
- The preserved `mgl-ios/libs/*.a` were **iphoneos** archives and failed the
  simulator link (`building for iOS-simulator, but linking object built for
  iOS`). `mgl-ios/src/mgl` was absent. The saved patch applied cleanly to a
  fresh MGL checkout at `de0ded0`; the MGL dependencies and iOS wrapper were
  restored, and simulator archives were rebuilt successfully.
- `xcodebuild -project build_ios/SRR2.xcodeproj -scheme SimpsonsIOS
  -configuration Debug -sdk iphonesimulator -destination
  'platform=iOS Simulator,id=61AB94C5-4A22-4BBD-8250-F97EDA345549' build`
  succeeds. Debug app:
  `build_ios/code/Debug-iphonesimulator/Simpsons.app`.
- The app was installed and launched. Allow the known long first load to
  finish before judging the screen. At the loaded front-end/legal page the
  render is malformed: broad cyan and white bands; a later capture also shows
  stretched horizontal strips. Screenshots from this run:
  `/tmp/shar-ios-debug-20260926-after-load.png` and
  `/tmp/shar-ios-debug-20260926-probes-after-wait.png`.
- Unified log from the first launch:
  `/tmp/shar-ios-debug-20260926.unified.log`. The separate console-attached
  run exposed the engine probes, but its filtered output was not saved.

### New runtime evidence
- MGL prints `MGL ERROR: Critical error in program.c at line 814` repeatedly.
  This is `mglBindAttribLocation()`, currently unimplemented in the rebuilt
  MGL source (`mgl-ios/src/mgl/MGL/src/program.c`). The engine calls it four
  times before linking (`position`, `normal`, `texcoord`, `color`) in
  `libs/pure3d/pddi/gles/glprog.cpp`.
- The iOS GLSL translator injects explicit vertex input locations in source
  declaration order (`esToDesktopGLSL()` in `glprog.cpp`). This may happen to
  match the engine's four attribute slots for the built-in shaders, but that
  has not been verified for every runtime shader. The MGL error is confirmed;
  its causal role in the bad pixels is not yet confirmed.
- `ENDPRIMSPROBE` shows CPU-side quad positions and non-constant UV pairs
  reaching `pglContext::EndPrims()`. `ALPHASTAT` records alpha progressing
  from 0 through 255 during the fade, then remaining at 255. This weakens the
  theory that the loaded screen is blank because CPU alpha never becomes
  visible; it does not verify what Metal fetches or samples.
- The loaded screenshot varies from broad flat bands to thin horizontal
  streaks. Treat the previous September 4 texture-upload/readback evidence as
  historical: that instrumented MGL fork lived in `/tmp` and was not the
  source rebuilt for this baseline.

### Next-session debugging sequence
See `IOS_MGL_DEBUG_KICKOFF.md` for a copy-paste brief. In order:
1. Reproduce this Debug build and wait through the full asset load; save a
   loaded screenshot and filtered console output.
2. Verify the shader translator's location assignment against each linked
   program's actual vertex inputs. Then make the smallest isolated correction:
   implement GL `glBindAttribLocation` semantics in MGL, or prove the iOS
   shader translation makes those calls redundant and safely bypass them.
   Do not merely suppress the error without checking the resulting mapping.
3. Compare one affected draw's CPU attributes with the actual MGL/Metal vertex
   buffer and vertex descriptor. Use a single-frame Metal capture if needed.
4. Only after vertex fetch is verified, check the bound texture, current
   texture bytes, sampler state, and sampled texel for that same draw.

Rebuild MGL archives with `FORCE=1 JOBS=8 mgl-ios/iosbuild/build.sh`; the
ordinary script fast path skips rebuilding when archives already exist. The
archives currently in `mgl-ios/libs` are simulator slices; rebuild device
slices before a physical-device link.

Rendering layer for the iOS SHAR port: OpenGL-on-Metal via **MGL**
(`openglonmetal/MGL`, Apache-2.0, static into the app). Companion to
`IOS_BLACK_FE_HANDOFF.md` (that doc covers the game-logic/FE black-screen era;
this one covers the GL render path and supersedes its "renders black" claims —
the September 2026 Debug build presents frames, but the loaded front-end image
is malformed; see the current update above).

---

## 1. What we had BEFORE this session (the GL path as of 2026-09-02)

- **EAGL/OpenGLES drawables are dead on iOS 26/27** (sim AND real A19 device):
  `renderbufferStorage` returns NO. Same wall NZ:P hit → the port pivoted to
  **MGL** (GL 4.6/ES3 entry points over CAMetalLayer/MTLDevice).
- MGL repo + deps cross-compiled for iOS-sim arm64:
  - repo `/tmp/mgl/mgl` (MGL + `external/{SPIRV-Cross,SPIRV-Headers,SPIRV-Tools,glslang}`)
  - `libmgl.a` via `/tmp/mgl/iosbuild/CMakeLists.txt` (iOS-sim toolchain,
    `MGL_GL_CORE=1`, `gl_es.c` excluded, `glm_params.c` → iOS stub)
  - glslang/SPIRV-Tools archives staged in `/tmp/mgl/libs/` (5 `.a`)
- MGL iOS portability patches already applied: UIKit bind
  `CppCreateMGLRendererAndBindToUIView`, Shared/Depth32 stencil storage,
  `didModifyRange:` mac-guarded, headless factory stub, etc.
- **Proof harness passed** (`/tmp/mgl/harness`): solid blue full-screen, ~60fps,
  dlsym entry OK → MGL itself was proven on iOS.
- Engine-side integration (uncommitted, present at session start):
  `gldisplay.cpp` iOS block (`iOSMGLStart` via `SDL_Metal_CreateView`,
  glad loader wired to `iOSMGLGetProcAddress` = dlsym), `win32platform.cpp`
  dropped `SDL_WINDOW_OPENGL`, EndPrims/BeginContext/SwapBuffers no-ops or
  MGL calls, `code/CMakeLists.txt` `IOS_MGL_LIB_DIR` links the 5 `.a`.

**Where it crashed at session start (Release sim):**
1. `pglDisplay::InitDisplay`/`pglContext::pglContext` NULL jump → glad table
   was empty: engine GL calls go through **glad function-pointer globals**
   (`glad_glGetIntegerv` …), not direct MGL links. glad's loader must run —
   or the whole table must be assigned — before any pddi call.
2. After glad ran: NULL `glGetIntegerv` in `pglContext` ctor.
3. After that: `TextureBuilder::BeginImage` NULL pddi texture
   (textures rejected as TOO_BIG).
4. Splash/FE layer: shader compile asserts, VAO NULL crash in `mglEnable…`,
   `validate_vao` NULL buffer, buffer-map `assert(mapped_buffers==count)`,
   `generatePipelineDescriptor` shader-compile asserts → black screen then
   crash after seconds. All fixed this session (below).

---

## 2. What this session DID (fix chain, all uncommitted working-tree edits)

### 2.1 Build/config of MGL in the app
- **glad loader is unreliable with MGL** (its version/ext string handling):
  fallback assigns all 663 glad entries directly from MGL exports
  (`gldisplay.cpp`); legacy GL1 names (glBegin…) resolve NULL and are unused.
- **OES VAO entries**: glad's ES2 loader fills neither `glBindVertexArrayOES`
  nor core `glBindVertexArray`; map OES → MGL core exports straight via dlsym.
- **MGL was dead-stripped / shadowed** — the big one:
  - Nothing references `gl_core.c`'s `gl*` wrappers statically → ld64 stripped
    them → every `gl*` call resolved (dlsym `RTLD_DEFAULT`) to the simulator's
    **OpenGLES stub** (stable 0x248… region) returning 0/NULL for object
    creation, `glCreateShader/Program/GenTextures → 0`, `glGetIntegerv → 0`.
  - Fixes in `code/CMakeLists.txt` (SimpsonsIOS):
    `-Wl,-force_load,<libmgl.a>` + `XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING NO`
    + `-Wl,-u,_glGetIntegerv`; and `iOSMGLGetProcAddress` now prefers
    `dlsym(dlopen(NULL))` (main image first) over `RTLD_DEFAULT`.

### 2.2 MGL iOS stubs (libmgl rebuilds; staged into `/tmp/mgl/libs`)
- `getMacOSDefaults` iOS stub now reports real capabilities:
  `max_texture_size=16384`, `max_3d_texture_size=2048`,
  `max_cube_map_texture_size=16384`, and **`state.max_vertex_attribs=16`**
  + `max_color_attachments=8` — without these MGL answered
  `GL_MAX_TEXTURE_SIZE=0` (every texture `PDDI_TEX_TOO_BIG`) and aborted every
  draw ("buffer_map count 10 exceeds max_vertex_attribs 0").
- `mglGetString(GL_EXTENSIONS)` added (returns "" → pddi uses its
  software-swizzle/pow2-safe paths).
- **Default VAO for name 0**: `mglBindVertexArray(0)` materializes a real
  default-state VAO under table key 0 (GLES2 engines bind 0 then configure
  attribs; `mglEnableVertexAttribArray` etc otherwise crash on NULL).
- **`glUniform` with location −1 is a silent no-op** (GLES spec) instead of
  assert — MGL reflection cannot name every engine uniform (struct arrays,
  fragment-only scalars).
- SPIRV-Cross C-API backends: iosbuild defines
  `SPIRV_CROSS_C_API_{MSL,GLSL,HLSL,CPP,REFLECT}=1` (MSL was off →
  `spvc_context_create_compiler` → "Invalid backend").
- Draw skip for empty-resource programs is graceful (logs once), not an assert.

### 2.3 Engine GLSL: ES1 → desktop translation (`glprog.cpp`, iOS-only)
Engine default + runtime shaders are GLSL ES 1.00; MGL/glslang parses desktop.
`esToDesktopGLSL()` now:
- strips `precision` lines; `attribute→in`, `varying→in/out`,
  `texture2D→texture`; `gl_FragColor→fragColor` (+ `out vec4 fragColor;`).
- presents the body as `#version 100` so MGL's legacy path enables
  **location auto-map** and rewrites the line to `#version 330 core`
  (its `client_version=330` branch is broken; `#version 450` also failed).
- injects `layout(location=N)` **on vertex inputs only** (MSL `stage_in`
  structs need `[[attribute(n)]]`; glslang only writes Location decorations
  for explicit `layout()`; layout on outputs breaks the 100→330 path).
- **unrolls `for(int i=0;i<4;i++){…lights[i]…}` loops** with guard inversion
  (`if (lights[k].enabled != 0) { … }` — spirv-cross turns plain `continue`
  copies into invalid MSL).
- **flattens `uniform struct LightParams {…} lights[4];` into
  `light0..light3`** — spirv-cross cannot express a uniform struct array as
  discrete Metal buffers with indexing (even constant), emitting
  "member reference on `const constant LightParams*`".
- Compile-status handling: MGL reports FALSE with empty info log when parse
  succeeded → engine proceeds to link (real errors surface at link/draw).

### 2.4 Immediate-mode draw path (`glcon.cpp` EndPrims, iOS)
Engine uses GLES2 client-side arrays (CPU pointers, no VBO). MGL requires a
buffer behind every enabled attrib and expects enabled attribs == program
inputs. iOS EndPrims uploads coords/normals/uvs/colours into one persistent
client VBO, **always enables all 4 attribs**, and feeds **zero-filled buffers**
for arrays the stream omits (normals/uvs/colours) so the program's input set is
always satisfied.

### 2.5 Debug support
- `crashwatch` (hub process `python3 /tmp/crash_watch.py`) tails
  `~/Library/Logs/DiagnosticReports/{,Retired/}` and prints parsed crash
  blocks instantly (termination/exception/faulting-thread backtrace).
  Seed file `/tmp/crash_watch_seen.txt`.
- Crash-report writing for Simpsons stopped working mid-session (~02:11);
  when that happens, watch with `simctl launch --console-pty` and grep for
  `Assertion failed` / `MGL ERROR`.

---

## 3. CURRENT STATE (verified 2026-09-03, clean Release build)

- `xcodebuild -project build_ios/SRR2.xcodeproj -scheme SimpsonsIOS
  -configuration Release -sdk iphonesimulator` → BUILD SUCCEEDED.
  Product `build_ios/code/Release-iphonesimulator/Simpsons.app`
  (29.7 MB, no instrumentation strings).
- Installed on sim `969432DF-267F-4B28-8560-2502D085A3A6` (iPhone 17 Pro,
  iOS 27.0): **launches, stays alive 70s+ (indefinite so far), no crash log.**
- Screen renders at 2622×1206 landscape (MGL presentation confirmed
  "Presenting drawable … 2622x1206"), currently a **dark front-end frame**
  (avgL≈5, ~6% colored pixels, max 85) that matches the clean-Debug frame
  pixel-for-pixel — FE renders; brightness/texture richness still under
  investigation (see open items).
- Front-end asset chain runs fully in the logs: `language.p3d` → `bootup.p3d`
  → `backend.p3d` → resource callbacks → `gui-project-load-complete`.
- Instrumentation stripped: probe logs, unroll hit-logs, translated-source
  dumps, dlsym-NULL spam, per-draw skip warning (now once). Real error logs
  kept (shader/link/NewTexture failures).

### Open items (next session)
1. **Runtime-material programs reflect EMPTY resource lists** (`unif=0
   stagein=0`, `linked_glsl_program` non-NULL) → MGL skips those draws with a
   single warning. Root cause candidate: resource-reflection fill ordering at
   link time for programs the engine assembles from runtime shaders (the three
   default programs reflect fine: `unif=10`). Fixing this should light up the
   texture-program draws (menus/text).
2. Dim visuals may be the same cause (missing texture/diffuse draws) or a
   lighting/uniform gap — re-check after (1).
3. Deferred (pre-existing FE-layer thread, see `IOS_BLACK_FE_HANDOFF.md`):
   bootup→FE handoff/`FeResourceManager` lifetime around project churn.

### Build/run facts
- Sim UDID `969432DF-267F-4B28-8560-2502D085A3A6`; bundle
  `com.c4rlox.simpsons.ios`; assets pre-extracted (no 60s re-extract unless
  container recreated).
- MGL rebuild: `cmake --build /tmp/mgl/iosbuild/build` → copy
  `libmgl.a` to `/tmp/mgl/libs/` → xcodebuild (app links the staged archives
  via `IOS_MGL_LIB_DIR=/tmp/mgl/libs` in `build_ios/CMakeCache.txt`).
- After any CMakeLists change: `cmake build_ios` to regenerate the Xcode
  project; product path is SYMROOT-based (`build_ios/code/…`),
  `-derivedDataPath` is ignored.
- **NEVER `git checkout`/reset in this tree** — the entire iOS port is
  uncommitted (working tree is the only copy; see the data-loss incident note
  in the session history for `gldisplay.cpp`).

## 2026-09-04 UPDATE — live-run recheck: ALL draws fail; MGL fork sources LOST

Relaunched the same Release build (sim 969432DF, fresh data container) and
captured the console. **Supersedes the "FE renders, dark frame" claim** — the
dark frame was the clear colour, not drawn content.

**Runtime evidence (log `/tmp/shar_run_000633.log`, 2.77M lines):**
- FE reached: `fe-onstart prev=1` → `fe-start nextScreen=14 splash` →
  `fe-gotoscreen id=14`; then **every draw aborts**.
- `MGL map: skipping draw (empty resource lists, mapped=3 count=4)` — printed
  ONCE at first FE draw. This is the open-item-1 warning: it is NOT empty
  reflection (engine has only 3 GL programs, all linked OK: 6 glslang stages,
  `Program link returned 1` ×6). It is **mapBuffersToMTL failing to map 1 of 4
  stage inputs** on the immediate path.
- Then 567,573× `MGL ERROR: mtlDrawArrays - processGLState failed` (fork
  `MGLRenderer.m:4141 expr=[self mapBuffersToMTL]`) — every immediate draw
  fails the same way from the first FE frame on. Presentation unaffected
  (`Presenting drawable 2622x1206` ×4481). Screen: black → light blue
  (FE splash clear), zero content.
- 13× `mglDrawElements GL error 0x502 / validate_vao: element buffer missing`
  before the skip — the retained/indexed path (`pglPrimBuffer::Display`,
  glcon.cpp:936-1015) uploads the index buffer into the VAO only on first
  lock; under MGL's incomplete VAO state that binding is lost → FE indexed
  draws dead.
- 2× `MGL BUFFER ERROR: mglBufferSubData - Suspicious buffer pointer …
  buffers.c:878` — the EndPrims zero-fill vectors (`sZeros`) feed pointers MGL
  rejects (buffers.c:878 path in fork).
- 226,910 pipeline-state creations (~2/draw) — per-draw `DIRTY_PROGRAM`/
  blend churn, no MGL-side cache hits.

**Engine-side facts (source, closed):** only 3 GL programs exist per context
(color/texture/alphaTest, glcon.cpp ctor; selected in SetTextureEnvironment
glcon.cpp:1282). Game code has ZERO direct GL calls; all rendering is pddi
(~50 API methods, 110 BeginPrims sites, 1 DrawPrimBuffer consumer:
p3d/primgroup.cpp). The GLES backend is a sealed 4.1k-line unit.

**CRITICAL:** the MGL fork sources (`/tmp/mgl/mgl`), the iosbuild wrapper, AND
the 5 staged `.a` (`/tmp/mgl/libs`) are GONE (/tmp wiped). The iOS build
cannot be relinked today — no recovery path. Stock MGL master is cloned at
`/tmp/mgl-src` (incl. external/{glslang,SPIRV-Cross,SPIRV-Tools,SPIRV-Headers})
and is close to the fork, but needs the fork's iOS patches redone
(`CppCreateMGLRendererAndBindToUIView`, caps stub, didModifyRange guard,
default-VAO/name-0, `#version 100→330` legacy path) plus new instrumentation.
Any MGL-level fix requires reconstructing that build first.

Next-step candidates (engine-side, testable once MGL lib is rebuildable):
1. `pglPrimBuffer::Display`: rebind `GL_ELEMENT_ARRAY_BUFFER` before
   `glDrawElements` each Display (not just first lock).
2. EndPrims/MGL attrib-4 mapping: mapped=3/count=4 — determine which stage
   input fails to resolve to the sClientVBO under MGL's default-VAO
   materialization.
3. Alternative: stop depending on the MGL fork — direct Metal or Vulkan
   backend (see Vulkan scope analysis in chat, 2026-09-04).

### Session artifacts
- Run/console logs: `/tmp/shar_run_*.log` (this session: `shar_run_000633.log`).
  MGL fork clone: `/tmp/mgl-src` (master + externals, 2026-09-04).
- Crash reports: `~/Library/Logs/DiagnosticReports/Simpsons-2026-09-03-*.ips`
  (reporting stopped ~02:11; earlier ones are the authoritative crash trail).
- Crash watcher: hub process `crashwatch` (`/tmp/crash_watch.py`,
  seed `/tmp/crash_watch_seen.txt`).

## 2026-09-04 LATE UPDATE — MGL master reconstructed; game draws; visual bugs remain

The MGL fork is dead; **stock master was rebuilt for iOS-sim from `/tmp/mgl-src`
and the whole build is now fossilized in-repo** (`mgl-ios/`: 5 static `.a` in
`libs/`, iosbuild wrapper, `mgl-ios.patch`, README; `ios-project/rebuild_ios.sh`
one-shot rebuild+relink+install). App relinks against `mgl-ios/libs` via
`cmake -B build_ios -DIOS_MGL_LIB_DIR=$PWD/mgl-ios/libs`.

### Fix chain on master MGL (all in mgl-ios.patch / engine tree)
1. **mglGetString(GL_EXTENSIONS)** returned NULL → glad `get_exts` assert.
   Patch: return "" (get.c). [Engine-init crash #1]
2. **Light uniforms**: esToDesktopGLSL flattens `lights[i]` → `light0..3`;
   engine queried old names → -1 → MGL assert in mglUniform. Engine fix
   (glprog.cpp): query `light%d.*` on RAD_IOS + SetLightState no-ops on -1.
3. **VAO 0 NULL**: glBindVertexArray(0) left `state.vao=NULL`; the next
   glEnableVertexAttribArray printed 0x502 then **NULL-deref crashed** (MGL's
   ERROR_CHECK_RETURN logs but does NOT return). Fix: bind(0) materializes
   default VAO (getVAO(ctx,0)) — GL semantics.
4. **Detach after link**: glDetachShader cleared program->shader_slots →
   next pipeline rebuild asserted `vertex_shader` NULL. Fix: detach on a
   linked program is a no-op (GL semantics).
5. **EBO per-VAO**: mglBindBuffer(GL_ELEMENT_ARRAY_BUFFER) never wrote
   `vao->element_array.buffer` → EVERY indexed draw failed
   `validate_vao: element buffer missing`. Fix: mirror ELEMENT binding into
   the current VAO (GL semantics).
6. **Attrib constant vs buffer**: engine enables only attribs the mesh has;
   MGL asserts `mapped_buffers == count` (program's 4 inputs). Engine fix
   (glcon.cpp): RAD_IOS zero-pads missing attribs via shared sZeroVBO
   (retained Display mirrors the immediate EndPrims path).
7. **BufferMap cap**: uniform constants each get a GL buffer (uniforms.c lazy
   per-location) → 13 uniforms + 4 attribs = 17 > `max_vertex_attribs`(16)
   guard (array is actually MAX_MAPPED_BUFFERS=32). Fix: guard vs 31.
8. **"Suspicious buffer pointer" 4GB ceiling** (mgl_safety.h): rejected every
   pointer >4GB — on arm64 the whole heap is there → 3.7M errors/run, vertex
   uploads silently dropped. Fix: drop the high-end check (keep low/alignment).
9. **GPU-recovery death spiral**: swap commits the command buffer; when
   `nextDrawable` returned NULL the swap skipped minting a fresh buffer → next
   draw encoded on the committed buffer → recordGPUError → recovery latched →
   every draw failed → flicker. Fix: newRenderEncoder mints a fresh command
   buffer and retries on committed status; swap tail mints a buffer even when
   nextDrawable is NULL.
10. **bindTexturesToCurrentRenderEncoder**: NULL active texture (unbound
    sampler = legal GL, reads black) never decremented the loop counter →
    guard trip → GPU error. Fix: skip + decrement.

### Verified state (run18, Release, sim 969432DF)
- App boots, FE reaches `fe-gotoscreen id=14` (splash), stays alive
  indefinitely; `Recorded GPU error` = 0; asserts = 0; subdata errors = 0.
- Drawables present at 60-200fps. Screen: **stable two-tone cyan/white bands**
  = splash *background* art geometry renders, but foreground sprites
  (logo/text/buttons) are missing/not visible. One draw at splash still runs
  with NO texture bound on unit 0 (engine bug candidate: element whose pddi
  texture is NULL/0).
- NEXT: hunt why FE foreground sprite draws produce no visible pixels —
  candidates: (a) engine draws them with a NULL/0 texture → black-on-black
  output (probe: bind unit0 NULL happened once at splash), (b) viewport
  mismatch (engine 640x480 GL viewport vs 2621x5700 drawable) scaling sprite
  quads off-window, (c) draw ordering/z.

### Artifacts
- mgl-ios.patch now 522 lines (10 fixes). Console logs /tmp/shar_run1{4..8}.log.

---

## 2026-09-04 DEVICE+DEEP-RENDER SESSION — device build mechanics, real texture upload, PSO cache, flush fix; one visual bug remains

### State at session start
- Sim run18 was the known-good floor: app boots, FE splash two-tone
  cyan/white, 0 GPU errors. User asked for a **real-iPhone deploy**, then
  (device matched sim 1:1) for deep render debugging of the flat splash.

### Device build mechanics (fully worked out; reproducible)
1. Repo libs `mgl-ios/libs/*.a` are **single-slice**; sim & device MGL slices
   are both arm64 → `lipo -create` refuses (`same architectures (arm64)`).
   Swap whole-slice sets per platform: sim in `/tmp/mgl-staged`, device in
   `/tmp/mgl-staged-dev`. Both staged by the ninja merge rule of
   `/tmp/mgl-src/build/mgl` (sim) and `/tmp/mgl-src/build/mgl-dev` (device,
   `ios-device.toolchain.cmake`, iphoneos SDK).
2. The CMake Xcode tree is sysroot-baked: `build_ios/` was configured with
   `CMAKE_OSX_SYSROOT=iphonesimulator`. A device build MUST regenerate a
   separate tree: `cmake -G Xcode -B build_ios_dev -S . -DIOS=ON
   -DIOS_MGL_LIB_DIR=…/mgl-ios/libs -DSRR2_IOS_FFMPEG_DIR=…/ffmpeg_out_device
   -DCMAKE_OSX_SYSROOT=iphoneos -DSDL_SHARED=OFF -DSDL_STATIC=ON`.
   - `SDL_SHARED=ON` (default in a fresh tree) links the app against
     `@rpath/libSDL2-2.0.0.dylib` → **dyld launch crash on device**
     (`Library not loaded` / absolute dev-machine rpath). Sim tree had it OFF
     by luck of the original configure. Must be OFF for device.
   - `ffmpeg_out_device` exists next to `ffmpeg_out_sim` in
     `~/src/srr2_ios_builds/`.
3. Device build/sign/install:
   `xcodebuild -project build_ios_dev/SRR2.xcodeproj -scheme SimpsonsIOS
   -configuration Release -sdk iphoneos -destination 'generic/platform=iOS'
   build CODE_SIGNING_ALLOWED=YES CODE_SIGN_STYLE=Automatic
   DEVELOPMENT_TEAM=L82Y9H9ZXA CODE_SIGN_IDENTITY="Apple Development"
   -allowProvisioningUpdates`
   - Project Release configs carry `CODE_SIGNING_ALLOWED=NO` → MUST pass
     `CODE_SIGNING_ALLOWED=YES` on the CLI or the product comes out unsigned
     ("code object is not signed at all").
   - Team is **L82Y9H9ZXA** (the keychain cert "Apple Development: Alan David
     Cornejo Banderas (84VCLFGST3)" is stale/wrong; automatic signing with
     L82Y9H9ZXA fetched "iOS Team Provisioning Profile: *"). Signing identity
     strings still echo 84VCLFGST3 in xcodebuild output — ignore; verify with
     `codesign -dv` → TeamIdentifier=L82Y9H9ZXA.
   - `xcrun devicectl device install app --device 00008150-001928C80CA1401C
     build_ios_dev/code/Release-iphoneos/Simpsons.app` then
     `xcrun devicectl device process launch --device … com.c4rlox.simpsons.ios`.
   - Installed + launched on "Alan David's iPhone (27.0)". **App runs; screen
     identical two-tone to sim** → confirms renderer bug, not platform.
   - Crash logs from the device land in
     `~/Library/Containers/com.apple.dt.Devices/Data/tmp/Simpsons-*.ips`.
   - After device builds, restore sim slices into `mgl-ios/libs/` from
     `/tmp/mgl-staged/*.a` before any sim relink.

### MGL render fixes landed this session (all in /tmp/mgl-src source; engine edits in tree)
1. **Real texture bytes never reached Metal.** Every `replaceRegion:` upload
   (MGLRenderer.m ~1198–1445) was commented out ("crash Apple AGX driver");
   the creation tail filled textures with a 1×1 opaque-black pixel or an
   "emergency" checkerboard (which never visibly appeared). Fix: **MGLFIX**
   block in `createMTLTextureFromGLTexture:` — per-level committed
   buffer-blit (`copyFromBuffer` stride padded to 256, dedicated command
   buffer, `commit` + `waitUntilCompleted`, `status.error` checked) for plain
   RGBA8/GL_RGBA8 2D textures; other formats keep the legacy path. Only
   fires when `tex->faces[].levels[].data` is present (it is `vm_address_t` —
   cast through `uintptr_t`).
2. **Per-draw PSO recompile storm.** No pipeline cache existed; every dirty
   VAO/FBO/alpha transition regenerated + synchronously compiled an identical
   MTLRenderPipelineState (XPC round-trip on sim; main thread pegged; screen
   black for minutes). Fix: descriptor-keyed `_psoCache`
   (`psoCacheKeyForDescriptor:vertexDescriptor:` folds function pointers,
   color/depth/stencil formats, blend state, vertex attrib/layout fields;
   attribute loop bound is 31, NOT 32 — Metal asserts index 31).
3. **glFlush committed the frame mid-scene** → every later draw landed on a
   committed buffer, minted a fresh one (84,890× "render encoder on committed
   buffer" per run), and those buffers were never presented (33k× swap
   "already committed"). Fix: `flushCommandBuffer:` is a no-op for
   `finish=false` (Metal defers to swap commit); `finish=true` (glFinish)
   keeps the real barrier and then mints a fresh buffer+encoder.
4. **Engine vertex arrays were tightly-packed sections in ONE GL buffer**
   (stride 0 per attribute) — Metal layouts are per-buffer so MGL's shared
   layout ended at the last attribute's stride (colour = 4 B), corrupting
   fetches. Fix attempted engine-side: `pglContext::EndPrims` RAD_IOS block
   now **interleaves** pos3f+normal3f+uv2f+col4ub (36 B stride, one
   `glBufferSubData`, four `glVertexAttribPointer(…, 36, …)`); colour fallback
   for C-less streams is white (was zeros/transparent-black). NOTE: run46
   showed NO visual change — geometry was already deterministic, so the
   visible bug is elsewhere (see Open Questions). Fix is still correct GL;
   keep unless it regresses.

### Verification runs (sim UDID 969432DF-267F-4B28-8560-2502D085A3A6, bundle com.c4rlox.simpsons.ios)
- run30: PSO cache + flush fixes live; 0 mints / 0 swap-committed; ~30 fps
  steady; 239 textures with real bytes.
- run32: DRAWPROBE (env `MGL_DRAWPROBE`) — 198 unique textures bound in draws,
  real varied art incl. Simpsons-yellow/indigo icon+logo sets.
- run34: FRAMEDUMP (env `MGL_FRAMEDUMP=N`) — FE pages advance
  (20-draw → 372-draw → 374-draw static final page at ~30 fps, fbo=0x0,
  blits=0: draws go straight to the drawable).
- run35: SPRPROBE alpha tracking — all 20 splash polys reach vertex alpha
  255, white RGB, full-span UVs.
- run36: ROWAVG (env `MGL_ROWAVG`) — the 16 strip textures = legal page
  pieces; 64×64s = bright icon art.
- run38/39: raw texture dumps (`MGL_RGBAVDUMP`, files
  /tmp/shar_tex_*.rgba) → converted to PNG (`*_w.png`) → **confirmed the
  splash page = the real SHAR legal/copyright screen**: black bg, cyan
  Fox/Radical/Vivendi legal text lines, yellow "© 2003 Twentieth Century Fox
  Film Corporation.", Radical diamond logo group (yellow diamond, red fox
  silhouette, green banner). Composite: /tmp/page_expect.png.
- run43/45: TESTBLIT+TEXREAD (env `MGL_TESTBLIT=N` w/ `MGL_FRAMEDUMP` also set
  — the frame counter only runs under FRAMEDUMP) — GPU-side texture readback
  confirms real cyan legal-text bytes at (128,30)=#31dbe6ff in the MTLTexture.
  Upload path is definitively GOOD.
- run46: interleave fix — still flat two-tone post-fade.

### Remaining bug (single open item)
FE splash renders flat: top ~38% solid cyan (#66F6FF≈, = the legal text color
#47FFFF at ~50% store alpha) over white; no glyphs/logo detail anywhere;
bottom = white. Draw list every frame = ~374 textured draws at white/full
alpha directly on the drawable. Since CPU bytes AND GPU texture content are
verified real art, the collapse is between vertex fetch and fragment sample
per draw: each draw appears to sample one collapsed texel region (text rows
average into a solid cyan field; empty rows fall through white by alpha).
Candidate next probes (not yet run):
(a) per-draw `tc` value probe — dump the actual MTL vertex buffer contents +
    vertexDescriptor bound for one text draw (is tc constant per quad?),
(b) single-frame MTLCapture (.gputrace) for exact pipeline/UV inspection,
(c) sampler state (min/mag filter + address mode) vs mipmapped storage for
    the strip textures.

### Probes still in the tree (REMOVE when the bug closes)
- engine `libs/pure3d/p3d/sprite.cpp`: SPRPROBE (RAD_IOS, alpha-progression
  variant, per-poly lastA[4096], cap 2400, prints rgb+alpha).
- engine `libs/pure3d/pddi/gles/glcon.cpp`: ENDPRIMSPROBE (cap 30) +
  ALPHASTAT (every 250 streams) — plus the interleave rewrite itself (keep).
- MGL `MGLRenderer.m`: MGLFIX upload (KEEP — it is the real fix), MGLFIX
  ROWAVG/PPMDUMP/RGBAVDUMP/TESTBLIT/TEXREAD/FRAMEDUMP/BLITPROBE blocks +
  `_psoCache` (KEEP), `_testBlitTex/_testBlitDone/_frameDrawLog/…
  _drawProbe*` ivars. DRAWPROBE dedupe sets keep the per-program/per-texture
  one-time dumps (cap 60/400).
- MGL `textures.c`: TEXPROBE (env `MGL_TEXPROBE`, level 0, cap 400) — prints
  guest bytes at glTexImage2D.

### Artifacts
- Logs: /tmp/shar_run{26..46}.log (run30+ are the fixed-code era).
- Texture dumps: /tmp/shar_tex_*.rgba|*_w.png|*_k.png; composite
  /tmp/page_expect.png; screenshots /tmp/shar*.png.
- mgl-ios.patch needs a regeneration pass once probes are stripped.

## 2026-10-08 continuation: live renderer result

The simulator was restored and the current build was run on iPhone 17 / iOS 27
(`61AB94C5-4A22-4BBD-8250-F97EDA345549`). The viewport probe initially printed
zero dimensions because the diagnostic used floating-point formatting for an
integer array; the actual viewport is `2622x1206`. Blend disabled/enabled tests
did not change the result, and the command-buffer churn stopped after the
`glFlush(finish=false)` change.

The decisive failure was uniform reflection: all 13 vertex standalone uniforms
reported SPIR-V binding `0`, so modelview, projection, and lighting values were
aliased to one buffer. `program.c` now assigns each `UNIFORM_CONSTANT` resource
a stable binding before MSL generation. Missing optional uniforms receive a
zero-initialized default buffer, matching OpenGL's default-uniform behavior.

Verification after that fix:

- Simulator build, install, and launch succeed.
- The app reaches `gui-project-load-complete` states 2, 4, 6, and 7 and emits
  `fe-start` instead of looping on pipeline/map failures.
- Screenshots now contain real rendered front-end geometry/boot marks over the
  intended `FE_BLUE_SKY_COLOUR` background; the previous all-cyan artifact was
  replaced by changing scene content. This is a substantial renderer recovery,
  but the full legal/logo/menu composition still needs visual work.
- A signed device build was produced with the device MGL archive at
  `mgl-ios/work/iphoneos/libs`. Installation was attempted on the paired iPhone
  (`00008150-001928C80CA1401C`) and was refused only because the phone was
  locked; no renderer/device failure was observed.

The remaining visual issue is now after shader transform and pipeline setup:
some front-end elements/textures are absent or malformed even though geometry
draws. Continue with per-draw UV/texture sampling inspection rather than
viewport, blend, or command-buffer debugging.
