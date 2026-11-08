# 47. model-hidden-group: a hidden group layer holding a model that wears a layer of the group as its texture (Ocarina
# of Time's Mido and Malon wear their eye and mouth layers, in groups that start hidden). The model isn't drawn; the
# layer it wears still is, for it (owe: a layer below a hidden layer isn't drawn, even when one further below is
# linked). A shown cube beside it, for comparison.
w = Scene3D('model-hidden-group', 'a model in a hidden group, wearing a layer of the group')
group = w.add({"name": "set", "origin": "0 0 0", "angles": "0 0 0", "scale": "1 1 1", "visible": False})
face = w.add({"name": "face", "image": "models/util/solidlayer.json", "origin": "0 0 0", "size": "64 64",
              "color": "1 0.8 0.2", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
              "parent": group})
worn = textured_material(w, 'worn', ['_rt_imageLayerComposite_%d_a' % face], combos={"LIGHTING": 0})
w.add({"name": "character", "model": worn, "origin": "-2 0 0", "angles": "0.3 0.6 0", "scale": "1 1 1",
       "parent": group})
w.cube('shown', w.material('shown', '0.3 0.6 1'), 2.5, 0, 0, (0.3, 0.6, 0))
w.save()
