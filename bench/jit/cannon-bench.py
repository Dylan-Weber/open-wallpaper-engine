#!/usr/bin/env python3
"""JavaScript engine comparison on the physics More Physics (Workshop 3734636606) runs: its inlined cannon-es stepping a
tower of 14 x 32 boxes, with the wallpaper's own world settings (SAP broadphase, split solver, 8 iterations, sleeping).

    jit/cannon-bench.py [--qjs PATH]      # builds a qjs CLI from the engine's QuickJS-ng revision when not given

Extracts the script from the wallpaper's scene.pkg, keeps the cannon-es part (up to "end cannon-es scope"), appends a
world, and times world.step(1/60) in node (V8 with its JIT), node --jitless (V8's interpreter) and QuickJS-ng -O3.
"""
import argparse
import json
import os
import struct
import subprocess
import sys
import tempfile

W = os.path.expanduser('~/snap/steam/common/.local/share/Steam/steamapps/workshop/content/431960')
BENCH = r'''
function run(layers, perLayer, steps, label) {
    const world = new _ce.World({ gravity: new _ce.Vec3(0, -9.81, 0), broadphase: new _ce.SAPBroadphase(), allowSleep: true });
    const solver = new _ce.GSSolver(); solver.iterations = 8; world.solver = new _ce.SplitSolver(solver);
    const mat = new _ce.Material({ friction: 0.4, restitution: 0.1 });
    const floor = new _ce.Body({ mass: 0, material: mat }); floor.addShape(new _ce.Plane());
    floor.quaternion.setFromEuler(-Math.PI / 2, 0, 0); world.addBody(floor);
    const R = 20;
    for (let l = 0; l < layers; l++) for (let i = 0; i < perLayer; i++) {
        const b = new _ce.Body({ mass: 1, material: mat });
        b.addShape(new _ce.Box(new _ce.Vec3(2.5, 2.5, 2.5)));
        const a = (i + (l & 1) * 0.5) / perLayer * Math.PI * 2;
        b.position.set(Math.cos(a) * R, 2.5 + l * 5.01, Math.sin(a) * R);
        b.quaternion.setFromEuler(0, -a, 0);
        world.addBody(b);
    }
    const per = [];
    for (let s = 0; s < steps; s++) { const t = Date.now(); world.step(1 / 60); per.push(Date.now() - t); }
    const mean = per.reduce((a, b) => a + b, 0) / steps;
    console.log(label, 'bodies', world.bodies.length, 'steps', steps, 'mean ms/step', mean.toFixed(2),
        'first30', (per.slice(0, 30).reduce((a, b) => a + b, 0) / 30).toFixed(2),
        'last30', (per.slice(-30).reduce((a, b) => a + b, 0) / 30).toFixed(2), 'contacts', world.contacts.length);
}
run(2, 8, 300, 'small');
run(14, 32, 300, 'tower');
'''


def read_pkg(path):
    d = open(path, 'rb').read()
    n = struct.unpack_from('<I', d, 0)[0]
    o = 4 + n
    cnt = struct.unpack_from('<I', d, o)[0]
    o += 4
    ents = []
    for _ in range(cnt):
        ln = struct.unpack_from('<I', d, o)[0]
        o += 4
        name = d[o:o + ln].decode()
        o += ln
        off, size = struct.unpack_from('<II', d, o)
        o += 8
        ents.append((name, off, size))
    return {n: d[o + off:o + off + s] for n, off, s in ents}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--qjs')
    args = ap.parse_args()
    scene = json.loads(read_pkg(W + '/3734636606/scene.pkg')['scene.json'])
    script = scene['objects'][1]['visible']['script']
    lib = script.split('} // end cannon-es scope')[0] + '}\n'
    lib = lib.replace("'use strict';", '', 1)
    with tempfile.TemporaryDirectory() as tmp:
        js = os.path.join(tmp, 'cannon-bench.js')
        with open(js, 'w') as f:
            f.write(lib + BENCH)
        for label, cmd in (('node (V8 JIT)', ['node', js]), ('node --jitless', ['node', '--jitless', js]),
                           ('quickjs-ng', [args.qjs or 'qjs', js])):
            print('==', label)
            print(subprocess.run(cmd, capture_output=True, text=True).stdout.strip())


if __name__ == '__main__':
    main()
