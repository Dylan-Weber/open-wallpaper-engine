# shipped-effects-color: colour effects that ship with Wallpaper Engine, one to a cell
effect_grid('shipped-effects-color', 'colour effects that ship with Wallpaper Engine', [
    ('plain', {"passes": [], "file": "", "name": "", "visible": False}),
    ('tint red', shipped('tint', {"color": "1 0 0", "alpha": 1})),
    ('tint half green', shipped('tint', {"color": "0 1 0", "alpha": 0.5})),
    ('tint multiply', shipped('tint', {"color": "1 0.5 0"}, {"BLENDMODE": 2})),
    ('opacity half', shipped('opacity', {"alpha": 0.5})),
    ('colorkey', shipped('colorkey', {"color": "1 1 1", "tolerance": 0.6, "fuzziness": 0.3})),
    ('colorkey inverted', shipped('colorkey', {"color": "1 1 1", "tolerance": 0.6}, {"INVERT": 1})),
    ('edge detection', shipped('edgedetection', {"outlinecolor": "0 0 0", "outlinecolorbg": "1 1 1",
                                                  "detectthreshold": 0.3})),
])
# the first cell is the picture without an effect
path = os.path.join(HERE, 'shipped-effects-color', 'scene.json')
scene = json.load(open(path))
scene['objects'][0]['effects'] = []
json.dump(scene, open(path, 'w'), indent=2)
