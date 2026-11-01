# texture-minify-nomip: Wallpaper Engine's noise texture (util/noise, one mipmap, 256 pixels) tiled by a material's
# texture coordinates is minified without mipmaps; here the texture is drawn by plain image layers at screen sizes of
# 256, 128, 64 and 29 pixels (what film grain's 10x tiling on a 282 pixel picture asks of it), and the same for
# clouds_256 (7 mipmaps) in the row below: does Wallpaper Engine's filtering blur a texture without mipmaps?
w = Wallpaper('texture-minify-nomip', 'minifying a texture with one mipmap and one with seven', clear='0.1 0.1 0.1')
for row, (texture, y) in enumerate((('util/noise', 760), ('util/clouds_256', 300))):
    model = w.texture_model('t%d' % row, texture, blending='normal')
    # the 1920-wide scene is captured 1600 wide: a layer at scale s shows 256 * s * 5 / 6 pixels across
    for x, screen in ((300, 256), (800, 128), (1250, 64), (1650, 29)):
        w.image('%s at %d' % (texture, screen), model, x, y, 256, 256,
                scale='%g %g 1' % (screen * 6 / 5 / 256, screen * 6 / 5 / 256))
w.save()
with open(os.path.join(HERE, 'texture-minify-nomip', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'texture-minify-nomip'))
