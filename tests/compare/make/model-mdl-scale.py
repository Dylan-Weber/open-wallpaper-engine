# model-mdl-scale: a sphere model of a workshop wallpaper (More Physics' checkered sphere) at scales 0.6, 1 and 2, in a
# row in front of the camera. The sizes of the three show whether a model's scale means the same to both engines: More
# Physics' sphere pile looked about 1.7 times larger in owe.
import shutil
SRC = os.environ.get('MP_UNPACKED', '/tmp/mp-unpacked')
w = Scene3D('model-mdl-scale', 'a workshop sphere model at three scales', ambient='1 1 1')
for rel in ('models/checkered_sphere/checkered_sphere.mdl', 'materials/models/checkered_sphere/DefaultMaterial.json',
            'materials/checker.tex'):
    dest = os.path.join(HERE, 'model-mdl-scale', rel)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    # the copies are committed: without the unpacked wallpaper, keep them
    if os.path.exists(os.path.join(SRC, rel)) or not os.path.exists(dest):
        shutil.copyfile(os.path.join(SRC, rel), dest)
for i, scale in enumerate((0.6, 1, 2)):
    w.add({"name": "sphere %g" % scale, "model": "models/checkered_sphere/checkered_sphere.mdl",
           "origin": vec(-4 + i * 4, 0, 0), "angles": vec(0, 0, 0), "scale": "%g %g %g" % (scale, scale, scale)})
w.save()
with open(os.path.join(HERE, 'model-mdl-scale', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'model-mdl-scale'))
