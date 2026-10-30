# effect-clouds: the shipped clouds effect: its defaults, its default speeds and scales written out, still (speed 0),
# a fast one (speed 0.1 -0.2) and another scale, on a picture of clouds. Static cells show whether the pattern is
# right, moving ones whether the clock is.
SHIPPED_PASSES['clouds'] = 1
effect_grid('effect-clouds', 'clouds effect settings', [
    ('plain', {"passes": [], "file": "", "name": "", "visible": False}),
    ('default', shipped('clouds')),
    ('written out', shipped('clouds', {"speed": "0.01 -0.02", "scale": "1.3 1.3 0.5 0.5"})),
    ('still', shipped('clouds', {"speed": "0 0"})),
    ('fast', shipped('clouds', {"speed": "0.1 -0.2"})),
    ('scale 2', shipped('clouds', {"speed": "0 0", "scale": "2 2 1 1"})),
    ('feather 0.2', shipped('clouds', {"speed": "0 0", "feather": 0.2})),
])
path = os.path.join(HERE, 'effect-clouds', 'scene.json')
scene = json.load(open(path))
scene['objects'][0]['effects'] = []
json.dump(scene, open(path, 'w'), indent=2)
with open(os.path.join(HERE, 'effect-clouds', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'effect-clouds'))
