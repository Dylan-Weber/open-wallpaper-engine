# camera-parallax-perspective: camera-parallax-delay's squares (depths 0, 0.5, 1, 2; the settings of ws2959390130: amount 0.21,
# delay 1.17, mouse influence 0.22) on layers with "perspective": true, which that wallpaper's orbs are. Its orbs sat 7 px
# right and 6 px up of Wallpaper Engine's in owe, its non-perspective layers matching. A second row of the same squares is
# not perspective, for comparison.
w = Wallpaper('camera-parallax-perspective', 'camera parallax on perspective layers', clear='0.05 0.05 0.1')
for k, (depth, color) in enumerate(((0, '1 1 1'), (0.5, '1 0.3 0.3'), (1, '0.3 1 0.3'), (2, '0.4 0.5 1'))):
    for j, (y, perspective) in enumerate(((300, True), (780, False))):
        w.add({"name": "depth %s %d" % (depth, j), "image": "models/util/solidlayer.json", "size": "160 160",
               "color": color, "alpha": 1, "origin": "%d %d 0" % (360 + 400 * k, y), "angles": "0 0 0",
               "scale": "1 1 1", "alignment": "center", "visible": True, "parallaxDepth": "%s %s" % (depth, depth),
               "perspective": perspective})
w.save()
path = os.path.join(HERE, 'camera-parallax-perspective', 'scene.json')
scene = json.load(open(path))
scene['general'].update({"cameraparallax": True, "cameraparallaxamount": 0.21, "cameraparallaxdelay": 1.17,
                         "cameraparallaxmouseinfluence": 0.22})
json.dump(scene, open(path, 'w'), indent=2)
with open(os.path.join(HERE, 'camera-parallax-perspective', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'camera-parallax-perspective'))
