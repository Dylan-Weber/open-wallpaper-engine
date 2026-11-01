# texture-animation-play: the rose petal sprite sheet playing freely on three layers, one of them made visible by a script
# after a second: which frame each shows at the moment the clock probe records (Dino Run's spinning coin shows another
# frame in owe than in Wallpaper Engine).
w = Wallpaper('texture-animation-play', 'sprite sheet animations playing', clear='0.1 0.1 0.2')
petals = w.texture_model('petals', 'particle/nature/rosepetals')
for i, x in enumerate((350, 800)):
    w.add({"name": "playing %d" % i, "image": petals, "size": "205 256", "color": "1 1 1", "alpha": 1,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True, "origin": vec(x, 600, 0)})
w.save()
with open(os.path.join(HERE, 'texture-animation-play', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'texture-animation-play'))
