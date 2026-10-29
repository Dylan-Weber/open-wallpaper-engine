# puppet-one-frame: an animation of no length (one frame) that bends the strip's second bone, looping. A looping
# animation divides by its length when it interpolates (owe #11).
import math
w = Wallpaper('puppet-one-frame', 'a looping animation of one frame', clear='0 0 0')
vertices, indices = strip_mesh()
one = (1, 1, 1)
pose = [[((0, 0, 0), (0, 0, 0), one)], [((0, 0, 0), (0, 0, math.radians(30)), one)]]
write_binary('puppet-one-frame/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices, [('', None, (0, 0, 0)), ('', 0, (0, 0, 0))], [('idle', 30.0, pose)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
w.image('strip', model, 960, 540, 600, 100,
        animationlayers=[{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}])
w.save()
