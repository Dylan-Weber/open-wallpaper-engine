#!/usr/bin/env python3
"""Benchmark of the presented path: SceneViewer in a real window (swapchain, acquire, present, compositor).

    bench/windowed.py [--viewer PATH] [--seconds 20] [--fps 60] [--only NAME...] [--in-flight 1 2 3] [--out FILE]

Each wallpaper runs for --seconds in a window on the current desktop session (WAYLAND_DISPLAY/DISPLAY must be set; a
window appears) at --fps, on the 4070. The viewer closes itself (OWE_EXIT_AFTER) and writes its stage timers
(OWE_PERF_LOG). Per wallpaper and OWE_FRAMES_IN_FLIGHT value this reports the presented frame rate, the frame interval
p50/p99 (the `frame` stage is the CPU time of a draw, so the interval comes from the frame count), CPU use of the
process, the time the render thread waits for the GPU (`wait`) and for the next frame, peak RSS and VRAM.
The swapchain is FIFO: with --fps above the display's refresh rate the frame rate is the refresh rate, so compare
CPU use and `wait` at a fixed --fps rather than the frame rate itself.
"""
import argparse
import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run  # noqa: E402


def once(viewer, entry, seconds, fps, depth, gpu_env):
    log = '/tmp/owe-windowed-%d.json' % os.getpid()
    cache = os.path.join(os.path.expanduser('~/.cache'), 'owe-bench-shaders-windowed')
    os.makedirs(cache, exist_ok=True)
    cmd = ['systemd-run', '--user', '--scope', '-q', '-p', 'MemoryMax=' + run.MEMORY, '-p', 'MemorySwapMax=0',
           viewer, '-C', cache, '--fps', str(fps)] + entry.get('args', []) + [run.ASSETS, run.package(entry)]
    env = dict(os.environ, OWE_PERF_LOG=log, OWE_EXIT_AFTER=str(seconds), OWE_FRAMES_IN_FLIGHT=str(depth), **gpu_env)
    start = time.monotonic()
    with open('/dev/null', 'w') as null:
        proc = subprocess.Popen(cmd, env=env, stdout=null, stderr=null)
        sampler = run.Sampler(proc.pid)
        sampler.start()
        while True:
            done, status, usage = os.wait4(proc.pid, os.WNOHANG)
            if done:
                break
            if time.monotonic() > start + seconds + 120:
                proc.kill()
            time.sleep(0.01)
    wall = time.monotonic() - start
    sampler.stop.set()
    sampler.join()
    if not os.path.exists(log):
        return None
    with open(log) as f:
        data = json.load(f)
    os.remove(log)
    return dict(wall=wall, cpu=usage.ru_utime + usage.ru_stime, rss_mb=usage.ru_maxrss // 1024, gpu_mem=sampler.gpu_mem,
                gpu_util=sampler.gpu_util, **data)


def main():
    run.hold_locks()
    ap = argparse.ArgumentParser()
    ap.add_argument('--viewer', default=run.DEFAULT_VIEWER)
    ap.add_argument('--seconds', type=int, default=20)
    ap.add_argument('--fps', type=int, default=60)
    ap.add_argument('--only', nargs='*')
    ap.add_argument('--in-flight', nargs='*', type=int, default=[1, 2])
    ap.add_argument('--out')
    ap.add_argument('--load-s', type=float, default=0, help='seconds of loading to subtract from the wall time (default: from the frame count)')
    args = ap.parse_args()
    with open(os.path.join(run.HERE, 'wallpapers.json')) as f:
        entries = [e for e in json.load(f)['wallpapers'] if not args.only or e['name'] in args.only]
    gpu_env = {'VK_DRIVER_FILES': run.ICDS['nvidia']}
    idle = run.gpu_sample()
    idle_mem = idle[0] if idle else None
    results = []
    for e in entries:
        for depth in args.in_flight:
            r = once(args.viewer, e, args.seconds, args.fps, depth, gpu_env)
            if not r:
                print(json.dumps({'name': e['name'], 'in_flight': depth, 'error': 'no perf log'}), flush=True)
                continue
            stages = r['stages']
            frames = r['frames']
            # the viewer starts counting at the first presented frame; the run is `seconds` long after loading
            fps_out = frames / args.seconds if frames else 0
            util = sorted(r['gpu_util'])[len(r['gpu_util']) // 2:] if r['gpu_util'] else []
            res = dict(name=e['name'], in_flight=depth, fps_target=args.fps, frames=frames, fps=round(fps_out, 1),
                       frame_ms=stages['frame'], wait_ms=stages['wait'], draw_ms=stages['draw'],
                       cpu_pct=round(100 * r['cpu'] / r['wall'], 1), rss_mb=r['rss_mb'],
                       gpu_mem_mb=(max(r['gpu_mem']) - idle_mem) if r['gpu_mem'] and idle_mem is not None else None,
                       gpu_util_pct=round(sum(util) / len(util)) if util else None)
            results.append(res)
            print(json.dumps(res), flush=True)
    out = args.out or os.path.join(run.HERE, 'results', 'windowed-%s.json' % time.strftime('%Y%m%d-%H%M'))
    with open(out, 'w') as f:
        json.dump(dict(viewer=args.viewer, seconds=args.seconds, fps=args.fps, results=results), f, indent=1)


if __name__ == '__main__':
    main()
