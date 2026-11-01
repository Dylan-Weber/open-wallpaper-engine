# camera-layer-fov: a camera layer (no path) drives the camera from 0 0 6, in a 3D scene whose own field of view is 80.
# The layer says 40 (first wallpaper) or 80 (second). More Physics' camera layer says 60 in a scene of 80, and
# Wallpaper Engine draws it with the scene's 80: which field of view shows the cubes this size?
for name, layer_fov in (('camera-layer-fov', 40), ('camera-layer-fov-same', 80)):
    w = Scene3D(name, 'a camera layer with a field of view of %d in a scene of 80' % layer_fov)
    w.general['fov'] = 80
    for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
        w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
    w.add({"name": "cam", "camera": "default", "origin": "0 0 6", "angles": "0 0 0", "fov": layer_fov, "zoom": 1})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))
