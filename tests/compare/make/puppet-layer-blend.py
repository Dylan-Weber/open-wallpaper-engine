# puppet-layer-blend: an animation layer at blend 0.3 / 0.5 / 0.8 / 1 whose pose is a large turn from the bone's rest. Wallpaper
# Engine lerps the layers' quaternions componentwise before it rebuilds the rest rotation (an unnormalised lerp of two unit
# quaternions is short by cos(half the angle between them), and the rebuild does not normalise), where owe nlerps; the
# picture's size and angle show which. One bone, bind = reference = 0.6 rad, the animation holds 2.4 rad (1.8 rad away).
# Cells: the puppet with no layer, a normal layer at 0.3 / 0.5 / 0.8 / 1, and an additive layer at 0.3 / 0.5 / 0.8.
src = open(os.path.join(os.path.dirname(__file__), 'effect-puppet-round.py')).read()
exec(src.split("# Gun Girl's sizes")[0], globals())

SCENE = (2400, 1350)


def blend_probe(name, title, bind_angle, pose_angle, cells):
    puppet_layers(name, title, SCENE, cells, 512, 384, 400, 320, 400, 320, bones=1,
                  bind=lambda i: (0.0, 0.0, bind_angle), animref=lambda i: (0.0, 0.0, bind_angle),
                  pose=lambda i: (0.0, 0.0, pose_angle))
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30, "probe": True, "probe_updates": True}, f)


xs, ys = (300, 900, 1500, 2100), (340, 1010)
blend_probe('puppet-layer-blend', 'a turned bone under normal and additive animation layers at blend 0.3 to 1', 0.6, 2.4,
            [(xs[0], ys[0], False),
             (xs[1], ys[0], False, 'normal', 0.3), (xs[2], ys[0], False, 'normal', 0.5), (xs[3], ys[0], False, 'normal', 0.8),
             (xs[0], ys[1], False, 'normal', 1.0),
             (xs[1], ys[1], False, 'additive', 0.3), (xs[2], ys[1], False, 'additive', 0.5), (xs[3], ys[1], False, 'additive', 0.8)])
# the same across the quaternion sign flip: bind 2.8 rad and a pose of -2.8 rad are 0.68 rad apart the short way (the
# quaternions' dot product is negative), 5.6 the long way
blend_probe('puppet-layer-blend-flip', 'a bone turned across the quaternion sign flip under animation layers at blend 0.3 to 1', 2.8, -2.8,
            [(xs[0], ys[0], False),
             (xs[1], ys[0], False, 'normal', 0.3), (xs[2], ys[0], False, 'normal', 0.5), (xs[3], ys[0], False, 'normal', 0.8),
             (xs[0], ys[1], False, 'normal', 1.0),
             (xs[1], ys[1], False, 'additive', 0.3), (xs[2], ys[1], False, 'additive', 0.5), (xs[3], ys[1], False, 'additive', 0.8)])
