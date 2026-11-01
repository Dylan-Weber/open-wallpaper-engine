# camera-lookat-perspective: a 512 x 321 projection (narrower than the 16:9 screen), a picture layer the size of the
# projection and a coloured frame marking its edges, seen three ways: the camera eye 0 0 0 looking at 0 0 -1 (as Noita's
# The Work has) with the layer "perspective": true; the same camera, layer not perspective; the usual camera (eye
# 0 0 1) with the layer perspective. The projection is cropped to fill the screen; where does each place the layer?
def probe(name, title, camera, perspective):
    w = Wallpaper(name, title, clear='0.7 0.7 0.7', size=(512, 321))
    model = w.texture_model('picture', 'util/clouds_256')
    extra = {"perspective": True} if perspective else {}
    w.image('picture', model, 256, 160.5, 512, 321, **extra)
    for label, x, y, c in (('a', 20, 20, '1 0 0'), ('b', 492, 20, '0 1 0'), ('c', 20, 301, '0 0 1'),
                           ('d', 492, 301, '1 1 0')):
        w.solid(label, x, y, 30, 30, color=c, **extra)
    w.save()
    path = os.path.join(HERE, name, 'scene.json')
    scene = json.load(open(path))
    scene['camera'] = camera
    json.dump(scene, open(path, 'w'), indent=2)
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))


LOOK = {"center": "0 0 -1", "eye": "0 0 0", "up": "0 1 0"}
USUAL = {"center": "0 0 0", "eye": "0 0 1", "up": "0 1 0"}
probe('camera-lookat-perspective', 'look-at camera, perspective layers', LOOK, True)
probe('camera-lookat-flat', 'look-at camera, flat layers', LOOK, False)
probe('camera-usual-perspective', 'usual camera, perspective layers', USUAL, True)
