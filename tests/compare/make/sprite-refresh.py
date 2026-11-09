# sprite-refresh: how a sprite sheet plays when its frames are short, with and without the scene's
# general.spritesheetrefreshsync (set by Wallpaper Engine's GIF template, gifscene.json). A sheet of 1000 frames, each its
# own colour (sprite_refresh_read.py tells the frame from the colour), at frame times 0.1, 0.06, 0.03 and 0.01 s; each with
# the flag on and off, at 30 and 60 fps. Comparing the frame with the probe's g_Time says whether the sheet plays at its
# frames' rate, at one frame an update, or something else. -gif is the same sheet in a gifscene.json laid out like the
# Workshop GIF wallpapers (autosize image, genericimage, flag on).
import struct

# a solid layer left of the sheet counts the updates: its colour script steps through the sheet's palette once an update, so
# the picture says how many updates Wallpaper Engine ran as well as its g_Time
COUNTER = """'use strict';
let n = -1;
export function update(value) {
    n = (n + 1) % 1000;
    return new Vec3((n % 256) / 255, (Math.floor(n / 256) * 60 + 20) / 255, ((n * 7) % 256) / 255);
}
"""


def counter(objects, next_id):
    objects.append({"id": next_id, "name": "update counter", "image": "models/util/solidlayer.json", "size": "200 200",
                    "color": {"value": "0 0 0", "script": COUNTER, "scriptproperties": {}}, "alpha": 1, "angles": "0 0 0",
                    "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "300 540 0"})

FRAMES, COLUMNS, FRAME = 1000, 40, 8


def frame_color(i):
    return (i % 256, (i // 256) * 60 + 20, (i * 7) % 256)


def sprite_tex(frametime):
    side_w, side_h = COLUMNS * FRAME, ((FRAMES + COLUMNS - 1) // COLUMNS) * FRAME
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
        data += struct.pack('<iffffff', 0, frametime, float((i % COLUMNS) * FRAME), float((i // COLUMNS) * FRAME), float(FRAME), 0.0, 0.0)
        data += struct.pack('<f', float(FRAME))
    return data


def set_general(name, scene_file, **values):
    path = os.path.join(HERE, name, scene_file)
    with open(path) as f:
        scene = json.load(f)
    scene['general'].update(values)
    with open(path, 'w') as f:
        json.dump(scene, f, indent=2)


def finish(name, fps):
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": fps}, f)
    clock_probe.stamp(os.path.join(HERE, name))


for frametime in (0.1, 0.06, 0.03, 0.01):
    for sync in (True, False):
        for fps in (15, 30, 60):
            name = 'sprite-refresh-%03d-%s%s' % (round(frametime * 1000), 'sync' if sync else 'nosync', '-%d' % fps if fps != 30 else '')
            w = Wallpaper(name, 'sprite sheet of %g s frames, refresh sync %s, %d fps' % (frametime, sync, fps), clear='0.1 0.1 0.2')
            write_binary(os.path.join(name, 'materials', 'frames.tex'), sprite_tex(frametime))
            w.files['models/frames.json'] = {"material": "materials/frames.json", "width": FRAME, "height": FRAME}
            w.files['materials/frames.json'] = {"passes": [{
                "shader": "genericimage2", "blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
                "depthwrite": "disabled", "combos": {"SPRITESHEET": 1, "VERSION": 2}, "textures": ["frames"]}]}
            w.add({"name": "sheet", "image": "models/frames.json", "size": "8 8", "color": "1 1 1", "alpha": 1,
                   "angles": "0 0 0", "scale": "60 60 1", "alignment": "center", "visible": True, "origin": "960 540 0"})
            counter(w.objects, w.next_id)
            w.save()
            set_general(name, 'scene.json', spritesheetrefreshsync=sync)
            finish(name, fps)

# the GIF template's layout: gifscene.json, auto projection, an autosize image layer drawn by genericimage
name = 'sprite-refresh-gif'
write_binary(os.path.join(name, 'materials', 'background.tex'), sprite_tex(0.03))
write(name + '/project.json', {"title": "wpe-test sprite sheet in a GIF template scene", "type": "scene",
                               "file": "gifscene.json", "version": 1, "general": {"properties": {}}})
write(name + '/models/background.json', {"material": "materials/background.json", "nopadding": True, "autosize": True})
write(name + '/materials/background.json', {"passes": [{
    "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled", "shader": "genericimage",
    "textures": ["background"], "combos": {"spritesheet": 1}}]})
write(name + '/gifscene.json', {
    "camera": {"center": "0.000 0.000 -1.000", "eye": "0.000 0.000 0.000", "up": "0.000 1.000 0.000"},
    "general": {"ambientcolor": "0.000 0.000 0.000", "bloom": False, "clearcolor": "0.000 0.000 0.000",
                "orthogonalprojection": {"width": 1920, "height": 1080}, "skylightcolor": "0.000 0.000 0.000",
                "spritesheetrefreshsync": True},
    "objects": [{"angles": "0.000 0.000 0.000", "id": 3, "image": "models/background.json", "name": "",
                 "origin": "960.000 540.000 0.000", "scale": "60.000 60.000 1.000"}]})
with open(os.path.join(HERE, name, 'gifscene.json')) as f:
    scene = json.load(f)
counter(scene['objects'], 4)
with open(os.path.join(HERE, name, 'gifscene.json'), 'w') as f:
    json.dump(scene, f, indent=2)
finish(name, 30)
