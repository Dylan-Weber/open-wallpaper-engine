# effect-vertex-default: an effect whose setting is declared (with its default) in its vertex shader: film grain's
# scale (default 10) is read by its vertex shader only. Three cells on a picture of clouds: film grain with its
# defaults, with scale 10 written out, and with scale 3. If the default isn't applied, the first looks like scale 1 or 0.
SHIPPED_PASSES['filmgrain'] = 1
effect_grid('effect-vertex-default', 'film grain with its settings written out', [
    ('plain', {"passes": [], "file": "", "name": "", "visible": False}),
    ('default', shipped('filmgrain')),
    ('exponent 0.5', shipped('filmgrain', {"strength": 2, "exponent": 0.5})),
    ('exponent 1', shipped('filmgrain', {"strength": 2, "exponent": 1})),
    ('exponent 2', shipped('filmgrain', {"strength": 2, "exponent": 2})),
    ('exponent 0.25', shipped('filmgrain', {"strength": 2, "exponent": 0.25})),
    ('greyscale off', shipped('filmgrain', {"strength": 2, "exponent": 0.5}, {"GREYSCALE": 0})),
])
path = os.path.join(HERE, 'effect-vertex-default', 'scene.json')
scene = json.load(open(path))
scene['objects'][0]['effects'] = []
json.dump(scene, open(path, 'w'), indent=2)
with open(os.path.join(HERE, 'effect-vertex-default', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'effect-vertex-default'))
