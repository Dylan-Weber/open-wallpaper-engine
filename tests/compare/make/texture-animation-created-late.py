# texture-animation-created-late: when does the sprite sheet of a layer a script creates late start? (ws2672187332's
# -sync12 variant: the sun GIF's layer created at 12 s shows another frame in owe; at 2 s it matches.) A sheet of 1000
# colour-coded frames of 0.03 s (sprite-refresh's; 30 s a round, so the frame says when it started to 0.03 s) on a layer
# there from the start, and on layers a script creates at 2, 7 and 12 s, the asset registered at the start (as
# sync_wallpaper.py does). Read with texture_animation_created_read.py.
FRAMES, COLUMNS, FRAME = 1000, 40, 8


def frame_color(i):
    return (i % 256, (i // 256) * 60 + 20, (i * 7) % 256)


def sprite_tex(frametime):
    side_w, side_h = COLUMNS * FRAME, ((FRAMES + COLUMNS - 1) // COLUMNS) * FRAME
    pixels = bytearray(side_w * side_h * 4)
    for i in range(FRAMES):
        x0, y0 = (i % COLUMNS) * FRAME, (i // COLUMNS) * FRAME
        cell = bytes(frame_color(i) + (255,)) * FRAME
        for y in range(y0, y0 + FRAME):
            pixels[(y * side_w + x0) * 4:(y * side_w + x0 + FRAME) * 4] = cell
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 7, side_w, side_h, side_w, side_h, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', side_w, side_h, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    data += b'TEXS0003\0' + struct.pack('<III', FRAMES, FRAME, FRAME)
    for i in range(FRAMES):
        data += struct.pack('<iffffff', 0, frametime, float((i % COLUMNS) * FRAME), float((i // COLUMNS) * FRAME),
                            float(FRAME), 0.0, 0.0) + struct.pack('<f', float(FRAME))
    return data


import struct


def make(name, from_start, times=(2, 7, 12)):
    w = Wallpaper(name, 'sprite phase of layers created at 2, 7 and 12 s', clear='0.1 0.1 0.2')
    write_binary(os.path.join(name, 'materials', 'frames.tex'), sprite_tex(0.03))
    w.files['models/frames.json'] = {"material": "materials/frames.json", "width": FRAME, "height": FRAME}
    w.files['materials/frames.json'] = {"passes": [{
        "shader": "genericimage2", "blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
        "depthwrite": "disabled", "combos": {"SPRITESHEET": 1, "VERSION": 2}, "textures": ["frames"]}]}
    if from_start:
        w.add({"name": "from the start", "image": "models/frames.json", "size": "8 8", "color": "1 1 1", "alpha": 1,
               "angles": "0 0 0", "scale": "30 30 1", "alignment": "center", "visible": True, "origin": "300 540 0"})
    script = """'use strict';
    const asset = engine.registerAsset('models/frames.json');
    const times = [%s];
    let made = 0;
    export function update(value) {
        while (made < times.length && engine.runtime >= times[made]) {
            const layer = thisScene.createLayer(asset);
            layer.origin = new Vec3(740 + 440 * made, 540, 0);
            layer.scale = new Vec3(30, 30, 1);
            made++;
        }
        return value;
    }
    """ % ', '.join(str(t) for t in times)
    w.add({"name": "maker", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
           "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30, "frames": 450}, f)
    clock_probe.stamp(os.path.join(HERE, name))


make('texture-animation-created-late', True)
# no layer uses the sheet until the first is created (as in sync_wallpaper.py's wallpapers)
make('texture-animation-created-late-only', False)
# one layer, created at 12 s: the sheet's first use (ws2672187332-clock-sync12)
make('texture-animation-created-12', False, (12,))
