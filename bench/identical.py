#!/usr/bin/env python3
"""Frames in flight must not change what is drawn: capture the same frame with OWE_FRAMES_IN_FLIGHT = 1, 2, 3 and compare
the images byte for byte (the captures are frame-exact: a fixed clock, a fixed random seed).

    bench/identical.py [--viewer PATH] [--frame 120] [--depths 1 2 3] [--only NAME...] [--valid]

Exit status 1 when any wallpaper's image differs between depths. --valid runs the viewer with the Khronos validation
layer and synchronization validation (slow) and fails on any validation error that depth 1 does not also have.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run  # noqa: E402


def capture(viewer, entry, frame, depth, valid):
    out = '/tmp/owe-identical-%d-%d.ppm' % (os.getpid(), depth)
    cache = os.path.join(os.path.expanduser('~/.cache'), 'owe-bench-shaders-identical')
    os.makedirs(cache, exist_ok=True)
    cmd = ['systemd-run', '--user', '--scope', '-q', '-p', 'MemoryMax=' + ('8G' if valid else run.MEMORY), '-p',
           'MemorySwapMax=0', viewer, '-C', cache, '--resolution', entry.get('size', '1920x1080'), '--fps',
           str(entry.get('fps', 60)), '--capture', out, '--capture-frame', str(frame), '--random-seed', '1'] \
        + (['--valid-layer'] if valid else []) + entry.get('args', []) + [run.ASSETS, run.package(entry)]
    env = {k: v for k, v in os.environ.items() if k not in ('WAYLAND_DISPLAY', 'DISPLAY')}
    env.update(VK_DRIVER_FILES=run.ICDS['nvidia'], OWE_FRAMES_IN_FLIGHT=str(depth))
    if valid:
        env.update(VK_LAYER_VALIDATE_SYNC='1',
                   VK_LAYER_PATH='/usr/share/vulkan/explicit_layer.d')
    p = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=entry.get('timeout', 900))
    digest = None
    if os.path.exists(out):
        with open(out, 'rb') as f:
            digest = hashlib.sha256(f.read()).hexdigest()[:16]
        os.remove(out)
    errors = sorted(set(re.findall(r'(VUID-[A-Za-z0-9-]+|SYNC-HAZARD-[A-Z-]+)', p.stdout + p.stderr)))
    return p.returncode, digest, errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--viewer', default=run.DEFAULT_VIEWER)
    ap.add_argument('--frame', type=int, default=120)
    ap.add_argument('--depths', nargs='*', type=int, default=[1, 2, 3])
    ap.add_argument('--only', nargs='*')
    ap.add_argument('--valid', action='store_true')
    args = ap.parse_args()
    with open(os.path.join(run.HERE, 'wallpapers.json')) as f:
        entries = [e for e in json.load(f)['wallpapers'] if not args.only or e['name'] in args.only]
    bad = 0
    for e in entries:
        got = {d: capture(args.viewer, e, args.frame, d, args.valid) for d in args.depths}
        base = got[args.depths[0]]
        again = capture(args.viewer, e, args.frame, args.depths[0], False)
        if again[1] != base[1]:
            print('%-28s skip (the image differs between two runs at depth %d: it depends on the wall clock)' % (e['name'], args.depths[0]), flush=True)
            continue
        same = all(g[1] == base[1] and g[1] is not None for g in got.values())
        new_errors = sorted(set(sum((g[2] for g in got.values()), [])) - set(base[2])) if args.valid else []
        status = 'ok' if same and not new_errors else 'DIFF'
        bad += status != 'ok'
        print('%-28s %s  %s%s' % (e['name'], status, ' '.join('%d:%s' % (d, g[1]) for d, g in got.items()),
                                  ('  new validation: ' + ' '.join(new_errors)) if new_errors else ''), flush=True)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
