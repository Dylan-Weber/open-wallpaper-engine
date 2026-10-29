# 14. particles-static: particle systems whose particles do not move: placed by sequence initializers and fixed values
ring = particle_system([{"name": "mapsequencearoundcontrolpoint", "controlpoint": 0, "count": 12, "speedmin": "0 0 0",
                         "speedmax": "0 0 0"}], 12, color='255 128 0', distance=20, emitter='sphererandom')
line = particle_system([{"name": "mapsequencebetweencontrolpoints", "controlpointstart": 0, "controlpointend": 1,
                         "count": 10}], 10, size=30, color='0 200 255', controlpoints=[
    {"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "400 0 0"}])
single = particle_system([], 1, size=120, color='50 255 50', alpha=0.6)

# split in three wallpapers, so that one that Wallpaper Engine cannot take is easy to find
w = Wallpaper('particles-ring', 'particles placed on a ring around a control point', clear='0 0 0')
w.files['particles/ring.json'] = ring
w.add({"name": "ring", "particle": "particles/ring.json", "origin": "960 540 0", "scale": "6 6 6",
       "angles": "0 0 0", "visible": True})
w.save()

w = Wallpaper('particles-line', 'particles placed between two control points', clear='0 0 0')
w.files['particles/line.json'] = line
w.add({"name": "line", "particle": "particles/line.json", "origin": "700 540 0", "scale": "1 1 1",
       "angles": "0 0 0", "visible": True})
w.save()

w = Wallpaper('particles-single', 'single particles: size, scale, rotation', clear='0 0 0')
w.files['particles/single.json'] = single
w.add({"name": "single", "particle": "particles/single.json", "origin": "400 540 0", "scale": "1 1 1",
       "angles": "0 0 0", "visible": True})
w.add({"name": "single big", "particle": "particles/single.json", "origin": "960 540 0", "scale": "2 2 1",
       "angles": "0.5 0 0.5", "visible": True})
w.add({"name": "single rotated", "particle": "particles/single.json", "origin": "1500 540 0", "scale": "1 1 1",
       "angles": "0 0 0.7854", "visible": True})
w.save()
