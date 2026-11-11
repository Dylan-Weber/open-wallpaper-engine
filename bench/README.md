# bench: performance benchmarks for open-wallpaper-engine

Goal: every wallpaper at 60 fps on the 4070 (1080p and the 5120x1440 desktop) with minimal CPU, GPU and memory use;
then up to 120 fps. These tools find where the time goes and catch regressions. Profile first, change second, then
profile again.

    run.py        the benchmark: steady-state ms/frame, CPU ms/frame, RSS, VRAM, GPU use, per wallpaper; --baseline
                  FILE fails on a regression
    stages.py     one wallpaper's per-stage CPU breakdown (and per-pass GPU breakdown) from the viewer's perf log
    wallpapers.json   the wallpapers measured, by category (simple, particles, puppet, 3d-shadow, effects, text, scripts,
                  audio, physics)
    results/      JSON of each run (and baseline-*.md: the analysis of the run)

## Rules for numbers

- One viewer at a time, nothing else heavy running (a build, the Windows VM, a browser with video) and the machine out of
  swap: `free -g`, `cat /proc/loadavg`.
- The viewer takes the first Vulkan device the loader lists, which on this machine is the Intel iGPU. `run.py` pins
  the 4070 through `VK_DRIVER_FILES` (`--gpu nvidia`, the default); a number taken on the iGPU is 10-30x slower.
- Offscreen capture (`--capture`, no window, no frame pacing) draws as fast as it can: `fps max` is the capacity
  of the render loop, not what a desktop shows. `--paced N` adds N seconds at the real frame rate with realtime
  pacing, giving the CPU and GPU use of a running wallpaper (without a window or present, so it omits the compositor
  and presentation cost). `CaptureSurface::read` (a `vkQueueWaitIdle` and readback) runs once, at the captured frame:
  it shows up in a profile that ends with the run, not in steady state.
- Timing is the difference of a short and a long run, so loading, shader compilation and the final readback cancel.
  A run is noisy at the 5 % level; repeat before believing a smaller change.
- Builds: `PATH=/usr/lib/llvm-22/bin:$PATH lito build -p owe-sceneviewer --profile release -j 4` under
  `systemd-run --user --scope -p MemoryMax=10G -p MemorySwapMax=0` and a `heavy-N` lock in `~/.cache/wpe-locks`.

## Commands

    bench/run.py --label baseline                      # all of wallpapers.json at 1920x1080
    bench/run.py --size 5120x1440 --label desktop      # the desktop resolution
    bench/run.py --paced 5 --only rain winter          # CPU and GPU use at the frame rate
    bench/run.py --baseline bench/results/baseline-1080p.json   # exit 1 if a wallpaper got slower or bigger

    # the stage timers (OWE_PERF_LOG) and the per-pass GPU timestamps (OWE_GPU_TIMING=1):
    bench/stages.py ocarina-of-time --frames 300
    OWE_GPU_TIMING=1 bench/stages.py night-city --size 5120x1440

## Profiling recipes

    # CPU, steady state only (skip loading with -D, ms), flat:
    perf record -e cpu-clock -F 999 -D 6500 -o x.data VIEWER ... --capture-frame 230 ...
    perf report -i x.data --stdio --no-children --sort sym -g none | head -40
    # CPU call stacks: a build with frame pointers (the dwarf unwinder fails through the modules):
    CXXFLAGS=-fno-omit-frame-pointer CFLAGS=-fno-omit-frame-pointer lito build -p owe-sceneviewer --profile release \
        --use-env-flags --build-dir build/prof
    perf record -e cpu-clock -F 999 -D 6500 -g ...; perf report --children --sort sym -g none --percent-limit 3
    # JavaScript: results/ has the cannon-es node vs QuickJS comparison method (baseline-*.md).

`perf` needs `kernel.perf_event_paranoid=1` (`sudo sysctl kernel.perf_event_paranoid=1`, until reboot).
