#!/usr/bin/env python3
"""Performance benchmark for open-wallpaper-engine's SceneViewer.

    bench/run.py [--viewer PATH] [--label NAME] [--only NAME...] [--category CAT...]
                 [--out FILE] [--baseline FILE [--tolerance 0.10]]

Each wallpaper of wallpapers.json is rendered offscreen (`--capture`, no window and no frame pacing: the viewer draws
as fast as it can) twice, to a short and to a long frame, in a memory cgroup of its own. Steady state cost is the
difference of the two runs divided by the extra frames, so scene loading, shader compilation and the final readback
cancel out:

    ms_per_frame = (wall_long - wall_short) / (frames_long - frames_short)
    cpu_ms_per_frame likewise from user+system time (a value above ms_per_frame means several busy threads)

The viewer picks the first Vulkan device the loader lists, which on this machine is the Intel iGPU, so --gpu
(default nvidia) restricts it to one driver via VK_DRIVER_FILES.

Also recorded: peak RSS, load time (wall_short minus short frames of work), and, from a poll of nvidia-smi while the
viewer runs, peak GPU memory above the idle level and mean GPU utilisation (the GPU must be otherwise idle).
`fps_max` = 1000 / ms_per_frame is the uncapped frame rate; the target is >= 60 (16.7 ms), then >= 120 (8.3 ms).

With --baseline FILE a wallpaper whose ms_per_frame, cpu_ms_per_frame or RSS grew by more than the tolerance (and by
at least 0.3 ms or 20 MB) fails: exit status 1. Run on an otherwise idle machine, one viewer at a time.
"""
import argparse
import json
import os
import resource
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
STEAM = os.path.expanduser('~/snap/steam/common/.local/share/Steam/steamapps')
WORKSHOP = os.path.join(STEAM, 'workshop/content/431960')
ASSETS = os.environ.get('WPE_ASSETS', os.path.join(STEAM, 'common/wallpaper_engine/assets'))
DEFAULT_VIEWER = os.path.join(os.path.dirname(HERE), 'build/release/bin/owe-sceneviewer/SceneViewer')
ICDS = {'nvidia': '/usr/share/vulkan/icd.d/nvidia_icd.json', 'intel': '/usr/share/vulkan/icd.d/intel_icd.json'}
GPU_ENV = {}
MEMORY = os.environ.get('WPE_BENCH_MEMORY', '4G')


LOCK_DIR = os.path.expanduser('~/.cache/wpe-locks')
# every measurement-style job on this machine takes one of these (the threads named theirs differently); a benchmark holds
# all of them, always in this order, so none of the others runs a GPU or CPU heavy job meanwhile
LOCK_NAMES = ('bench.lock', 'bench-gpu.lock', 'perf-bench.lock')
_held = []


def hold_locks(names=LOCK_NAMES, wait=True):
    """Take every shared benchmark lock, blocking, always in the order of LOCK_NAMES (the order the other jobs chain their
    `flock` calls in, so waiters queue behind each other and nobody deadlocks); held for the life of the process."""
    import fcntl
    os.makedirs(LOCK_DIR, exist_ok=True)
    for name in names:
        f = open(os.path.join(LOCK_DIR, name), 'a')
        fcntl.flock(f, fcntl.LOCK_EX if wait else fcntl.LOCK_EX | fcntl.LOCK_NB)
        _held.append(f)


def package(entry):
    folder = entry.get('path') or os.path.join(WORKSHOP, str(entry['id']))
    folder = os.path.expanduser(folder)
    with open(os.path.join(folder, 'project.json')) as f:
        scene_file = json.load(f).get('file', 'scene.json')
    return os.path.join(folder, os.path.splitext(scene_file)[0] + '.pkg')


def gpu_sample():
    """(memory used MiB, utilisation %) of the first NVIDIA GPU, or None."""
    try:
        out = subprocess.run(['nvidia-smi', '--query-gpu=memory.used,utilization.gpu', '--format=csv,noheader,nounits'],
                             capture_output=True, text=True, timeout=5).stdout.split('\n')[0].split(',')
        return int(out[0]), int(out[1])
    except Exception:
        return None


class Sampler(threading.Thread):
    def __init__(self, pid):
        super().__init__(daemon=True)
        self.pid, self.stop, self.gpu_mem, self.gpu_util, self.rss = pid, threading.Event(), [], [], 0

    def run(self):
        while not self.stop.is_set():
            try:
                with open('/proc/%d/status' % self.pid) as f:
                    for line in f:
                        if line.startswith('VmHWM:'):
                            self.rss = max(self.rss, int(line.split()[1]) // 1024)
            except OSError:
                pass
            s = gpu_sample()
            if s:
                self.gpu_mem.append(s[0])
                self.gpu_util.append(s[1])
            self.stop.wait(0.25)


def run_once(viewer, entry, frame, extra):
    size = entry.get('size', '1920x1080')
    out = '/tmp/owe-bench-%d.ppm' % os.getpid()
    cache = os.path.join(os.path.expanduser('~/.cache'), 'owe-bench-shaders-' + os.path.basename(os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(viewer)))))))
    os.makedirs(cache, exist_ok=True)
    cmd = ['systemd-run', '--user', '--scope', '-q', '-p', 'MemoryMax=' + MEMORY, '-p', 'MemorySwapMax=0',
           viewer, '-C', cache, '--resolution', size, '--fps', str(entry.get('fps', 60)),
           '--capture', out, '--capture-frame', str(frame)] + entry.get('args', []) + extra + [ASSETS, package(entry)]
    env = {k: v for k, v in os.environ.items() if k not in ('WAYLAND_DISPLAY', 'DISPLAY')}
    env.update(GPU_ENV)
    start = time.monotonic()
    with open('/dev/null', 'w') as null:
        proc = subprocess.Popen(cmd, env=env, stdout=null, stderr=null)
        sampler = Sampler(proc.pid)
        sampler.start()
        deadline = start + entry.get('timeout', 900)
        while True:
            done, status, usage = os.wait4(proc.pid, os.WNOHANG)
            if done:
                break
            if time.monotonic() > deadline:
                proc.kill()
            time.sleep(0.01)
        proc.returncode = os.waitstatus_to_exitcode(status)
    wall = time.monotonic() - start
    sampler.stop.set()
    sampler.join()
    ok = proc.returncode == 0 and os.path.exists(out)
    if os.path.exists(out):
        os.remove(out)
    return dict(ok=ok, wall=wall, cpu=usage.ru_utime + usage.ru_stime,
                rss_mb=usage.ru_maxrss // 1024, gpu_mem=sampler.gpu_mem, gpu_util=sampler.gpu_util)


def bench(viewer, entry, idle_mem, paced):
    short, long = entry.get('frames_short', 100), entry.get('frames_long', 1100)
    a = run_once(viewer, entry, short, [])
    b = run_once(viewer, entry, long, [])
    result = {'name': entry['name'], 'category': entry.get('category', ''), 'size': entry.get('size', '1920x1080')}
    if not (a['ok'] and b['ok']):
        result['error'] = 'viewer failed (short ok=%s, long ok=%s)' % (a['ok'], b['ok'])
        return result
    frames = long - short
    ms = (b['wall'] - a['wall']) * 1000 / frames
    result.update(ms_per_frame=round(ms, 3), fps_max=round(1000 / ms, 1) if ms > 0 else None,
                  cpu_ms_per_frame=round((b['cpu'] - a['cpu']) * 1000 / frames, 3),
                  load_s=round(a['wall'] - short * ms / 1000, 2), rss_mb=b['rss_mb'])
    if b['gpu_mem'] and idle_mem is not None:
        result['gpu_mem_mb'] = max(b['gpu_mem']) - idle_mem
        # skip the load phase: utilisation of the busiest half of the samples
        util = sorted(b['gpu_util'])[len(b['gpu_util']) // 2:]
        result['gpu_util_pct'] = round(sum(util) / len(util))
    if paced:
        # what the wallpaper costs while it runs at its frame rate, as on a desktop: realtime pacing, the CPU time and
        # the GPU utilisation of (paced - 60 frames) of work
        fps = entry.get('fps', 60)
        p1 = run_once(viewer, entry, fps, ['--capture-realtime'])
        p2 = run_once(viewer, entry, fps * (1 + paced), ['--capture-realtime'])
        if p1['ok'] and p2['ok']:
            result['paced_cpu_pct'] = round(100 * (p2['cpu'] - p1['cpu']) / (p2['wall'] - p1['wall']), 1)
            result['paced_fps'] = round(fps * paced / (p2['wall'] - p1['wall']), 1)
            util = p2['gpu_util'][len(p2['gpu_util']) // 3:]
            result['paced_gpu_pct'] = round(sum(util) / len(util)) if util else None
    return result


def compare(results, baseline, tolerance):
    old = {r['name']: r for r in baseline['results']}
    failures = []
    for r in results:
        o = old.get(r['name'])
        if not o or 'error' in o or 'error' in r:
            continue
        for key, floor in (('ms_per_frame', 0.3), ('cpu_ms_per_frame', 0.3), ('rss_mb', 20)):
            if r[key] > o[key] * (1 + tolerance) and r[key] - o[key] >= floor:
                failures.append('%s: %s %.2f -> %.2f' % (r['name'], key, o[key], r[key]))
    return failures


def main():
    hold_locks()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--viewer', default=DEFAULT_VIEWER)
    ap.add_argument('--label', default=time.strftime('%Y-%m-%d'))
    ap.add_argument('--only', nargs='*')
    ap.add_argument('--category', nargs='*')
    ap.add_argument('--list', default=os.path.join(HERE, 'wallpapers.json'))
    ap.add_argument('--gpu', choices=['nvidia', 'intel', 'any'], default='nvidia',
                    help='the Vulkan driver the viewer may use (default nvidia): the loader lists the Intel iGPU first '
                         'and the viewer takes the first device, so without this it renders on the iGPU')
    ap.add_argument('--paced', type=int, default=0, metavar='SECONDS',
                    help='also run SECONDS of realtime-paced frames (as on a desktop) for CPU and GPU use at the fps')
    ap.add_argument('--size', help='override every wallpaper\'s resolution, e.g. 5120x1440')
    ap.add_argument('--out')
    ap.add_argument('--baseline')
    ap.add_argument('--tolerance', type=float, default=0.10)
    args = ap.parse_args()
    with open(args.list) as f:
        entries = json.load(f)['wallpapers']
    entries = [e for e in entries if (not args.only or e['name'] in args.only) and
               (not args.category or e.get('category') in args.category)]
    if args.gpu != 'any':
        GPU_ENV['VK_DRIVER_FILES'] = ICDS[args.gpu]
    if args.size:
        for e in entries:
            e['size'] = args.size
    idle = gpu_sample()
    idle_mem = idle[0] if idle else None
    results = []
    for e in entries:
        r = bench(args.viewer, e, idle_mem, args.paced)
        results.append(r)
        print(json.dumps(r), flush=True)
    out = args.out or os.path.join(HERE, 'results', args.label + '.json')
    with open(out, 'w') as f:
        json.dump({'label': args.label, 'viewer': args.viewer, 'gpu': args.gpu, 'gpu_idle_mb': idle_mem, 'results': results}, f, indent=1)
    print('\n%-34s %-10s %9s %8s %8s %7s %7s %6s' % ('wallpaper', 'category', 'ms/frame', 'fps max', 'cpu ms', 'RSS MB',
                                                    'VRAM MB', 'GPU %'))
    for r in sorted(results, key=lambda r: -r.get('ms_per_frame', 1e9)):
        print('%-34s %-10s %9s %8s %8s %7s %7s %6s' % (r['name'][:34], r['category'], r.get('ms_per_frame', 'FAIL'),
              r.get('fps_max', ''), r.get('cpu_ms_per_frame', ''), r.get('rss_mb', ''), r.get('gpu_mem_mb', ''),
              r.get('gpu_util_pct', '')))
    if args.baseline:
        with open(args.baseline) as f:
            failures = compare(results, json.load(f), args.tolerance)
        for line in failures:
            print('REGRESSION', line)
        sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
