# shadow-contact: owe draws a thin dark stripe in oot-repro's sunlit floor patch, parallel to the patch's edge where a wall
# meets the floor; Wallpaper Engine doesn't. A wall standing on a floor (touching it) and a box standing apart, lit by a sun at
# Ocarina of Time's angles (0 0.323 -2.068, cascades 3 / 14.34 / 100), seen from where OoT's camera sees its courtyard floor.
w = Scene3D('shadow-contact', 'shadow where a wall stands on the floor', ambient='0.15 0.15 0.15', eye='6 2.5 6')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -0.1, 0, (0, 0, 0), '10 0.1 10')
w.cube('wall', lit, -2, 1.0, 0, (0, 0, 0), '0.3 1.0 6')
w.cube('box', lit, 2, 0.5, -2, (0, 0.6, 0), '0.6 0.5 0.6')
w.light('sun', 'ldirectional', 0, 1.519, 0, '1 1 1', 2, 30, (0.0, 0.32301, -2.06849), castshadow=True,
        cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))
