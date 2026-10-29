# 16. animation-final: timeline animations that have finished (single mode) by the time of the picture
w = Wallpaper('animation-final', 'finished timeline animations', clear='0.1 0.1 0.1')
# origin moves from the left edge to x = 600 (absolute), then from x = 1000 by +400 (relative)
w.add({"name": "absolute", "image": "models/util/solidlayer.json", "size": "100 100", "color": "1 0 0",
       "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
       "origin": {"value": "0 0 0", "animation": animation([[(0, 100), (6, 600)], [(0, 800), (6, 800)], [(0, 0), (6, 0)]])}})
w.add({"name": "relative", "image": "models/util/solidlayer.json", "size": "100 100", "color": "0 1 0",
       "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
       "origin": {"value": "1000 800 0", "animation": animation([[(0, 0), (6, 400)], [(0, 0), (6, 0)], [(0, 0), (6, 0)]], relative=True)}})
w.add({"name": "alpha", "image": "models/util/solidlayer.json", "size": "200 200", "color": "1 1 1",
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "400 400 0",
       "alpha": {"value": 1.0, "animation": animation([[(0, 1), (6, 0.25)]])}})
w.add({"name": "scale", "image": "models/util/solidlayer.json", "size": "100 100", "color": "0 0.5 1",
       "alpha": 1, "angles": "0 0 0", "alignment": "center", "visible": True, "origin": "900 400 0",
       "scale": {"value": "0 0 0", "animation": animation([[(0, 1), (6, 3)], [(0, 1), (6, 0.5)], [(0, 1), (6, 1)]])}})
w.add({"name": "angle", "image": "models/util/solidlayer.json", "size": "250 60", "color": "1 1 0",
       "alpha": 1, "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "1500 400 0",
       "angles": {"value": "0 0 0", "animation": animation([[(0, 0), (6, 0)], [(0, 0), (6, 0)], [(0, 0), (6, 0.7854)]])}})
w.add({"name": "colour", "image": "models/util/solidlayer.json", "size": "200 120", "color": {"value": "0 0 0", "animation": animation([[(0, 1), (6, 0)], [(0, 0), (6, 1)], [(0, 0), (6, 0.5)]])},
       "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "400 150 0"})
w.save()
