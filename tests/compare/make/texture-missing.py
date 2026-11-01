# texture-missing: an image layer whose material names a texture that doesn't exist: Wallpaper Engine draws a yellow and
# black checkerboard there (attach-probe's Link, whose face samples a removed layer's composite); owe drew whatever was
# bound before. Two sizes, to see the checker's cells.
w = Wallpaper('texture-missing', 'a texture that does not exist', clear='0.1 0.1 0.2')
missing = w.texture_model('missing', 'this/texture/does/not/exist')
for k, (x, size) in enumerate(((560, 256), (1300, 512))):
    w.add({"name": "missing %d" % k, "image": missing, "size": "%d %d" % (size, size), "color": "1 1 1", "alpha": 1,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "%d 540 0" % x})
w.save()
with open(os.path.join(HERE, 'texture-missing', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'texture-missing'))
