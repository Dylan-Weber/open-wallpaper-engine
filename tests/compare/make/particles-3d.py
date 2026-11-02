# 14b. particles-3d: particles in a 3D scene, placed in the world and seen by its camera (DNA Fragment's sparkles);
# three single particles of world size 1 left, centre (further away) and right, and a cube for scale
w = Scene3D('particles-3d', 'particles in a 3D scene', ambient='1 1 1')
w.files['particles/single.json'] = particle_system([], 1, size=1, color='50 255 50', alpha=0.6)
for name, x, z in (('left', -3, 0), ('far', 0, -6), ('right', 3, 0)):
    w.add({"name": name, "particle": "particles/single.json", "origin": vec(x, 0, z), "scale": "1 1 1",
           "angles": "0 0 0", "visible": True})
w.cube('cube', w.material('white', '1 1 1'), 0, -2.5, 0, (0.3, 0.5, 0), '1 1 1')
w.save()
