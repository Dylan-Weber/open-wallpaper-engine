# bloom-defaults: a scene that turns bloom on and says no more. Wallpaper Engine blooms with its editor's defaults
# (strength 2, threshold 0.65): the bright shapes glow, the dim one doesn't (owe #16).
w = Wallpaper('bloom-defaults', 'bloom on, strength and threshold left out', clear='0 0 0')
w.solid('white', 400, 700, 200, 200, '1 1 1')
w.solid('red', 960, 700, 200, 200, '1 0 0')
w.solid('dim', 1500, 700, 200, 200, '0.3 0.3 0.3')
w.solid('thin line', 960, 300, 800, 8, '1 1 1')
w.save()
path = os.path.join(HERE, 'bloom-defaults', 'scene.json')
scene = json.load(open(path))
scene['general'].update({"bloom": True})
json.dump(scene, open(path, 'w'), indent=2)
