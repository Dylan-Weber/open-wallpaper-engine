# flag-mip-levels: Wallpaper Engine's own flag shader (Eagle flag's) over a cloth whose mipmaps each have a brightness
# of their own (1, 0.6, 0.3, then 0.1), with a flat normal map: which mipmaps of the cloth (drawn four times across
# the layer) does it sample, at three sizes on the screen? Plain image layers sample them as owe does
# (texture-mip-levels); Eagle flag's cloth is sharper in Wallpaper Engine.
reds = [255, 153, 77, 26]
levels, size = [], 256
for level in range(9):
    levels.append((size, size, bytes((reds[min(level, 3)], 0, 0, 255)) * (size * size)))
    size //= 2
w = Wallpaper('flag-mip-levels', 'the flag shader over a cloth with a brightness for each mipmap', clear='0.1 0.1 0.1')
write_binary(os.path.join(w.name, 'materials', 'cloth.tex'), tex_mips(levels))
tex_file(w, 'albedo', 4, 4, bytes((0, 0, 255, 255)) * 16)
tex_file(w, 'normal', 4, 4, bytes((128, 128, 255, 128)) * 16)
w.files['models/flag.json'] = {"material": "materials/flag.json", "width": 1024, "height": 512}
w.files['materials/flag.json'] = {"passes": [{
    "shader": "flag", "combos": {"TINT": 1}, "depthtest": "disabled", "depthwrite": "disabled",
    "constantshadervalues": {"Strength": 0.0, "Speed": 0.0, "color1": "1 1 1", "color2": "0 0 0", "color3": "0 0 0"},
    "textures": ["albedo", "normal", "cloth"]}]}
# the cloth is 1024 texels across the 1024-wide layer; the 1920-wide scene is captured 1600 wide (5/6)
for y, texel in ((200, 1.0), (560, 2.0), (880, 4.0)):
    scale = 6 / 5 / texel
    w.image('cloth per %g pixels' % texel, 'models/flag.json', 960, y, 1024, 512, scale='%g %g 1' % (scale, scale))
w.save()
