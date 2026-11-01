# light-tube-scale: a tube light stretched to scale 2 along its x axis, before a lit wall. Its segment stretches with it
# in Wallpaper Engine as in owe (a guard: the old experiments lx-tube-a-1.5 and -a-2 had references from before their
# scale was set, and seemed to say it kept its length).
w = Scene3D('light-tube-scale', 'a stretched tube light keeps its length', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('wall', lit, 0, 0, -1, (0, 0, 0), '12 7 0.2')
w.light('light', 'ltube', 0, 0, 3, intensity=6, radius=20, scale='2 1 1')
w.save()
with open(os.path.join(HERE, 'light-tube-scale', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'light-tube-scale'))
