# iOS FE Black Screen — Handoff Summary

## 2026-10-09 status

The save path now selects the iOS writable SDL drive and has passed two
simulator write/commit/readback checks. This proves file persistence; the
save-menu touch navigation still needs a live UI repro. Bounded opt-in save and
render diagnostics are documented in `IOS_RUNTIME_DIAGNOSTICS.md`. The user's
latest screenshots still show black panel regions and 3D characters covering
menu text; these visual defects remain open. See the latest status at the top
of `IOS_MGL_RENDER_HANDOFF.md`. The current diagnostic build was not installed
or launched on the physical phone.

## 2026-10-08 update

Legal artwork and the complete living-room menu render in live captures, with
the menu also confirmed by the user on the phone. Touch buttons now render after
the OpenGL-to-Metal depth conversion. A New Game buffer-deletion crash and missing
iOS virtual-controller integration are repaired. Touch movement and restored
ground/buildings are confirmed on the phone and simulator. The retained mesh
upload now preserves the populated vertex count across attribute/index locks.
Pause Settings asset-row mismatch is repaired and confirmed by the user.
Latest phone menu uptime is 9.8 s and New Game loading is 17.9 s, compared with
about 79–88 s in earlier level-loading runs. A HUD map placement defect is in
live verification. Read the latest
`IOS_MGL_RENDER_HANDOFF.md` update and the capture
README before following the historical notes. CoreSimulator permissions work.

## 2026-09-26 update — frame presents, but loaded artwork is malformed

The current iOS Debug build runs on the iPhone 17 simulator and completes the
known long load, but the legal/front-end page appears as cyan and white bands
with horizontal stretching. A live console run also exposed repeated calls to
MGL's unimplemented `glBindAttribLocation`. The current build, screenshots,
runtime evidence, and next debugging sequence are in
`IOS_MGL_RENDER_HANDOFF.md`; a concise restart prompt is in
`IOS_MGL_DEBUG_KICKOFF.md`. The older updates below are historical.

## 2026-09-03 initial render-path bring-up — black screen moved into MGL debugging

The black screen was ultimately a **video-layer** problem, not the FE logic below.
This session brought the GL path up on MGL (OpenGL-on-Metal): glad table +
symbol resolution + MGL iOS stubs + ES1→desktop GLSL translation + VBO/immediate
draw fixes. The app now builds clean (Release), installs, runs indefinitely
(alive 70s+, no crash), and renders a dark front-end frame at 2622×1206 —
the FE asset chain (`language.p3d`→`bootup.p3d`→`backend.p3d`,
`gui-project-load-complete`) runs to completion in the logs. The remaining
visual gap (dim content; runtime-material draws skipped on empty resource
lists) is tracked in `IOS_MGL_RENDER_HANDOFF.md` §Open items. The chronology
below documents the FE/game-logic layer this supersedes.

---

Date: 2026-08-31. Simulator debug of the iOS port's black front-end screen.

## 2026-09-02 update — root cause fixed; new crash unmasked

The "bootup context restarts" stall was NOT the real blocker. The actual deadlock
was the `m_bAsyncLoading( false )` iOS special-case in
`FeResourceManager::FeResourceManager()` (libs/scrooby/src/ResourceManager/FeResourceManager.cpp).
With async off, `P3DCallback::Done()` no longer calls `ContinueLoading()`, and the only
remaining pump is `FeApp::DrawFrame()` — which does NOT run during bootup (the GUI render
layer is frozen via `FreezeCorpse()` until a project loads). So the first Scrooby project
(`language.p3d`) loads its `.p3d` file (`fe-add-project`) but `ContinueLoading()` never
re-runs to resolve it → `gui-project-load-complete` never fires → black screen.

**Fix (kept):** reverted `m_bAsyncLoading` to `true`. The async pump
(`P3DCallback::Done()` → `ContinueLoading()`) advances the chain. The basename /
`RT_PROJECT`→`m_Projects` fallbacks already added in `ContinueLoading()` are sufficient
for the image/project name-resolution mismatch — disabling async was unnecessary.

**Verified with the fix** (fresh launch, Release sim):
- `language.p3d` loads and fires `gui-project-load-complete state=2`.
- `bootup.p3d` loads (`gui-project-load-complete state=4`), license screen shown,
  `quit-bootup` fires, transition to `context=2` (FRONTEND) succeeds.

**New crash (the actual next blocker):** intermittent SIGSEGV in the radLoad load thread,
`radLoadManager::InternalService()` at `m_pCurrent->AddRef()` (use-after-free / dangling
`QueueItem` vtable), while loading the bootup loading-screen resources
(`art/frontend/scrooby/pages/Loading.pag` and friends, `sec=ScroobyBootup`). Crash dump
is in `Documents/srr2_crash.txt` (signal 11, load thread `LoadThreadEntry`). Flaky race —
one launch survived ~20s to `context=2`, next launch crashed at the same load. Suspect the
`ContinueLoading()` data race: it now runs on BOTH the load thread (via `P3DCallback::Done`)
and the main thread (via `FeApp::LoadProject`), mutating the non-thread-safe `rVector
<ResourceEntry*> mResources`. Next step: serialize `ContinueLoading()` (or make
`mResources` access thread-safe) and re-test.

## 2026-09-02 (later) — `.pag` crash was a section mismatch; now front-end never starts

The load-thread SIGSEGV above was **NOT** a data race. It is a deterministic NULL file
loader: `radLoadManager::InternalService()` does `loader = m_pFileLoaders->Find(ext)`, gets
NULL for `.pag` (no `.pag` file loader is registered on any platform — pages are embedded in
the `.p3d` chunks), then `loader->LoadFile()` dereferences NULL (`rAssert` is a no-op in
Release). The `.pag` lookup was only reached because the page wasn't found in the inventory
in the **wrong section**: `Loading.pag` (backend, `ScroobyBackend`) was looked up in
`ScroobyBootup` because `FeApp::RemoveProject()` clobbers the resource manager's *global*
section during the bootup→frontend transition.

**Fixes applied (all kept):**
1. `m_bAsyncLoading` → `true` (async pump; the real deadlock fix).
2. `FeApp::RemoveProject()`: save/restore `m_pInventorySection` around `RemoveResource()`
   so the bootup release no longer clobbers the section while backend/frontend are loading.
3. `BootupContext::OnUpdate()`: `bootLoadReady = m_bootupLoadCompleted` (not forced true).

**Result after these:** all Scrooby projects load in their correct sections
(`language.p3d` state=2, `bootup.p3d` state=4, `backend.p3d` state=8, `frontend.p3d`
state=8), pages resolve, `context=2` (FRONTEND) reached, and the **touch HUD renders and is
interactive**. The `.pag` crash is gone.

**Remaining blocker — the splash never starts (black game screen behind the touch UI):**
`FrontEndContext::OnStart` runs (`fe-onstart prev=1 feMgr=0x0` → front-end manager NULL) and
queues `GUI_MSG_INIT_FRONTEND` + `AddCallback(this)`. But `OnProcessRequestsComplete`
(`fe-proc-complete`) / `StartFrontEnd` (`fe-start`) never fire, so the front-end manager's
`Start(GUI_SCREEN_ID_SPLASH)` is never called and the current screen stays an unnamed/black
screen. Root cause is the **PAL language flow**: `CGuiSystem::Init()` loads `language.p3d`
first (LANGUAGE_LOADING); `OnInitBootUp()` (which queues bootup/backend/frontend projects)
is deferred until the language selector completes, so it runs *after*
`BootupContext::OnStart` has already queued the cars/chars requests and the
`AddCallback(this)` marker. The bootup callback marker therefore fires (sets
`m_bootupLoadCompleted=true`) **before** the Scrooby projects finish, the bootup→frontend
transition happens early, and the frontend `.p3d` load trips `FeApp::LoadProject`'s
`m_ProjectLoadPending` guard (returns early, load never starts) — the LoadingManager then
never reaches the FE callback. Next step: rework the bootup→frontend handoff so the
transition (and `StartFrontEnd`) waits for the frontend Scrooby project to actually load,
or make `FeApp::LoadProject` queue (not drop) a request while `m_ProjectLoadPending`.



## Current build state
- Compiles (`BUILD SUCCEEDED`), installs, launches, process stays alive (no crash log).
- App reaches `context=1` (BOOTUP) on a fresh data container and stays there — it does NOT reach
  `context=2` (FRONTEND). Screen renders black (`avg=0.0`).
- Every `simctl install` recreates the data container → assets re-extract (~60s, 2.0G) → `bootLoad=0`.

## Root cause chain (confirmed by diag + lldb)
1. **First Scrooby project (`art/frontend/scrooby/language.p3d`) never resolves.**
   `FeResourceManager::ContinueLoading()` looks resources up with `p3d::find(resource->GetName())`
   (full path). `FeProjectChunkHandler::LoadObject()` creates the `FeProject` and registers it in
   `FeApp::m_Projects` (`FeApp::AddProject`), storing it under **basename** (`FeEntity::SetName(PascalCString::FullFilename())`
   → `language.p3d`). The full-path `p3d::find` misses → `entity==NULL` → `ContinueLoading()` hits its
   `break` and re-queues forever → the project is never marked `RM_LOAD_COMPLETE`.
2. This stalls the whole GUI bootup chain: `gui-project-load-complete` never fires, `bootLoad` stays 0,
   the bootup→FE transition never runs, and the app renders a black screen.
3. The `png-create-image name=language.p3d` console spam (760 lines) was `ContinueLoading` re-queuing
   the same unresolved project, decoding it repeatedly.

## Fixes kept in this build (safe, targeted)
- `code/contexts/bootupcontext.cpp`
  - `ResetLicenseScreenDisplayTime()` seeded in `OnStart` (license screen never fires it on iOS).
  - On iOS: `soundReady=true`; `bootLoadReady=true` — the bootup→FE transition gate no longer blocks on
    `m_bootupLoadCompleted`/`m_soundLoadCompleted` (assets are extracted before OnUpdate pumps).
- `code/presentation/gui/bootup/guimanagerbootup.cpp`
  - On iOS: `s_memcardCheckState = MEM_CARD_CHECK_COMPLETED` in the ctor (no memcard hardware; avoids the
    empty `MEMORY_CARD_CHECK` FE screen).
  - On iOS in `GUI_MSG_QUIT_BOOTUP`: force `m_state = GUI_FE_SCREEN_RUNNING` and clear the screen queue so
    the `WINDOW_FINISHED` → `StartMovies()` → `SetContext(CONTEXT_FRONTEND)` handoff can complete.
- `code/presentation/gui/bootup/guimanagerlanguage.cpp` — `isLanguageSupported = true` on iOS (skip the
  language selector whose auto-select race drops `GUI_MSG_LANGUAGE_SELECTION_DONE`).
- `libs/scrooby/src/ResourceManager/FeResourceManager.cpp` (`ContinueLoading`)
  - On iOS: the image handler is set to `SetFullName(false)` so standalone images store **by basename**
    (e.g. `bomb`).
  - Expanded entity fallback: if `p3d::find(full_path)` misses, try basename, then basename-sans-extension,
    then (for `RT_PROJECT`) search `FeApp::m_Projects` by `GetFileName()`/`GetName()`.
- Diagnostic (`iOSProgress`, writes `Documents/srr2_diag.txt` + stderr) spread across boot/extract/load/render.
  Spammy per-frame `png-create-image`/`png-decode-done` removed; `fe-render`/`scrooby-drawframe` rate-limited
  to every 300 frames to keep the trace readable.

## Verified observations
- `fe-add-project prj=0x…` fires once for `language.p3d` → the RT_PROJECT fallback resolves it.
- After `fe-add-project`, the diag stops (no `bootup.p3d` load, no `gui-project-load-complete`) and the
  bootup context re-seeds (`bootup-onstart-seed` reappears) — the bootup context restarts instead of
  advancing to `bootup.p3d`.
- Main thread confirmed alive and in GL swap (`glgConvertTo_32` → `presentRenderbuffer`) — loop runs, renders black.

## The open blocker (next session)
After the language project resolves, the **bootup context restarts** instead of loading the next project
(`bootup.p3d`). Diagnosis of that restart:
- It resembles the known **FRONTEND-release `dynamic_cast` crash**: a `Scrooby::Project*` is cast in
  `CGuiSystem::HandleMessage` (`GUI_MSG_RELEASE_FRONTEND` → `OnReleaseFrontEnd`) after the project was
  freed/NULLed. On iOS the release ordering / project lifetime differs.
- Next step A: trap why the bootup context re-inits (diag at the `BootupContext::OnStart` call site and the
  `GameFlow` context push to see what re-triggers it after `fe-add-project`).
- Next step B: fix `OnReleaseFrontEnd` / the FE project cast to guard against a NULL/freed `Scrooby::Project*`.

## Simulator / build facts
- Sim UDID `969432DF-267F-4B28-8560-2502D085A3A6` (iPhone 17 Pro); bundle `com.c4rlox.simpsons.ios`.
- Build: `./ios-project/build-ios.sh` (Release sim) → `build_ios/code/Release-iphonesimulator/Simpsons.app`.
- A `CONFIG=RelWithDebInfo` build keeps `RAD_RELEASE` and adds debug symbols (`-g`) for lldb inspection.
- Assets bundled into `Simpsons.app/game_assets/`, extracted to `Documents/` by `GameAssetExtractor`.
