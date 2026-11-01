# camera-layer-emptypath: a camera layer with a path file that has no paths ("queuemode": "random", as More Physics' camera
# layer has) in a 3D scene of field of view 80; the layer says 40 and sits at 0 0 6. With no path to play, does Wallpaper
# Engine keep the layer's field of view (as camera-layer-fov shows it does without a path file) or the scene's?
w = Scene3D('camera-layer-emptypath', 'a camera layer with an empty path file')
w.general['fov'] = 80
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
w.files['scripts/camera_paths_9.json'] = {"paths": []}
w.add({"name": "cam", "camera": "default", "origin": "0 0 6", "angles": "0 0 0", "fov": 40, "zoom": 1,
       "path": "scripts/camera_paths_9.json", "queuemode": "random", "solid": True})
w.save()
with open(os.path.join(HERE, 'camera-layer-emptypath', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'camera-layer-emptypath'))
