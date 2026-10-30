# camera-shake-perspective: camera shake (the settings of ws2959390130: amplitude 0.53, speed 0.81, roughness 1) in an
# orthographic scene with two rows of squares, the lower with "perspective": true, drawn through the perspective camera. Wallpaper
# Engine shakes both rows alike; owe shook only the flat one (the orbs of that wallpaper sat 7 px from Wallpaper Engine's).
w = Wallpaper('camera-shake-perspective', 'camera shake on perspective layers', clear='0.05 0.05 0.1')
for k, color in enumerate(('1 1 1', '1 0.3 0.3', '0.3 1 0.3', '0.4 0.5 1')):
    for j, (y, perspective) in enumerate(((300, False), (780, True))):
        w.add({"name": "square %d %d" % (k, j), "image": "models/util/solidlayer.json", "size": "160 160",
               "color": color, "alpha": 1, "origin": "%d %d 0" % (360 + 400 * k, y), "angles": "0 0 0",
               "scale": "1 1 1", "alignment": "center", "visible": True, "perspective": perspective})
w.save()
path = os.path.join(HERE, 'camera-shake-perspective', 'scene.json')
scene = json.load(open(path))
scene['general'].update({"camerashake": True, "camerashakeamplitude": 0.53, "camerashakespeed": 0.81,
                         "camerashakeroughness": 1.0})
json.dump(scene, open(path, 'w'), indent=2)
with open(os.path.join(HERE, 'camera-shake-perspective', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'camera-shake-perspective'))
