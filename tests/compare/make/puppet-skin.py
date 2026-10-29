# puppet-skin: a strip skinned to two bones, the second bent 30 degrees by the animation's one pose, and a copy of
# the strip with no animation. Both engines bend the right half of the first and leave the second straight.
import math
w = Wallpaper('puppet-skin', 'a skinned strip bent by an animation', clear='0 0 0')
width, height, columns = 600.0, 100.0, 9
vertices, indices = [], []
for row in range(2):
    for column in range(columns):
        x = -width / 2 + width * column / (columns - 1)
        blend = min(1.0, max(0.0, (x + width / 4) / (width / 2)))
        vertices.append((x, (row - 0.5) * height, 0, column / (columns - 1), row, [0, 1, 0, 0], [1 - blend, blend, 0, 0]))
for column in range(columns - 1):
    a, b, c, d = column, column + 1, columns + column, columns + column + 1
    indices += [(a, b, c), (b, d, c)]
bent = [((0, 0, 0), (0, 0, 0), (1, 1, 1))] * 2, [((0, 0, 0), (0, 0, math.radians(30)), (1, 1, 1))] * 2
write_binary('puppet-skin/models/strip_puppet.mdl', puppet_mdl('materials/strip.json', vertices, indices,
             [('', None, (0, 0, 0)), ('', 0, (0, 0, 0))], [('bend', 30.0, list(bent))]))
write_binary('puppet-skin/models/still_puppet.mdl', puppet_mdl('materials/still.json', vertices, indices,
             [('', None, (0, 0, 0)), ('', 0, (0, 0, 0))]))
animated = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
still = w.texture_model('still', 'gradient/gradient_rainbow', model_extra={"puppet": "models/still_puppet.mdl"})
w.image('animated', animated, 960, 800, 600, 100,
        animationlayers=[{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "bend"}])
w.image('still', still, 960, 300, 600, 100)
w.save()
