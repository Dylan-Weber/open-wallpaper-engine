# model-wall-material: More Physics' arena wall model (dev_cube_mat2: generic4, alphatocoverage, alpha 0, textures
# "cubic", FOG on) in front of a blue cube. Wallpaper Engine draws nothing of it; owe drew dark boxes.
import shutil
SRC = os.environ.get('MP_UNPACKED', '/tmp/mp-unpacked')
w = Scene3D('model-wall-material', 'a transparent wall of a workshop wallpaper', ambient='1 1 1')
for rel in ('models/dev_cube_mat2/dev_cube_mat2.mdl', 'materials/models/dev_cube_mat2/DefaultMaterial.json',
            'materials/cubic.tex'):
    dest = os.path.join(HERE, 'model-wall-material', rel)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    # the copies are committed: without the unpacked wallpaper, keep them
    if os.path.exists(os.path.join(SRC, rel)) or not os.path.exists(dest):
        shutil.copyfile(os.path.join(SRC, rel), dest)
blue = w.material('blue', '0 0 1')
w.cube('behind', blue, 0.4, 0.3, -3, (0, 0.5, 0), '1.5 1.5 1.5')
w.add({"name": "wall", "model": "models/dev_cube_mat2/dev_cube_mat2.mdl", "origin": vec(0, 0, 0),
       "angles": vec(0.3, 0.5, 0), "scale": "2 2 2"})
w.save()
with open(os.path.join(HERE, 'model-wall-material', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'model-wall-material'))
