# camera-parallax-delay: camera-parallax with the settings of the Workshop wallpaper ws2959390130 (amount 0.21, delay 1.17,
# mouse influence 0.22) and with a long delay (4 s): its orbs at depth ~1.9 sat 7 px right and 6 px up of Wallpaper Engine's in
# owe while its other layers matched. The pointer is where the capture leaves it (outside the window); the parallax eases to
# its target over the delay, so the picture shows how far it got.
for name, amount, delay, influence in (('camera-parallax-delay', 0.21, 1.17, 0.22), ('camera-parallax-delay-long', 0.21, 4.0, 0.22)):
    w = Wallpaper(name, 'camera parallax, delay %g' % delay, clear='0.05 0.05 0.1')
    for k, (depth, color) in enumerate(((0, '1 1 1'), (0.5, '1 0.3 0.3'), (1, '0.3 1 0.3'), (2, '0.4 0.5 1'))):
        for j, y in enumerate((300, 780)):
            w.add({"name": "depth %s %d" % (depth, j), "image": "models/util/solidlayer.json", "size": "160 160",
                   "color": color, "alpha": 1, "origin": "%d %d 0" % (360 + 400 * k, y), "angles": "0 0 0",
                   "scale": "1 1 1", "alignment": "center", "visible": True, "parallaxDepth": "%s %s" % (depth, depth)})
    w.save()
    path = os.path.join(HERE, name, 'scene.json')
    scene = json.load(open(path))
    scene['general'].update({"cameraparallax": True, "cameraparallaxamount": amount, "cameraparallaxdelay": delay,
                             "cameraparallaxmouseinfluence": influence})
    json.dump(scene, open(path, 'w'), indent=2)
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))
