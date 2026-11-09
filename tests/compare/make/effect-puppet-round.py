# effect-puppet-round: autosize puppet layers (a skinned quad over a padded, not power-of-two texture, as Gun Girl's
# 3800x3000 image in a 3840x3008 texture) drawn with and without a tint effect that changes nothing. Wallpaper Engine
# draws Gun Girl 0.22 % (3808 / 3800) wider through her tints than owe does; the plain image layers of effect-width-round
# don't show it. Each wallpaper holds a plain layer and a tinted copy of it, on a stripe pattern that makes any stretch
# measurable.
import struct
import numpy


def padded_texture(w, name, tex_w, tex_h, img_w, img_h, stripe=50):
    """A texture of tex_w x tex_h whose picture (img_w x img_h, in its top-left corner) is vertical stripes with a
    colour per stripe and a bright border; the padding is transparent."""
    pixels = numpy.zeros((tex_h, tex_w, 4), numpy.uint8)
    x = numpy.arange(img_w)
    r = ((x // stripe) * 97) % 200 + 55
    g = ((x // stripe) * 53) % 200 + 55
    b = (((x // stripe) % 2) * 120 + 100)
    pixels[:img_h, :img_w, 0] = r
    pixels[:img_h, :img_w, 1] = g
    pixels[:img_h, :img_w, 2] = b
    pixels[:img_h, :img_w, 3] = 255
    pixels[:img_h:stripe, :img_w] = 255
    pixels[:img_h, 0:4] = 255
    pixels[:img_h, img_w - 4:img_w] = 255
    data = binary.tex(tex_w, tex_h, pixels.tobytes())
    # the image size in the header (after the texture size) is the picture's, as the editor writes it
    head = data.index(b'TEXI0001\0') + 9
    fmt, flags, tw, th, iw, ih, unk = struct.unpack_from('<7I', data, head)
    data = data[:head] + struct.pack('<7I', fmt, flags, tw, th, img_w, img_h, unk) + data[head + 28:]
    write_binary(os.path.join(w.name, 'materials', name + '.tex'), data)


def bone_list(count, turned):
    """A chain of `count` bones; with `turned` each is a quarter turn from its parent and its parent's far end (as Gun Girl's
    arm and eye bones are), the chain still returning every vertex to where it started because the animation is the rest."""
    return [('bone%d' % i, None if i == 0 else i - 1, (0, 0, 0)) for i in range(count)]


def puppet_layers(name, title, scene, cells, tex_w, tex_h, img_w, img_h, layer_w, layer_h, inset=(0, 0), mask=None, shading=False, bones=1, turned=False):
    """cells: [(x, y, tinted)]; every layer is layer_w x layer_h over the image img_w x img_h of a tex_w x tex_h texture."""
    w = Wallpaper(name, title, clear='0 0 0', size=scene)
    padded_texture(w, 'picture', tex_w, tex_h, img_w, img_h)
    # the skinned quad covers the layer less `inset` on each side (Gun Girl's mesh stops 41 and 26 pixels short of her
    # layer's edges); the picture's pixel at x, y of the layer is the texture's at x + w/2, h/2 - y
    hw, hh = layer_w / 2, layer_h / 2
    il, ir, it, ib = inset if len(inset) == 4 else (inset[0], inset[0], inset[1], inset[1])  # left, right, top, bottom
    def vertex(x, y):
        return (x, y, 0, (x + hw) / tex_w, (hh - y) / tex_h, [0, 0, 0, 0], [1, 0, 0, 0])
    vertices = [vertex(-hw + il, -hh + ib), vertex(hw - ir, -hh + ib), vertex(hw - ir, hh - it), vertex(-hw + il, hh - it)]
    if mask:
        tex_file(w, 'masks/white_mask', mask[0], mask[1], bytes([255]) * (mask[0] * mask[1]), binary.R8)
    write_binary(os.path.join(name, 'models/picture_puppet.mdl'),
                 puppet_mdl('materials/picture.json', vertices, [(0, 1, 2), (0, 2, 3)], bone_list(bones, turned),
                            [('rest', 30.0, [[((0, 0, 0), (0, 0, 0), (1, 1, 1))] * 2 for _ in range(bones)])],
                            shading=shading))
    w.files['models/picture.json'] = {"autosize": True, "material": "materials/picture.json",
                                      "puppet": "models/picture_puppet.mdl"}
    w.files['materials/picture.json'] = {"passes": [{
        "blending": "translucent", "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
        "shader": "genericimage3", "combos": {"VERSION": 2}, "textures": ["picture"]}]}
    for i, cell in enumerate(cells):
        x, y, tinted, *rest = cell
        extra = {}
        if rest and rest[0]:
            # an animation layer at blend 0: additive, normal or (hidden) invisible
            extra["animationlayers"] = [{"animation": 1, "blend": 0.0, "rate": 1.0, "additive": rest[0] != 'normal',
                                         "visible": rest[0] != 'hidden', "id": 500 + i, "name": "rest"}]
        if tinted:
            effect = shipped("tint", {"color": "0 0 0", "alpha": 0.25}, id=10 + i)
            if mask:
                effect['passes'][0]["textures"] = [None, "masks/white_mask"]
            extra["effects"] = [effect]
        w.image('%s %d' % ('tint' if tinted else 'plain', i), 'models/picture.json', x, y, layer_w, layer_h, **extra)
    w.save()
    copy_shipped_effects(w, ['tint'])
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))


# Gun Girl's sizes: 3800x3000 in a 3840x3008 texture, a plain layer beside a tinted one
puppet_layers('effect-puppet-round-big', 'a 3800x3000 puppet layer, plain and tinted', (8000, 4500),
              [(2000, 2250, False), (6000, 2250, True)], 3840, 3008, 3800, 3000, 3800, 3000)
# about a thousand wide, the same padding: 1000x800 in 1024x832
puppet_layers('effect-puppet-round-small', 'a 1000x800 puppet layer, plain and tinted', (2400, 1350),
              [(600, 675, False), (1800, 675, True)], 1024, 832, 1000, 800, 1000, 800)
# a width that rounds to 16 only by 8 pixels, no padding: 1001x801 in a 1001x801 texture
puppet_layers('effect-puppet-round-exact', 'a 1001x801 puppet layer over an exact texture, plain and tinted', (2400, 1350),
              [(600, 675, False), (1800, 675, True)], 1001, 801, 1001, 801, 1001, 801)
# what else Gun Girl has: a mesh short of the layer's edges, and tints with masks half the layer's size
puppet_layers('effect-puppet-round-inset', 'a puppet mesh short of its layer edges, plain and tinted', (2400, 1350),
              [(600, 675, False), (1800, 675, True)], 1024, 832, 1000, 800, 1000, 800, inset=(12, 8))
puppet_layers('effect-puppet-round-mask', 'a tint with a half-size mask on a puppet layer, plain and tinted', (2400, 1350),
              [(600, 675, False), (1800, 675, True)], 1024, 832, 1000, 800, 1000, 800, mask=(500, 400))
puppet_layers('effect-puppet-round-both', 'inset mesh and masked tint on a puppet layer, plain and tinted', (2400, 1350),
              [(600, 675, False), (1800, 675, True)], 1024, 832, 1000, 800, 1000, 800, inset=(12, 8), mask=(500, 400))
# Gun Girl's scene: 2560x1440, the layer 3800x3000 (larger than the scene, centred), plain and tinted as two wallpapers
for kind, tinted in (('plain', False), ('tint', True)):
    puppet_layers('effect-puppet-round-over-' + kind, 'an oversize puppet layer, ' + kind, (2560, 1440),
                  [(1280, 720, tinted)], 3840, 3008, 3800, 3000, 3800, 3000, inset=(41, 26))
# a visible animation layer at blend 0 on an autosize puppet: Gun Girl's pictures differ in Wallpaper Engine with it
for name, size, scene, tex, img, inset in (
        ('small', (1000, 800), (2400, 1350), (1024, 832), (1000, 800), (12, 8)),
        ('big', (3800, 3000), (8000, 4500), (3840, 3008), (3800, 3000), (41, 26))):
    cx, cy = scene[0] / 4, scene[1] / 2
    puppet_layers('effect-puppet-layer-' + name, 'an autosize puppet with and without an animation layer at blend 0', scene,
                  [(cx, cy, False), (cx * 3, cy, False, 'additive')], tex[0], tex[1], img[0], img[1], size[0], size[1], inset=inset)
# the same with the mesh's normals and tangents (Gun Girl's mesh flags are 0xf: position, normal, tangent, texture coordinates)
puppet_layers('effect-puppet-layer-shaded', 'a shaded autosize puppet with and without an animation layer at blend 0', (2400, 1350),
              [(600, 675, False), (1800, 675, False, 'additive')], 1024, 832, 1000, 800, 1000, 800, inset=(12, 8), shading=True)
# Gun Girl's mesh: 41 px short of the layer's left edge, 35 of the right, 26 of the top and 36 of the bottom
puppet_layers('effect-puppet-layer-asym', 'an asymmetric inset puppet with and without an animation layer at blend 0', (8000, 4500),
              [(2000, 2250, False), (6000, 2250, False, 'additive')], 3840, 3008, 3800, 3000, 3800, 3000, inset=(41, 35, 26, 36), shading=True)
# many bones: Gun Girl has 54
puppet_layers('effect-puppet-layer-bones', 'a 54-bone chain puppet with and without an animation layer at blend 0', (2400, 1350),
              [(600, 675, False), (1800, 675, False, 'additive')], 1024, 832, 1000, 800, 1000, 800, inset=(12, 8), bones=54)
