# camera-parallax: the camera parallax with the pointer where the capture leaves it (parked outside the window): squares
# at parallax depths 0, 0.5, 1 and 2, with Reze's settings (amount 0.1, delay 0.4, mouse influence 0.6). Reze's whole
# picture sat 6 to 9 px from Wallpaper Engine's, its layers at different depths by different amounts.
w = Wallpaper('camera-parallax', 'camera parallax with the pointer outside the window', clear='0.05 0.05 0.1')
for k, (depth, color) in enumerate(((0, '1 1 1'), (0.5, '1 0.3 0.3'), (1, '0.3 1 0.3'), (2, '0.4 0.5 1'))):
    for j, y in enumerate((300, 780)):
        w.add({"name": "depth %s %d" % (depth, j), "image": "models/util/solidlayer.json", "size": "160 160",
               "color": color, "alpha": 1, "origin": "%d %d 0" % (360 + 400 * k, y), "angles": "0 0 0",
               "scale": "1 1 1", "alignment": "center", "visible": True, "parallaxDepth": "%s %s" % (depth, depth)})
w.save()
path = os.path.join(HERE, 'camera-parallax', 'scene.json')
scene = json.load(open(path))
scene['general'].update({"cameraparallax": True, "cameraparallaxamount": 0.1, "cameraparallaxdelay": 0.4,
                         "cameraparallaxmouseinfluence": 0.6})
json.dump(scene, open(path, 'w'), indent=2)
with open(os.path.join(HERE, 'camera-parallax', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'camera-parallax'))
