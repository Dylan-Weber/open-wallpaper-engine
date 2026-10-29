# 10. bloom: the glow around bright shapes
w = Wallpaper('bloom', 'bloom', clear='0 0 0')
w.solid('white', 400, 700, 200, 200, '1 1 1')
w.solid('red', 960, 700, 200, 200, '1 0 0')
w.solid('dim', 1500, 700, 200, 200, '0.3 0.3 0.3')
w.solid('thin line', 960, 300, 800, 8, '1 1 1')
w.solid('blue', 400, 300, 120, 120, '0.2 0.2 1')
w.save()
path = os.path.join(HERE, 'bloom', 'scene.json')
scene = json.load(open(path))
scene['general'].update({"bloom": True, "bloomstrength": 2.0, "bloomthreshold": 0.5, "bloomtint": "1 1 1",
                         "hdr": False})
json.dump(scene, open(path, 'w'), indent=2)
