# shadow-sunk-wall: thin walls standing in a thick floor, their feet sunk 0, 0.2 and 0.4 into it (oot-repro's
# courtyard: the part of a wall buried in the raised floor makes WE's lit strip at the wall's foot wider; with the foot
# raised to the floor's top the two match). shadow-contact's view, OoT's sun and cascades, medium shadows.
w = Scene3D('shadow-sunk-wall', 'walls sunk into a thick floor', ambient='0.15 0.15 0.15', eye='6 2.5 6')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -0.25, 0, (0, 0, 0), '10 0.25 10')
for i, sunk in enumerate((0.0, 0.2, 0.4)):
    w.cube('wall %g' % sunk, lit, -2, 1.0 - sunk / 2, 3 - 3 * i, (0, 0, 0), '0.05 %g 1.2' % (1.0 + sunk / 2))
w.light('sun', 'ldirectional', 0, 1.519, 0, '1 1 1', 2, 30, (0.0, 0.32301, -2.06849), castshadow=True,
        cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
clock_probe.stamp(os.path.join(HERE, w.name))
