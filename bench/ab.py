#!/usr/bin/env python3
"""A/B benchmark: several configurations (viewer binary + environment) measured in interleaved rounds, medians reported.

    bench/ab.py --config base=VIEWER --config d1=VIEWER:OWE_FRAMES_IN_FLIGHT=1 --config d2=VIEWER:OWE_FRAMES_IN_FLIGHT=2
                [--rounds 3] [--only NAME...] [--out FILE]

Sequential sweeps drift when other jobs share the machine (a slower minute hits one configuration only). Here every
wallpaper is measured by each configuration in turn, `rounds` times, and each metric is the median over the rounds.
"""
import argparse
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--config', action='append', required=True, help='NAME=VIEWER[:VAR=VALUE...]')
    ap.add_argument('--rounds', type=int, default=3)
    ap.add_argument('--only', nargs='*')
    ap.add_argument('--size')
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    configs = []
    for c in args.config:
        name, rest = c.split('=', 1)
        parts = rest.split(':')
        configs.append((name, parts[0], dict(p.split('=', 1) for p in parts[1:])))
    with open(os.path.join(run.HERE, 'wallpapers.json')) as f:
        entries = [e for e in json.load(f)['wallpapers'] if not args.only or e['name'] in args.only]
    if args.size:
        for e in entries:
            e['size'] = args.size
    run.GPU_ENV['VK_DRIVER_FILES'] = run.ICDS['nvidia']
    idle = run.gpu_sample()
    idle_mem = idle[0] if idle else None
    out = {'configs': [c[0] for c in configs], 'rounds': args.rounds, 'results': []}
    for e in entries:
        samples = {c[0]: [] for c in configs}
        for _ in range(args.rounds):
            for name, viewer, env in configs:
                saved = {k: os.environ.get(k) for k in env}
                os.environ.update(env)
                try:
                    r = run.bench(viewer, e, idle_mem, 0)
                finally:
                    for k, v in saved.items():
                        if v is None:
                            os.environ.pop(k, None)
                        else:
                            os.environ[k] = v
                if 'error' not in r:
                    samples[name].append(r)
        row = {'name': e['name'], 'category': e.get('category', ''), 'size': e.get('size', '1920x1080')}
        for name, _, _ in configs:
            rs = samples[name]
            if not rs:
                row[name] = {'error': 'failed'}
                continue
            med = lambda k: round(statistics.median(r[k] for r in rs if k in r), 3) if any(k in r for r in rs) else None
            row[name] = {k: med(k) for k in ('ms_per_frame', 'cpu_ms_per_frame', 'rss_mb', 'gpu_mem_mb', 'gpu_util_pct')}
            row[name]['all_ms'] = [r['ms_per_frame'] for r in rs]
        out['results'].append(row)
        print(json.dumps(row), flush=True)
        with open(args.out, 'w') as f:
            json.dump(out, f, indent=1)


if __name__ == '__main__':
    main()
