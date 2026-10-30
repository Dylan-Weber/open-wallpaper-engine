# camera-perspective-override: an orthographic 512 x 321 scene (narrower than the 16:9 screen, so the window crops it)
# with "perspectiveoverridefov" 30, a perspective layer the size of the projection and four corner markers. Wallpaper
# Engine fits the perspective layer to the cropped view as it does the ortho one (it fills the screen's width);
# owe kept the perspective camera at the uncropped distance, so the layer came out narrower with bars at the sides.
# The second wallpaper has no override: the same cells must agree there.
for name, override in (('camera-perspective-override', 30), ('camera-perspective-plain', None)):
    w = Wallpaper(name, 'perspective layer, override fov %s' % override, clear='0.7 0.7 0.7', size=(512, 321))
    model = w.texture_model('picture', 'util/clouds_256')
    w.image('picture', model, 256, 160.5, 512, 321, perspective=True)
    for label, x, y, c in (('a', 20, 20, '1 0 0'), ('b', 492, 20, '0 1 0'), ('c', 20, 301, '0 0 1'),
                           ('d', 492, 301, '1 1 0')):
        w.solid(label, x, y, 30, 30, color=c, perspective=True)
    w.save()
    path = os.path.join(HERE, name, 'scene.json')
    scene = json.load(open(path))
    if override:
        scene['general']['perspectiveoverridefov'] = override
        scene['general']['fov'] = 50
    json.dump(scene, open(path, 'w'), indent=2)
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))
