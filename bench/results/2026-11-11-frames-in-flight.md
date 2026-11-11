# Frames in flight, 2026-11-11

Change: `VulkanRender.cpp` submits a frame without waiting and keeps up to `OWE_FRAMES_IN_FLIGHT` (default 2, 1 = the old
behaviour) submitted frames in a queue; the oldest is waited for when a new frame starts. No vrento change: vrento already
tracks every submission by timeline serial with its own fence, so the one-frame limit was owe's single `m_frame`/`m_output`
and the `wait` right after submit.

## What actually serialized the loop

Removing the wait was not enough (night-city gained 3 %). The wait had moved into `prepare`/`draw`: every frame began with
`waitForPreparedUploads`, which commits the frame's texture uploads and waits for them. They are submitted behind the previous
frame, so the wait was a wait for the previous frame's GPU work. With frames in flight the uploads are committed but not waited
for (the queue and the texture states order them before the frames that sample them); the blocking wait stays when nothing is
in flight (loading). Scene/graph changes, swapchain changes, render-target reconfiguration and shutdown drain the queue first.

## Offscreen, 40 wallpapers, 1920x1080, 4070 (interleaved A/B, 2 rounds, medians; `bench/ab.py`)

Raw: `2026-11-11-frames-in-flight-ab-1080p.json`. `base` = the viewer before this change, `d1`/`d2` = this build at depth 1/2.
Other threads ran GPU jobs during the sweep (their locks differ from ours), hence interleaving.

- ms/frame, depth 2 vs base: **median -25 %**, mean -25 %, best -52 % (mermaid-audio), 36 of 40 faster by more than 5 %, 4 within
  5 %, **none slower by more than 5 %** (worst +4.8 %, more-physics, CPU-bound). Examples (ms, base -> d2): night-city 5.09 ->
  3.25, rain 5.72 -> 3.79, china-town 5.60 -> 2.92, reze 7.41 -> 4.86, bleach 5.49 -> 4.26, lofi-cafe 6.06 -> 3.92,
  sculpture-forest 6.96 -> 4.13, winter 4.78 -> 2.70, elden-ring 3.19 -> 2.23.
- CPU-bound wallpapers, 5 rounds: littlebigplanet 15.5 -> 14.6 median, ocarina-of-time 33.4 -> 34.0, pixels-100-text 5.8 -> 5.4:
  no change beyond noise (their cost is CPU, there is no GPU wait to hide). more-physics 213 -> 223 (2 rounds, CPU-bound, +5 %, within noise).
- Depth 1 vs base: +3 % median (within the run-to-run noise of about 5 %, but note it; depth 1 is only the old code path plus an env read).
- Stage timers, night-city (800 frames, unpaced): frame p50 4.93 -> 2.86 ms (depth 2), 2.84 ms (depth 3); `wait` p50 3.2 ms -> 1.07 ms
  (the remaining wait is the GPU being the bottleneck). Depth 3 is no better than 2 in any run: use 2.
- RSS: median 0 %, worst +4.2 %. VRAM: the sampled nvidia-smi totals include other threads' processes in this sweep and
  are not usable (e.g. snow 401 -> 1274 MB is noise); no per-frame render targets were added, extra cost is two semaphores,
  a command buffer and a fence per frame in flight.

## Re-run on the rebased branch (perf-bench with the frame-graph and fence-reuse changes in vrento), depth 1 vs 2 vs 3

The branch was rebased onto a `perf-bench` that gained vrento changes (cached graph bindings, fence reuse in `TextureContext::Submit`)
while this was being measured. Rebuilt on top of it (vrento-perf working tree as of 2026-11-11 ~01:00, uncommitted fence-reuse edits
included): captures still byte-identical (35 of 35 deterministic, same hashes as before), validation clean, and the full A/B
(`2026-11-11-frames-in-flight-ab2-1080p.json`, 2 rounds, interleaved, d1 is the baseline) gives depth 2 vs depth 1: **median -36.5 %,
mean -32 %, best -66 %, 36 of 40 faster by more than 5 %**; depth 3 median -36.5 % too. Only puppet-warp-test is "slower" (0.13 -> 0.14-0.23 ms:
a 0.1 ms difference on a 0.13 ms frame, noise). night-city 4.67 -> 2.92 ms, rain 5.31 -> 3.03, reze 7.07 -> 4.04, china-town 4.53 -> 3.10,
ocarina 32.9 -> 31.5, more-physics 212.9 -> 208.1. These are the numbers to use; the first sweep above (vs. the pre-rebase binary) shows the same pattern.

## Windowed (swapchain + present on the desktop, FIFO, 60 Hz display, `bench/windowed.py`, 15 s, fps 60)

Raw: `2026-11-11-frames-in-flight-windowed.json`. The display refreshes at 60 Hz, so presented fps is vsync-bound: every
configuration, including depth 1, dropped to ~45-50 fps with p95 33 ms (missed vsyncs; other threads were loading the GPU and
the compositor was drawing a 5120x1440 desktop), so frame rate did not differ. What changed: the render thread's wait for the
GPU went from 2-4 ms mean (rain 2.2, reze 4.0, night-city 3.9, nebula 0.9) to 0.00, and process CPU went down (night-city 30.6 ->
26.1 %, reze 37 -> 33 %, rain 27.5 -> 26.1 %). ocarina-of-time (CPU bound, 98 % of a core): 22.3 -> 23.7 (d2) -> 24.6 (d3) fps.
Not measured: the Waywallen renderer (external swapchain / sync-fd export) end to end; it uses the same `drawFrameOffscreen`
path that the capture runs exercise (acquire, export fd, complete) but not a real consumer and its buffer pool size.

## Correctness

- `bench/identical.py`: the same frame captured at depth 1/2/3 is **byte-identical for every deterministic wallpaper (34 of 34;
  the other 6 differ between two runs at depth 1 already: they read the wall clock)**.
- Khronos validation + synchronization validation (`--valid-layer`, `VK_LAYER_VALIDATE_SYNC=1`) at depth 1/2/3 on night-city,
  rain, jinx-puppet, snow, more-physics, sculpture-forest, street-japan-audio, littlebigplanet: no validation or hazard message
  that depth 1 does not also print (none printed).
- Parity vs the Wallpaper Engine references (`owe_compare.py`, 71 wallpapers scored, 12 skipped): scores of the base viewer and
  this build (depth 2) are identical for all 71 (ssim to 1e-4, same ok/near/differ status; 70 ok, 1 differ in both). Outputs
  (side-by-side.png per wallpaper): `fif-work/parity-out-{base,new}/`. Not re-run in the VM: the references already exist and the
  captures are byte-identical, so no new VM capture can differ; audio-exclusive wallpapers were only covered by the byte-identical
  capture of street-japan-audio.

## Latency and risks

- Latency was not measured directly. In a vsync-paced wallpaper the GPU finishes inside the interval, the frame is presented right after
  submit (as before), so input-to-display is unchanged. When GPU-saturated, a frame can queue behind up to depth-1 others: at most one extra
  GPU frame time (about 3-6 ms for the heavy scenes above, not a full 16 ms) of added age; the audio spectrum is read at the start of
  the frame so it is that much older at display. Use `OWE_FRAMES_IN_FLIGHT=1` to compare.
- `OWE_GPU_TIMING` forces depth 1 (its timestamps assume one frame).
- Per-frame uploads are no longer waited for while frames are in flight; correctness relies on vrento's texture state tracking (validated above).
- The unmeasured Waywallen consumer needs a buffer pool of at least `depth + 1` buffers to keep two frames in flight; with fewer
  the acquire fails and the tick is skipped (no error, just no overlap).
- Overlap with the frame-graph rebuild thread: none in vrento; this touches only `VulkanRender.cpp`, `SceneViewer.cpp` (OWE_EXIT_AFTER) and `bench/`.

## Follow-up (same day): pacing, present mode, locks, Waywallen analysis

- **Frame timer pacing defect (found, fixed, unit-tested):** `ThreadTimer` waited a full interval *after* each callback returned, so the period
  was interval + frame time (a 4 ms frame at 60 fps = 20.7 ms = 48 fps; the windowed runs above show 45-50 fps at a 60 target, depth 1
  included, which is this and not frames in flight). A 120 fps target (8.3 ms) with a 3 ms frame would run at ~88 fps. Callbacks now start one
  interval apart (an overrun of more than an interval restarts the schedule); test `ThreadTimer.StartsCallbacksOneIntervalApartNotOneIntervalAfterTheLastEnds`
  (12 ms callback on a 20 ms interval: 44-52 calls/s, the old code gave 31). Unpaced captures are unaffected (1 us interval); only paced
  runs (`--capture-realtime`, windows, Waywallen's timer) change. Effect on the real frame rate: not yet measured (bench-lock queue).
- **`OWE_PRESENT_MODE=fifo|mailbox|immediate|relaxed`** picks the window swapchain's present mode (default fifo, unchanged), to measure
  how fast a window can be drawn without waiting for the display. Not yet measured.
- **Queued-frames counter** (`queued` stage of `OWE_PERF_LOG`, in frames): how many earlier frames the GPU has not finished when a frame is submitted,
  i.e. the display latency a deeper queue adds, in frames of GPU time. Not yet measured.
- **Bench locks:** `bench/run.py hold_locks()` takes `bench.lock`, `bench-gpu.lock`, `perf-bench.lock` (blocking, in that order, the order other
  jobs chain their `flock`s in) and is called by run.py, ab.py, windowed.py, stages.py. A first version that took them one by one in another
  order deadlocked with another thread's A/B.
- **Waywallen path, by code reading (not run end to end):** `BridgeProducerCore` allows one pending slot at a time (`m_have_pending`): a slot is
  acquired when a frame starts and handed to the consumer with the render-done sync fd right after the GPU submit (`submitSlot`), then the next
  frame acquires another slot. With frames in flight the slot is therefore released to the consumer at submit, exactly as before, and the sync fd
  (exported from the frame's signal semaphore, valid with its signal pending) is what the consumer waits on. Overlap needs a free slot for
  each frame in flight: `acquireSlot` returns Busy when the consumer holds all of them and the tick is skipped (no error, no overlap). The daemon's
  pool is `directive.count` slots (3 in its tests), enough for depth 2 while the consumer holds one. Untested because a real run needs the daemon,
  a display client speaking the bridge protocol and a scene package; the existing `waywallen/tests/display.rs` harness drives the Rust test renderer,
  not `waywallen-wescene-renderer`. To try on the desktop: `OWE_FRAMES_IN_FLIGHT=1` vs `2` in the plugin's environment and watch for stutter.
