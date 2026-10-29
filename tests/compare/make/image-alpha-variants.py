# 6. image-alpha-variants: how the opacity of a textured layer is set, over a white strip and over the dark scene.
# A layer's own alpha, brightness and colour do not reach genericimage2 (its alpha and brightness are the material
# settings "Alpha" and "Brightness"; lower case keys are ignored), unless the material has the VERSION combo: then the
# layer's colour and alpha (not its brightness) do
# ---------------------------------------------------------------------------------------------------------------
# More wallpapers, one feature each
# ---------------------------------------------------------------------------------------------------------------

# 6. image-alpha-variants: how the opacity of a textured layer is set, over a white strip and over the dark scene.
# A layer's own alpha, brightness and colour do not reach genericimage2 (its alpha and brightness are the material
# settings "Alpha" and "Brightness"; lower case keys are ignored), unless the material has the VERSION combo: then the
# layer's colour and alpha (not its brightness) do
w = Wallpaper('image-alpha-variants', 'opacity and brightness of textured layers', clear='0.2 0.2 0.2')
plain = w.texture_model('plain', 'gradient/gradient_rainbow')
const_alpha = w.texture_model('const-alpha', 'gradient/gradient_rainbow', constants={"alpha": 0.5})
const_bright = w.texture_model('const-bright', 'gradient/gradient_rainbow', constants={"brightness": 0.5})
setting_alpha = w.texture_model('setting-alpha', 'gradient/gradient_rainbow', constants={"Alpha": 0.5})
setting_bright = w.texture_model('setting-bright', 'gradient/gradient_rainbow', constants={"Brightness": 0.5})
version = w.texture_model('version', 'gradient/gradient_rainbow', combos={"VERSION": 1})
w.solid('white strip', 960, 700, 1800, 220, '1 1 1')
for i, (model, extra, label) in enumerate([
        (plain, {}, 'plain'), (plain, {"alpha": 0.5}, 'layer alpha'), (const_alpha, {}, 'lower case alpha'),
        (const_bright, {}, 'lower case brightness'), (plain, {"brightness": 0.5}, 'layer brightness'),
        (plain, {"color": "1 0.5 0.5"}, 'layer colour'), (setting_alpha, {}, 'material Alpha'),
        (setting_bright, {}, 'material Brightness'), (version, {"alpha": 0.5}, 'version alpha'),
        (version, {"color": "1 0.5 0.5"}, 'version colour'), (version, {"brightness": 0.5}, 'version brightness')]):
    w.image(label + ' on white', model, 130 + i * 166, 700, 64, 2, scale='2.4 80 1', **extra)
    w.image(label + ' on dark', model, 130 + i * 166, 400, 64, 2, scale='2.4 80 1', **extra)
w.save()
