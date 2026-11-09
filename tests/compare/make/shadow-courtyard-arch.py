# shadow-courtyard-arch: oot-repro's courtyard edge, rebuilt at scale 1 from the 41 triangles of the wall that cast it
# and the stone floor it falls on (make/data/courtyard-arch.json, from castle_courtyard.mdl's naka_kabe_02 and
# naka_isi_01, times its 0.01). Untextured, under OoT's camera (looking at the origin), sun and cascades, medium
# shadows. WE lets a wider strip of sun in at the wall's foot. Variants: -lowered, the floor 0.4 lower (the wall's
# foot no longer inside it); -novault, the wall without its vault (only its upright faces); -nowall, the floor alone;
# -closed, the wall two-sided (each triangle twice).
import struct
CENTER = (8.98972, 1.30595, -0.10218)
DATA = json.load(open(os.path.join(os.path.dirname(HERE), 'make', 'data', 'courtyard-arch.json')))


def mesh_mdl(parts):
    """One model of meshes (material, positions, normals, triangles)."""
    lo = [min(p[i] for _, ps, _, _ in parts for p in ps) for i in range(3)]
    hi = [max(p[i] for _, ps, _, _ in parts for p in ps) for i in range(3)]
    data = b'MDLV0017\0' + struct.pack('<iiI', 15, 1, len(parts))
    for material, positions, normals, triangles in parts:
        vertices = b''.join(struct.pack('<12f', *p, *n, 1, 0, 0, 1, 0, 0) for p, n in zip(positions, normals))
        indices = b''.join(struct.pack('<3H', *t) for t in triangles)
        data += material.encode() + b'\0' + struct.pack('<I', 0) + struct.pack('<6f', *lo, *hi)
        data += struct.pack('<I', 15) + struct.pack('<I', len(vertices)) + vertices
        data += struct.pack('<I', len(indices)) + indices
    return data


def make(name, floor_dy=0.0, vault=True, wall=True, closed=False):
    w = Scene3D(name, 'the courtyard wall that casts the edge (%s)' % name, ambient='0.15 0.15 0.15',
                eye='0.98868 0.13889 0.05676')
    mat = 'materials/plain.json'
    w.files[mat] = {"passes": [{"shader": "generic4", "combos": {"LIGHTING": 1},
                                "constantshadervalues": {"color": "0.8 0.8 0.8", "alpha": 1.0}, "textures": [None]}]}
    def part(key, dy=0.0, keep=lambda n: True):
        d = DATA[key]
        ps = [[p[0] - CENTER[0], p[1] - CENTER[1] + dy, p[2] - CENTER[2]] for p in d['positions']]
        tris = [t for t in d['triangles'] if all(keep(d['normals'][i]) for i in t)]
        normals = d['normals']
        if closed and key == 'wall':  # each triangle twice: its back face too, wound and facing the other way
            n = len(ps)
            ps, normals = ps + ps, normals + [[-c for c in v] for v in normals]
            tris = tris + [[t[0] + n, t[2] + n, t[1] + n] for t in tris]
        return (mat, ps, normals, tris)
    upright = (lambda n: True) if vault else (lambda n: abs(n[1]) < 0.01)
    mdl = 'models/arch.mdl'
    write_binary(os.path.join(w.name, mdl), mesh_mdl(([part('wall', keep=upright)] if wall else []) + [part('floor', floor_dy)]))
    w.cube('arch', mdl, 0, 0, 0)
    w.light('sun', 'ldirectional', -CENTER[0], 1.51901 - CENTER[1], -CENTER[2], '1 1 1', 3.09, 5,
            (0.0, 0.32301, -2.06849), castshadow=True, cascadedistance0=3.0, cascadedistance1=14.34,
            cascadedistance2=100.0)
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
    clock_probe.stamp(os.path.join(HERE, name))


make('shadow-courtyard-arch')
make('shadow-courtyard-arch-lowered', floor_dy=-0.4)
make('shadow-courtyard-arch-novault', vault=False)
make('shadow-courtyard-arch-nowall', wall=False)
make('shadow-courtyard-arch-closed', closed=True)
