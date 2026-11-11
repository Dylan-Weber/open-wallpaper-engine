#!/usr/bin/env python3
"""Runs every scripted Workshop wallpaper on this machine with two SceneViewers (QuickJS and SpiderMonkey builds) and compares
crashes, script errors and pictures.

    parity-sweep.py --quickjs VIEWER --spidermonkey VIEWER [--frame 200] [--out report.json] [--only ID ...]

Per wallpaper: QuickJS twice (to measure how much two runs of the same engine differ: timing makes some scenes move) and
SpiderMonkey once, each rendered offscreen to the same frame. Compared: exit status, the number of script errors and the
scripts that raised them, and the mean of the largest per-channel difference between pictures (SpiderMonkey vs QuickJS,
against QuickJS vs QuickJS). Each wallpaper takes the bench locks (bench.lock, then bench-gpu.lock) on its own, with a gap.
"""
import argparse, glob, json, os, re, subprocess, sys, time

STEAM = os.path.expanduser('~/snap/steam/common/.local/share/Steam/steamapps')
W = os.path.join(STEAM, 'workshop/content/431960')
ASSETS = os.path.join(STEAM, 'common/wallpaper_engine/assets')
LOCKS = os.path.expanduser('~/.cache/wpe-locks')


def read_pkg(path):
    import struct
    d = open(path, 'rb').read()
    n = struct.unpack_from('<I', d, 0)[0]; o = 4 + n
    cnt = struct.unpack_from('<I', d, o)[0]; o += 4
    ents = []
    for _ in range(cnt):
        ln = struct.unpack_from('<I', d, o)[0]; o += 4
        name = d[o:o + ln].decode(); o += ln
        off, size = struct.unpack_from('<II', d, o); o += 8
        ents.append((name, off, size))
    return {n: d[o + off:o + off + s] for n, off, s in ents}


def scripted():
    out = []
    for d in sorted(glob.glob(W + '/*')):
        try:
            proj = json.load(open(os.path.join(d, 'project.json')))
            if proj.get('type', '').lower() != 'scene': continue
            pkg = os.path.join(d, os.path.splitext(proj.get('file', 'scene.json'))[0] + '.pkg')
            scene = read_pkg(pkg)['scene.json']
        except Exception:
            continue
        if b'"script"' in scene: out.append((os.path.basename(d), proj.get('title', '')[:28], pkg))
    return out


def ppm(path):
    d = open(path, 'rb').read(); parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split()); return parts[3][:w * h * 3]


def diff(a, b):
    if len(a) != len(b): return None
    tot = 0
    for i in range(0, len(a), 3):
        tot += max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
    return tot / (len(a) // 3)


def run(viewer, pkg, frame, tag, extra_env=None):
    out = '/tmp/owe-sweep-%s.ppm' % tag
    cache = os.path.expanduser('~/.cache/owe-sweep-shaders-' + os.path.basename(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(viewer)))))))
    os.makedirs(cache, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if k not in ('WAYLAND_DISPLAY', 'DISPLAY')}
    env['VK_DRIVER_FILES'] = '/usr/share/vulkan/icd.d/nvidia_icd.json'
    # no systemd-run scope: it swallows the viewer's log lines, and the error counts are half of what this compares
    cmd = [viewer, '-C', cache, '--resolution', '1920x1080', '--fps', '60', '--capture', out, '--capture-frame', str(frame), ASSETS, pkg]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=900)
    errors = [re.sub(r'^\[[^\]]*\]\s*', '', l) for l in (p.stdout + p.stderr).splitlines() if 'ERROR' in l]
    shas = sorted({m.group(1)[:8] for l in errors for m in [re.search(r'script\[([0-9a-f]+)\]', l)] if m})
    return {'rc': p.returncode, 'errors': len(errors), 'error_scripts': shas, 'picture': out if os.path.exists(out) else None,
            'sample': errors[:2]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--quickjs', required=True); ap.add_argument('--spidermonkey', required=True)
    ap.add_argument('--errors-only', action='store_true', help='one run per engine, no picture comparison')
    ap.add_argument('--frame', type=int, default=200); ap.add_argument('--out'); ap.add_argument('--only', nargs='*')
    args = ap.parse_args()
    report = []
    for wid, title, pkg in scripted():
        if args.only and wid not in args.only: continue
        # one lock hold per wallpaper: bench.lock, then bench-gpu.lock; the three viewers run under it
        lockfd = []
        import fcntl
        for name in ('bench.lock', 'bench-gpu.lock'):
            f = open(os.path.join(LOCKS, name), 'a'); fcntl.flock(f, fcntl.LOCK_EX); lockfd.append(f)
        try:
            a = run(args.quickjs, pkg, args.frame, 'q1')
            b = a if args.errors_only else run(args.quickjs, pkg, args.frame, 'q2')
            c = run(args.spidermonkey, pkg, args.frame, 'sm')
            noise = None if args.errors_only else diff(ppm(a['picture']), ppm(b['picture'])) if a['picture'] and b['picture'] else None
            d = None if args.errors_only else diff(ppm(a['picture']), ppm(c['picture'])) if a['picture'] and c['picture'] else None
        finally:
            for f in lockfd: fcntl.flock(f, fcntl.LOCK_UN); f.close()
        row = {'id': wid, 'title': title, 'quickjs': {k: a[k] for k in ('rc', 'errors', 'error_scripts')},
               'quickjs2': {k: b[k] for k in ('rc', 'errors', 'error_scripts')}, 'spidermonkey': {k: c[k] for k in ('rc', 'errors', 'error_scripts')},
               'sm_samples': c['sample'], 'qjs_samples': a['sample'], 'noise': noise, 'diff': d}
        report.append(row)
        print('%-11s %-28s rc %d/%d  errors %2d/%2d  scripts-with-errors %s/%s  picture diff sm-vs-qjs %s  qjs-vs-qjs %s' % (
            wid, title, a['rc'], c['rc'], a['errors'], c['errors'], len(a['error_scripts']), len(c['error_scripts']),
            '%.3f' % d if d is not None else '-', '%.3f' % noise if noise is not None else '-'), flush=True)
        time.sleep(10)  # a gap, so waiting jobs can take the locks
    if args.out: json.dump(report, open(args.out, 'w'), indent=1)


main()
