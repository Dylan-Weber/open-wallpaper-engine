# texture-animation-synced: how a sprite animation starts when a hidden layer is shown, or when a script clones a hidden
# template layer (getInitialLayerConfig + createLayer, as More Physics spawns its spheres), at 2.1, 2.3 and 2.5 s. The rose
# petal sheet (5 frames of 0.2 s) with a tint effect on every layer; one layer is there from the start as the control.
# -shown: three layers hidden until the script shows them. -cloned: three hidden templates a script clones (visible, same
# place and effect). If the animation starts when a layer appears, both engines show other frames in each; if it runs from
# the scene's start, the same one. The synced-wallpaper transform (sync_wallpaper.py) relies on the cloned form.
for name, mode in (('texture-animation-shown', 'show'), ('texture-animation-cloned', 'clone')):
    w = Wallpaper(name, 'sprite phase of layers ' + ('shown' if mode == 'show' else 'cloned') + ' mid-round', clear='0.1 0.1 0.2')
    petals = w.texture_model('petals', 'particle/nature/rosepetals', model_extra={"autosize": True})
    copy_shipped_effects(w, ['tint'])

    def layer(label, x, visible):
        effect = dict(shipped('tint', {"color": "1 0.7 0.9", "alpha": 0.5}), id=900 + int(x))
        effect['passes'] = [dict(effect['passes'][0], id=1900 + int(x))]
        return w.add({"name": label, "image": petals, "size": "256 256", "color": "1 1 1", "alpha": 1, "angles": "0 0 0",
                      "scale": "1 1 1", "alignment": "center", "visible": visible, "origin": "%d 600 0" % x,
                      "effects": [effect]})

    layer('from the start', 250, True)
    for i in range(3):
        layer('later %d' % i, 650 + 400 * i, False)
    script = """'use strict';
const asset = engine.registerAsset('%s');
const times = [2.1, 2.3, 2.5];
let made = 0;
export function update(value) {
    while (made < times.length && engine.runtime >= times[made]) {
        const template = thisScene.getLayer('later ' + made);
        if (%s) {
            template.visible = true;
        } else {
            const config = thisScene.getInitialLayerConfig(template);
            config.name = 'clone ' + made;
            config.visible = true;
            thisScene.createLayer(config);
        }
        made++;
    }
    return value;
}
""" % (petals, 'true' if mode == 'show' else 'false')
    w.add({"name": "maker", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
           "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))
