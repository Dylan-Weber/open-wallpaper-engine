# effects-sweep: the shipped effects the other grids leave out, each at its own default settings on a picture of
# clouds, 12 or fewer to a wallpaper at 30 frames a second with the clock probe (many move with time). The first cell
# of each is the picture without an effect.
import json as _json
_assets = os.environ.get('WPE_ASSETS')
GROUPS = {
    'effects-sweep-1': ['pulse', 'shake', 'scroll', 'spin', 'swing', 'shimmer', 'iris', 'waterripple', 'waterwaves',
                        'waterflow', 'xray'],
    'effects-sweep-2': ['blurradial', 'cloudmotion', 'clouds', 'filmgrain', 'fire', 'foliagesway', 'glitter',
                        'godrays', 'shine'],
    'effects-sweep-3': ['lightshafts', 'motionblur', 'nitro', 'reflection', 'refraction', 'vhs', 'watercaustics',
                        'depthparallax'],
}
for wallpaper, names in GROUPS.items():
    cells = [('plain', {"passes": [], "file": "", "name": "", "visible": False})]
    for effect in names:
        details = _json.load(open(os.path.join(_assets, 'effects', effect, 'effect.json')))
        SHIPPED_PASSES[effect] = len(details['passes'])
        cells.append((effect, shipped(effect)))
    effect_grid(wallpaper, 'shipped effects at their defaults: ' + ', '.join(names), cells)
    path = os.path.join(HERE, wallpaper, 'scene.json')
    scene = _json.load(open(path))
    scene['objects'][0]['effects'] = []
    _json.dump(scene, open(path, 'w'), indent=2)
    with open(os.path.join(HERE, wallpaper, 'compare.json'), 'w') as f:
        _json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, wallpaper))
