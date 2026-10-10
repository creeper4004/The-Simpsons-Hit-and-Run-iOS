# iOS streaming follow-up

The latest physical-phone logs contain occasional long frames. Existing frame
samples lack timestamps and world load events lack queue/load durations, so they
cannot establish which file caused the reported 2–3 second missing-ground episode.
Memory near the end was approximately 236–261 MB in frontend/pause contexts;
this is not a proof that all mission transitions are leak-free.

World Pure3D files use tFileFTT, not radLoadFileStream (used by RMS sound scripts).
The existing world stream is double-buffered. On iOS its fixed buffers are now
768 KiB each instead of 192 KiB: an additional 1.125 MiB globally, not per file.
Parsing/decompression yields every approximately 2 ms instead of 5.2 ms.
Yield checks are cooperative; a single expensive chunk can still exceed this.
These changes need physical-route comparison and do not establish that terrain
pop-in is fixed. Outgoing-zone deletion order has not been changed.

Always-on diagnostics now include:

- `load-timing`: filename, queue wait, load duration, queue depth and wall time.
- `p3d-stream`: bytes/position, cumulative I/O waiting, I/O and parser yields.
- Renderer frame reports: epoch/uptime and counts above 33.33/50 ms.

No visible FPS counter was added. Logs remain in the app's Documents directory
with the existing bounded retention. Compare the same drive and mission transition
on the phone; correlate region files with frame timestamps and memory samples.

## Full-map physical-phone follow-up

Captured the next run directly from the phone before relaunching:
`/tmp/shar-phone-latest/health.log` and `console.log` (session epoch 1791596797,
PID 1201, simulator=0). This is a different route/session, not a controlled A/B.

Examples of region queue/load delays in milliseconds:

| File | Queue wait | Load |
| --- | ---: | ---: |
| l1z3.p3d | 15467 | 170 |
| l1r3.p3d | 15564 | 833 |
| l1z4.p3d | 11052 | 4896 |
| l1r1.p3d (later visit) | 9881 | 2670 |

A small grandp_m.p3d file spent 11.27 seconds in cooperative file-wait scopes
and 12.63 seconds loading. These scopes include time parked while another thread
owns the loader mutex: they are **not measurements of raw storage latency**.
The previous sleep(0)/mutex race provides no guarantee the worker gets a turn.
The iOS loader now uses two semaphores to hand off each cooperative slice.
The mutex still protects serialized engine access, and termination wakes the
parked worker. Other platforms retain the original scheduling implementation.

225 frame reports associated with the last sampled gameplay context had a
median interval average of 18.85 ms; later driving intervals averaged 33–42 ms.
The existing counters cannot attribute those intervals specifically to collision
particles, physics, GPU execution or file loading. New `frame-phases` reports
aggregate simulation, rendering (including presentation waits), file service,
sound and loader CPU wall time once per second. They are profiling evidence,
not a claim that collision rendering is already optimized.

GPU errors at the session end explicitly report background submission being
forbidden; these occur after the background lifecycle event. They are not
proof that the driving effects caused a GPU crash.

### Runtime checks after explicit handoff

Both Release builds succeeded. The next real-phone session traversed l1z1,
l1r1, l1r7, l1z2, l1r2, l1z3, l1r3 and l1z4 with zero reported region queue
wait; loads ranged from 51–114 ms. l1z4 loaded in 53 ms, compared with 4896 ms
plus 11052 ms queue wait in the preceding full-map run. Different routes and
warm filesystem caches prevent treating this as a controlled speedup ratio.
Evidence: `/tmp/shar-phone-latest/handoff-health.log`.

The new phase reports identify render/presentation as the dominant remaining
frame cost (sample averages 18–21 ms), while sampled update/sound/file-service
costs are much smaller. On iOS, swap now presents and creates a fresh command
buffer without immediately acquiring the next drawable or starting an empty
render encoder. Drawable acquisition occurs when the next render pass needs it,
allowing subsequent game/file/sound work to overlap the previous GPU submission.
Render state is marked dirty so the next draw restores encoder state. The
existing null-drawable recovery is retained in the render encoder path.
Frame stats include drawable acquisition wait averages/maxima.

Successful per-object hash-table cleanup tracing is opt-in through
`MGL_TRACE_OBJECT_CLEANUP`; the captured console had 3797 lines containing
"Metal cleanup". Error logging and actual cleanup remain active.

Existing simulator GPU regression: **24/24 pixel assertions passed** after
these rendering changes, with persistent health records present. Artifacts:
`/tmp/shar-deferred-ui-check/`. This does not reproduce the phone collision
route or prove that all driving frame drops are fixed.
