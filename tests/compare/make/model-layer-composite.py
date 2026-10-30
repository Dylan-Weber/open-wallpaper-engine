# model-layer-composite: a model whose texture is another layer's composite (_rt_imageLayerComposite_<id>_a, with the
# layer in its "dependencies"), as More Physics' skybox takes its colours from a gradient layer. The source layer is a
# 128 x 128 blue solid; scale 0 0 0 (never seen on its own, as the sky's), or scale 1 (seen at the corner). Two cubes
# for each: the first texture-less red one is a control.
for name, scale in (('model-layer-composite', '0 0 0'), ('model-layer-composite-seen', '1 1 1')):
    w = Scene3D(name, 'a model textured by a layer composite, source scale ' + scale, ambient='1 1 1')
    src = w.add({"name": "source", "image": "models/util/solidlayer.json", "origin": vec(-8, 4, 0),
                 "size": "128 128", "color": "0.2 0.6 1", "alpha": 1, "scale": scale, "angles": vec(0, 0, 0)})
    w.files['materials/composite.json'] = {"passes": [{
        "shader": "generic4", "blending": "normal", "cullmode": "normal", "depthtest": "enabled",
        "depthwrite": "enabled", "combos": {"LIGHTING": 0, "REFLECTION": 0, "RIMLIGHTING": 0, "SHADINGGRADIENT": 0},
        "constantshadervalues": {"color": "1 1 1", "alpha": 1}, "textures": ["_rt_imageLayerComposite_%d_a" % src]}]}
    write_binary(os.path.join(name, 'models/composite.mdl'), cube_mdl('materials/composite.json'))
    red = w.material('red', '1 0 0')
    cube = w.cube('textured', 'models/composite.mdl', 1, 0, 0, (0.3, 0.5, 0), '1.5 1.5 1.5')
    for o in w.objects:
        if o['name'] == 'textured':
            o['dependencies'] = [src]
    w.cube('control', red, -2, 0, 0, (0.3, 0.5, 0), '1 1 1')
    w.save()
