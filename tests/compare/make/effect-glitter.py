# effect-glitter: the shipped glitter effect (two passes: the sparkle tiles, then their combination with the picture)
# at its defaults, with its default settings written out in each pass, and with another speed, density, scale and
# brightness, on a picture of clouds, at 30 frames a second.
SHIPPED_PASSES['glitter'] = 2


def glitter(first=None, second=None, id=0):
    instance = shipped('glitter', first, None, id=id)
    if second:
        instance['passes'][1]["constantshadervalues"] = second
    return instance


effect_grid('effect-glitter', 'glitter effect settings', [
    ('plain', {"passes": [], "file": "", "name": "", "visible": False}),
    ('default', glitter()),
    ('written out', glitter({"speed": 1, "density": 0.5}, {"scale": 1, "alpha": 1, "color": "1 1 1"})),
    ('density 0.1', glitter({"speed": 1, "density": 0.1}, {})),
    ('density 1', glitter({"speed": 1, "density": 1}, {})),
    ('scale 3', glitter({"speed": 1, "density": 0.5}, {"scale": 3})),
    ('brightness 0.3', glitter({"speed": 1, "density": 0.5}, {"alpha": 0.3})),
])
path = os.path.join(HERE, 'effect-glitter', 'scene.json')
scene = json.load(open(path))
scene['objects'][0]['effects'] = []
json.dump(scene, open(path, 'w'), indent=2)
with open(os.path.join(HERE, 'effect-glitter', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'effect-glitter'))
