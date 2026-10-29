# 44. model-normal-rg88: a wall whose normal map is RG88 (LittleBigPlanet's clouds have one): 16 stripes leaning left
# and right in turn, lit from the left, so every other stripe is lit. Wallpaper Engine decodes an RG88 normal map from
# .rg when the shader's TEX1FORMAT says RG88 (owe #63); read as .wy, every normal leans the same way.
w = Scene3D('model-normal-rg88', 'an RG88 normal map lit from the side', ambient='0.1 0.1 0.1')
white = tex_file(w, 'test/white', 4, 4, bytes([255, 255, 255, 255] * 16))
stripes = []
for y in range(16):
    for x in range(16):
        stripes += [230, 128] if x % 2 == 0 else [26, 128]
normals = tex_file(w, 'test/stripes_n', 16, 16, bytes(stripes), binary.RG88)
w.cube('wall', textured_material(w, 'striped', [white, normals]), 0, 0, -1, (0, 0, 0), '6 3.4 0.2')
w.light('left', 'lpoint', -9, 0, 1, '1 1 1', 5, 40)
w.save()
