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
