# sprite-clock: when does a sprite sheet's animation start? A sheet of 300 frames (0.1 s each, a 30 s loop) whose every
# frame is a different colour: the colour of a layer in the picture says which frame it shows, so its sprite time to 0.1 s
# (sprite_clock_read.py). One layer is there from the start; a script creates two more from the asset (createLayer(path)) at
# 2.0 and 3.5 s. -small is a tiny sheet (loaded at once); -big has the same frames in a 4096 x 4096 texture (67 MB), which
# takes Wallpaper Engine seconds to get ready (the Workshop sun's sheet, 79 MB, started 5.6 s late).
import struct

FRAMES, COLUMNS, FRAME = 300, 20, 8


def frame_color(i):
    return (i % 256, (i // 256) * 100 + 50, (i * 3) % 256)


def sprite_tex(side_w, side_h):
    rows = (FRAMES + COLUMNS - 1) // COLUMNS
    pixels = bytearray(side_w * side_h * 4)
    for i in range(FRAMES):
        x0, y0 = (i % COLUMNS) * FRAME, (i // COLUMNS) * FRAME
        r, g, b = frame_color(i)
        for y in range(y0, y0 + FRAME):
            row = (y * side_w + x0) * 4
            pixels[row:row + FRAME * 4] = bytes((r, g, b, 255)) * FRAME
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 7, side_w, side_h, side_w, side_h, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', side_w, side_h, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    data += b'TEXS0003\0' + struct.pack('<III', FRAMES, FRAME, FRAME)
    for i in range(FRAMES):
        data += struct.pack('<iffffff', 0, 0.1, float((i % COLUMNS) * FRAME), float((i // COLUMNS) * FRAME), float(FRAME), 0.0, 0.0)
        data += struct.pack('<f', float(FRAME))
    return data


for name, side in (('sprite-clock-small', (COLUMNS * FRAME, ((FRAMES + COLUMNS - 1) // COLUMNS) * FRAME)),
                   ('sprite-clock-big', (4096, 4096))):
    w = Scene2D = Wallpaper(name, 'when a sprite sheet animation starts (%s sheet)' % name.split('-')[-1], clear='0.1 0.1 0.2')
    write_binary(os.path.join(name, 'materials', 'frames.tex'), sprite_tex(*side))
    w.files['models/frames.json'] = {"material": "materials/frames.json", "width": FRAME, "height": FRAME}
    w.files['materials/frames.json'] = {"passes": [{
        "shader": "genericimage2", "blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
        "depthwrite": "disabled", "combos": {"SPRITESHEET": 1, "VERSION": 2}, "textures": ["frames"]}]}
    w.add({"name": "from the start", "image": "models/frames.json", "size": "8 8", "color": "1 1 1", "alpha": 1,
           "angles": "0 0 0", "scale": "25 25 1", "alignment": "center", "visible": True, "origin": "500 540 0"})
    script = """'use strict';
const asset = engine.registerAsset('models/frames.json');
const times = [2.0, 3.5];
let made = 0;
export function update(value) {
    while (made < times.length && engine.runtime >= times[made]) {
        const layer = thisScene.createLayer(asset);
        layer.origin = new Vec3(1000 + 500 * made, 540, 0);
        layer.scale = new Vec3(25, 25, 1);
        made++;
    }
    return value;
}
"""
    w.add({"name": "maker", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
           "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))
