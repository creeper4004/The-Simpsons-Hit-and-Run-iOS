# iOS MGL Debug Kickoff

Paste this brief into the next session:

> Continue the iOS Simpsons Hit & Run renderer investigation using the existing
> MGL OpenGL-on-Metal path. Do not switch to MoltenVK. Read the 2026-09-26
> update and the September 4 historical notes in `IOS_MGL_RENDER_HANDOFF.md`
> first. Preserve the uncommitted worktree.
>
> Current target: iPhone 17 / iOS 27 simulator
> (`61AB94C5-4A22-4BBD-8250-F97EDA345549`), bundle
> `com.c4rlox.simpsons.ios`. MGL source is restored at `mgl-ios/src/mgl` with
> the saved patch applied at commit `de0ded0`. `mgl-ios/libs` currently holds
> simulator archives. A Debug app already builds at
> `build_ios/code/Debug-iphonesimulator/Simpsons.app`.
>
> Let the known long asset load finish before taking a screenshot. The loaded
> page currently renders as cyan/white bands and stretched horizontal strips.
> A live console run reported repeated `MGL ERROR: Critical error in
> program.c at line 814`: MGL's `mglBindAttribLocation()` is unimplemented,
> while `pglProgram::LinkProgram()` calls it for position, normal, texcoord,
> and color before linking. The engine's iOS GLSL translator injects explicit
> input locations in declaration order; verify that order against every
> runtime shader before deciding whether to implement the API or bypass those
> calls. The error is confirmed, but not yet proven to cause the image bug.
>
> Existing `ENDPRIMSPROBE` output shows CPU-side positions and varying UVs;
> `ALPHASTAT` shows alpha reaches 255 after the fade. Neither probe verifies
> the actual Metal vertex fetch or texture sample. First fix/verify attribute
> locations in isolation, then inspect one affected draw's MGL buffer and
> Metal vertex descriptor. Use a one-frame Metal capture if those disagree.
> After vertex fetch is known correct, inspect the bound texture bytes,
> sampler, and sampled texel. The old September 4 texture-readback evidence
> came from a temporary MGL fork and does not establish the behavior of the
> currently rebuilt source.
>
> Build the app with:
> `cmake -B build_ios -DIOS_MGL_LIB_DIR="$PWD/mgl-ios/libs"`
> `xcodebuild -project build_ios/SRR2.xcodeproj -scheme SimpsonsIOS -configuration Debug -sdk iphonesimulator -destination 'platform=iOS Simulator,id=61AB94C5-4A22-4BBD-8250-F97EDA345549' build`
>
> To force a simulator MGL archive rebuild, use
> `FORCE=1 JOBS=8 mgl-ios/iosbuild/build.sh`. The default fast path skips it
> when archives exist. Capture the loaded screenshot and filtered console
> output under `/tmp` for each controlled change. Keep probes narrow and
> compare against the current screenshot before changing another layer.

The matching simulator screenshots and first-launch unified log from
2026-09-26 are under `/tmp/shar-ios-debug-20260926-*` on the current machine.
