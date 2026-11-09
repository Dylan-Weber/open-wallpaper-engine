# script-cadence: how often Wallpaper Engine runs layers' update() scripts, against how many frames it rendered. Solid
# layers show counts as colours (sprite-refresh's palette: n -> (n % 256, n // 256 x 60 + 20, 7n % 256), n below 1000), read
# by script_cadence_read.py:
#   A  a visible layer's own colour script, counting its updates
#   B  an invisible layer's origin script, counting into shared.b (shown by a display layer)
#   C  a layer far outside the view, counting into shared.c
#   D  a puppet layer's origin script, counting into shared.d and kicking its middle bone every 30 updates
#   F  engine.frametime in tenths of a millisecond
#   S  a 1000-frame sprite sheet of 0.01 s frames with general.spritesheetrefreshsync on: one frame a rendered update
# With the probe's g_Time frame, one picture says whether scripts run once a rendered frame (A = S, any drop global), on a
# clock of their own, or layer by layer. -60 is the same at 60 fps.
import struct

SHARED = "const S = (typeof shared !== 'undefined') ? shared : globalThis;\n"
COLOUR = "new Vec3((n % 256) / 255, (Math.floor(n / 256) * 60 + 20) / 255, ((n * 7) % 256) / 255)"
OWN = """'use strict';
let n = -1;
export function update(value) {
    n = (n + 1) % 1000;
    return COLOUR;
}
""".replace('COLOUR', COLOUR)
COUNT = """'use strict';
SHARED
export function update(value) {
    S.KEY = ((S.KEY === undefined ? -1 : S.KEY) + 1) % 1000;
    EXTRA
    return value;
}
""".replace('SHARED', SHARED)
SHOW = """'use strict';
SHARED
export function update(value) {
    const n = VALUE;
    return COLOUR;
}
""".replace('SHARED', SHARED).replace('COLOUR', COLOUR)
KICK = "if (S.d % 30 == 29) thisLayer.applyBonePhysicsImpulse('mid', new Vec3(0, 0, 0), new Vec3(0, 0, 3));"
FRAMES, COLUMNS, FRAME = 1000, 40, 8


def sprite_tex(frametime):
    side_w, side_h = COLUMNS * FRAME, ((FRAMES + COLUMNS - 1) // COLUMNS) * FRAME
    pixels = bytearray(side_w * side_h * 4)
    for i in range(FRAMES):
        x0, y0 = (i % COLUMNS) * FRAME, (i // COLUMNS) * FRAME
        r, g, b = i % 256, (i // 256) * 60 + 20, (i * 7) % 256
        for y in range(y0, y0 + FRAME):
            row = (y * side_w + x0) * 4
            pixels[row:row + FRAME * 4] = bytes((r, g, b, 255)) * FRAME
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 7, side_w, side_h, side_w, side_h, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', side_w, side_h, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    data += b'TEXS0003\0' + struct.pack('<III', FRAMES, FRAME, FRAME)
    for i in range(FRAMES):
        data += struct.pack('<iffffff', 0, frametime, float((i % COLUMNS) * FRAME), float((i // COLUMNS) * FRAME), float(FRAME), 0.0, 0.0)
        data += struct.pack('<f', float(FRAME))
    return data


PHYSICS = {"a": "1 0 0", "gd": "0 -1 0", "ge": False, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
           "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
           "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 20,
           "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
           "re": False, "rf": 3, "ri": 30, "rs": 200, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
           "taz": True, "tf": 3, "ti": 30, "tm": 60.0, "tp": "62.5 0 0", "ts": 200}
ROWS = ('A', 'B', 'C', 'D', 'F')


def solid(w, name, x, y, colour):
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "160 120", "color": colour, "alpha": 1,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "%d %d 0" % (x, y)})


for fps in (30, 60):
    name = 'script-cadence' + ('' if fps == 30 else '-%d' % fps)
    w = Wallpaper(name, 'how often layer scripts run, %d fps' % fps, clear='0.1 0.1 0.2')
    vertices, indices = strip_mesh(height=50.0, bones=3)
    rest = [[((0, 0, 0), (0, 0, 0), (1, 1, 1))] * 2 for _ in range(3)]
    write_binary(name + '/models/strip_puppet.mdl', puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0), json.dumps(PHYSICS)), ('tip', 1, (150, 0, 0))],
        [('idle', 30.0, rest)]))
    strip = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
    # the counted layers
    w.add({"name": "B counter", "image": "models/util/solidlayer.json", "size": "10 10", "color": "1 1 1", "alpha": 1,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": False,
           "origin": {"value": "960 100 0", "script": COUNT.replace('KEY', 'b').replace('EXTRA', ''), "scriptproperties": {}}})
    w.add({"name": "C counter", "image": "models/util/solidlayer.json", "size": "10 10", "color": "1 1 1", "alpha": 1,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
           "origin": {"value": "-5000 -5000 0", "script": COUNT.replace('KEY', 'c').replace('EXTRA', ''), "scriptproperties": {}}})
    w.add({"name": "D puppet", "image": strip, "size": "600 50", "color": "1 1 1", "alpha": 1, "angles": "0 0 0",
           "scale": "1 1 1", "alignment": "center",
           "origin": {"value": "1250 950 0", "script": COUNT.replace('KEY', 'd').replace('EXTRA', KICK), "scriptproperties": {}},
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
    # what the picture shows, a column of rows
    for i, row in enumerate(ROWS):
        x, y = 500 + i * 200, 540
        if row == 'A':
            colour = {"value": "0 0 0", "script": OWN, "scriptproperties": {}}
        elif row == 'F':
            colour = {"value": "0 0 0", "script": SHOW.replace('VALUE', 'Math.min(999, Math.round(engine.frametime * 10000))'),
                      "scriptproperties": {}}
        else:
            colour = {"value": "0 0 0", "script": SHOW.replace('VALUE', '(S.%s === undefined ? 0 : S.%s)' % (row.lower(), row.lower())),
                      "scriptproperties": {}}
        solid(w, 'row ' + row, x, y, colour)
    write_binary(os.path.join(name, 'materials', 'frames.tex'), sprite_tex(0.01))
    w.files['models/frames.json'] = {"material": "materials/frames.json", "width": FRAME, "height": FRAME}
    w.files['materials/frames.json'] = {"passes": [{
        "shader": "genericimage2", "blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
        "depthwrite": "disabled", "combos": {"SPRITESHEET": 1, "VERSION": 2}, "textures": ["frames"]}]}
    w.add({"name": "row S", "image": "models/frames.json", "size": "8 8", "color": "1 1 1", "alpha": 1,
           "angles": "0 0 0", "scale": "20 15 1", "alignment": "center", "visible": True, "origin": "1500 540 0"})
    w.save()
    path = os.path.join(HERE, name, 'scene.json')
    with open(path) as f:
        scene = json.load(f)
    scene['general']['spritesheetrefreshsync'] = True
    with open(path, 'w') as f:
        json.dump(scene, f, indent=2)
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": fps}, f)
    clock_probe.stamp(os.path.join(HERE, name))
