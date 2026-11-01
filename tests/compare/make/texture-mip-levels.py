# texture-mip-levels: a texture whose mipmaps are each a colour of their own (red, green, blue, then white), drawn by
# plain image layers at sizes that put it on the screen at its full size, half, between half and a quarter, and a
# quarter: which mipmaps does Wallpaper Engine sample? (Eagle flag's cloth, tiled four times, is sharper there.)
colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]
levels, size = [], 256
for level in range(9):
    r, g, b = colours[min(level, 3)]
    levels.append((size, size, bytes((r, g, b, 255)) * (size * size)))
    size //= 2
w = Wallpaper('texture-mip-levels', 'a texture with a colour for each mipmap', clear='0.1 0.1 0.1')
write_binary(os.path.join(w.name, 'materials', 'mips.tex'), tex_mips(levels))
model = w.texture_model('mips', 'mips', blending='normal')
# the 1920-wide scene is captured 1600 wide: a layer at scale s shows 256 * s * 5 / 6 pixels across
for x, screen in ((300, 256), (800, 128), (1250, 90), (1650, 64)):
    w.image('at %d' % screen, model, x, 540, 256, 256, scale='%g %g 1' % (screen * 6 / 5 / 256, screen * 6 / 5 / 256))
w.save()
