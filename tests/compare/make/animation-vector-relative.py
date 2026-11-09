# animation-vector-relative: does a vector animation without a "relative" key take the place of the value for origin, scale
# and angles too, as it does for colour (animation-color-relative), while one with the key (true or false) is added?
# Each pair: left square without the key, right one with relative false.
w = Wallpaper('animation-vector-relative', 'vector animations without a relative key', clear='0.1 0.1 0.15')
def keyed(values, key):
    a = constant_animation(values)
    if not key:
        del a['relative']
    return a
for i, key in enumerate((False, True)):
    x = 300 + i * 300
    # origin: value 0 400 0 + animation x,600 -> add: (x, 1000), replace: (x, 600)
    w.solid('origin %s' % key, 0, 0, 120, 120, color='1 0.3 0.3',
            origin={"value": "0 400 0", "animation": keyed([x, 600, 0], key)})
    # scale: value 1 1 1, animation 1 1 1 on a 100 square -> add: 200, replace: 100
    w.solid('scale %s' % key, 1100 + i * 300, 700, 100, 100, color='0.3 1 0.3', scale={"value": "1 1 1", "animation": keyed([1, 1, 1], key)})
    # angles: value 0 0 0.4, animation 0 0 0.4 (radians) -> add: 0.8, replace: 0.4
    w.solid('angles %s' % key, 1100 + i * 300, 300, 240, 40, color='0.3 0.6 1', angles={"value": "0 0 0.4", "animation": keyed([0, 0, 0.4], key)})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))
