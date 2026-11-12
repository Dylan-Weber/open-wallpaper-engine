#!/usr/bin/env python3
"""CPU stage breakdown of one wallpaper (needs a viewer built with the OWE_PERF_LOG stage timers).

    bench/stages.py NAME_OR_ID [--frames 300] [--size 1920x1080] [--viewer PATH] [--gpu nvidia]

Renders offscreen as fast as the machine allows and prints, per stage, the mean / p50 / p95 / p99 / max milliseconds
per frame over the frames after the first 10. Stages overlap (draw contains update, begin, advance, record, submit,
wait): `wait` is time the render thread sat waiting for the GPU to finish the frame.
"""
import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('name')
    ap.add_argument('--frames', type=int, default=300)
    ap.add_argument('--size')
    ap.add_argument('--json', help='also write the raw stage and GPU pass data to FILE')
    ap.add_argument('--top', type=int, default=25, help='GPU passes to list')
    ap.add_argument('--viewer', default=run.DEFAULT_VIEWER)
    ap.add_argument('--gpu', default='nvidia', choices=['nvidia', 'intel', 'any'])
    args = ap.parse_args()
    with open(os.path.join(run.HERE, 'wallpapers.json')) as f:
        entries = {e['name']: e for e in json.load(f)['wallpapers']}
    entry = entries.get(args.name) or {'name': args.name, 'id': args.name}
    if args.size:
        entry = dict(entry, size=args.size)
    if args.gpu != 'any':
        run.GPU_ENV['VK_DRIVER_FILES'] = run.ICDS[args.gpu]
    log = '/tmp/owe-stages-%d.json' % os.getpid()
    os.environ['OWE_PERF_LOG'] = log
    r = run.run_once(args.viewer, entry, args.frames, [])
    if not r['ok'] or not os.path.exists(log):
        sys.exit('viewer failed, or has no OWE_PERF_LOG support')
    with open(log) as f:
        data = json.load(f)
    os.remove(log)
    if args.json:
        with open(args.json, 'w') as f:
            json.dump(dict(name=entry['name'], size=entry.get('size', '1920x1080'), wall=r['wall'], cpu=r['cpu'], **data), f)
    print('%s: %d frames, wall %.1f s, cpu %.1f s, rss %d MB' % (entry['name'], data['frames'], r['wall'], r['cpu'],
                                                              r['rss_mb']))
    print('%-9s %8s %8s %8s %8s %9s' % ('stage', 'mean', 'p50', 'p95', 'p99', 'max'))
    for stage, v in data['stages'].items():
        print('%-9s %8.3f %8.3f %8.3f %8.3f %9.3f' % (stage, v['mean'], v['p50'], v['p95'], v['p99'], v['max']))
    passes = data.get('gpu_passes', [])
    if passes:
        total = sum(p['ms_per_frame'] for p in passes)
        print('\nGPU time by pass name (OWE_GPU_TIMING): %.3f ms/frame over %d passes' % (total, len(passes)))
        print('%-52s %9s %9s %9s %7s' % ('pass', 'ms/frame', 'ms/run', 'p95/run', 'frames'))
        for p in passes[:args.top]:
            print('%-52s %9.3f %9.3f %9.3f %7d' % (p['name'][:52], p['ms_per_frame'], p['ms_when_run'], p['p95_when_run'],
                                                  p['frames_run']))


if __name__ == '__main__':
    main()
