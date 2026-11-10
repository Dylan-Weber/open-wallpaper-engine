# sprite-created-coins: Dino Run's coins (a 4-frame sheet of 0.2 s frames, layers a script creates every 5 s) show another
# frame in Wallpaper Engine than in owe: the coin made 5 s earlier is one frame behind the newer (25 frames = 1 mod 4),
# where owe draws both on one clock. Four colour frames of 0.2 s; layers created at 3, 8 and 13 s (as the coin script does),
# read at 15 s. Variants: `-fixed` (never destroyed), `-destroy` (the first destroyed at 10.4 s, on a frame's edge, or 10.5 s: `-destroy-mid`), `-coin` (the coin script:
# autosize, moved each update, sortLayer, destroyed when off the left edge). Read with sprite_created_coins_read.py.
import struct

FRAMES, FRAME_SIZE, FRAMETIME = 4, 8, 0.2
COLORS = [(230, 40, 40), (40, 200, 60), (50, 70, 230), (240, 220, 30)]


def sprite_sheet():
    side = FRAME_SIZE
    width, height = FRAMES * side, side
    pixels = bytearray(width * height * 4)
    for i, color in enumerate(COLORS):
        for y in range(side):
            for x in range(i * side, (i + 1) * side):
                pixels[(y * width + x) * 4:(y * width + x) * 4 + 4] = bytes(color + (255,))
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', 0, 7, width, height, width, height, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', width, height, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    data += b'TEXS0003\0' + struct.pack('<III', FRAMES, side, side)
    for i in range(FRAMES):
        data += struct.pack('<iffffff', 0, FRAMETIME, float(i * side), 0.0, float(side), 0.0, 0.0) + struct.pack('<f', float(side))
    return data


def make_coins(name, title, script_body):
    w = Wallpaper(name, title, clear='0.1 0.1 0.2')
    write_binary(os.path.join(name, 'materials', 'frames.tex'), sprite_sheet())
    w.files['models/frames.json'] = {"material": "materials/frames.json", "autosize": True}
    w.files['materials/frames.json'] = {"passes": [{
        "shader": "genericimage2", "blending": "translucent", "cullmode": "nocull", "depthtest": "disabled",
        "depthwrite": "disabled", "combos": {"SPRITESHEET": 1, "VERSION": 2}, "textures": ["frames"]}]}
    w.add({"name": "maker", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
           "origin": {"value": "10 10 0", "script": script_body, "scriptproperties": {}}})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30, "frames": 450}, f)
    clock_probe.stamp(os.path.join(HERE, name))


FIXED = """'use strict';
const asset = engine.registerAsset('models/frames.json');
const times = [3, 8, 13];
const doomed = %s;
let made = 0;
let destroyed = false;
const layers = [];
export function update(value) {
    while (made < times.length && engine.runtime >= times[made]) {
        const layer = thisScene.createLayer(asset);
        layer.origin = new Vec3(500 + 500 * made, 540, 0);
        layer.scale = new Vec3(30, 30, 1);
        layers.push(layer);
        made++;
    }
    if (doomed > 0 && !destroyed && engine.runtime >= doomed) {
        destroyed = true;
        thisScene.destroyLayer(layers.shift());
    }
    return value;
}
"""
# the first layer is destroyed at doomed s (0: never)
make_coins('sprite-created-coins-fixed', 'sprite layers created every 5 s, kept', FIXED % '0')
make_coins('sprite-created-coins-destroy', 'sprite layers created every 5 s, the first destroyed',
           FIXED % '10.4')
# not on a frame's edge: 10.5 - 3 s is 37.5 frames
make_coins('sprite-created-coins-destroy-mid', 'sprite layers created every 5 s, the first destroyed mid-frame',
           FIXED % '10.5')
make_coins('sprite-created-coins-coin', 'Dino Run coins: created every 5 s, moved, sorted, destroyed', """'use strict';
const asset = engine.registerAsset('models/frames.json');
let coins = [];
let timer = 3.0;
export function update(value) {
    const dt = engine.frametime;
    coins.forEach((coin) => {
        const origin = coin.origin;
        origin.x -= dt * 230;
        if (origin.x < -10) {
            thisScene.destroyLayer(coin);
            coins.splice(coins.indexOf(coin), 1);
        } else {
            coin.origin = origin;
        }
    });
    timer -= dt;
    if (timer < 0.0) {
        timer = 5.0;
        const layer = thisScene.createLayer(asset);
        thisScene.sortLayer(layer, 0);
        layer.origin = new Vec3(1700, 540, 0);
        layer.scale = new Vec3(30, 30, 1);
        coins.push(layer);
    }
    return value;
}
""")
