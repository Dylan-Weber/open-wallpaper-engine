# texture-anisotropy: a texture whose mipmaps are each a colour of their own (red, green, blue, yellow, then white),
# drawn by plain image layers at its full width and squeezed 2, 4 and 7 times in height. Wallpaper Engine filters
# anisotropically, 8 samples at most, so it keeps sampling the full-size mipmap (red) for all three (squeezed 12.8
# times it takes the next one partly); without anisotropic filtering they show the second, third and fourth
# mipmaps. Eagle flag's cloth is squeezed 2 times.
colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 255, 255)]
levels, size = [], 256
for level in range(9):
    r, g, b = colours[min(level, 4)]
    levels.append((size, size, bytes((r, g, b, 255)) * (size * size)))
    size //= 2
w = Wallpaper('texture-anisotropy', 'a texture with a colour for each mipmap, squeezed', clear='0.1 0.1 0.1')
write_binary(os.path.join(w.name, 'materials', 'mips.tex'), tex_mips(levels))
model = w.texture_model('mips', 'mips', blending='normal')
# the 1920-wide scene is captured 1600 wide: scale 6/5 shows the texture 256 pixels wide, 6/5/squeeze that high
for x, squeeze in ((420, 2), (960, 4), (1500, 7)):
    w.image('squeezed %d' % squeeze, model, x, 540, 256, 256, scale='1.2 %g 1' % (1.2 / squeeze))
w.save()
