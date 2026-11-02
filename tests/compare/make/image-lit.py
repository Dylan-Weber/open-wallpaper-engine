# 19i. image-lit: a lit image layer (genericimage2 with LIGHTING) under a point light 300 units above its middle. The
# shader divides the light's colour by the distance squared; the colour is the light's times its intensity and its
# radius squared (found with experiments in the real engine)
w = Wallpaper('image-lit', 'a lit image layer', clear='0 0 0')
w.files['models/lit.json'] = {"material": "materials/lit.json"}
w.files['materials/lit.json'] = {"passes": [{
    "shader": "genericimage2", "blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
    "depthwrite": "disabled", "combos": {"LIGHTING": 1}, "constantshadervalues": {"roughness": 1.0, "metallic": 0.0},
    "textures": ["util/white"]}]}
w.image('lit', 'models/lit.json', WIDTH / 2, HEIGHT / 2, WIDTH, HEIGHT)
w.add({"name": "light", "light": "point", "origin": vec(WIDTH / 2 + 200, HEIGHT / 2, 300), "color": "1 0.8 0.6",
       "intensity": 1, "radius": 600, "angles": "0 0 0", "scale": "1 1 1"})
w.save()
