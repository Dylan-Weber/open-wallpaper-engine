# 15. script-layers: scripts that run once and decide what is drawn
w = Wallpaper('script-layers', 'scripts that place and create layers', clear='0.05 0.05 0.1')
script = """'use strict';
engine.registerAsset('models/util/solidlayer.json', true);

export function init(value) {
    // layers made by the script: a row of squares that grows, coloured by their number
    for (let i = 0; i < 6; i++) {
        thisScene.createLayer({
            image: 'models/util/solidlayer.json',
            name: 'made ' + i,
            origin: new Vec3(300 + i * 150, 900, 0),
            size: new Vec2(100, 40 + i * 25),
            color: new Vec3(i / 5, 0.5, 1 - i / 5),
        });
    }
    // the canvas size places a marker in the middle
    const centre = new Vec3(engine.canvasSize.x / 2, engine.canvasSize.y / 2, 0);
    thisScene.createLayer({image: 'models/util/solidlayer.json', name: 'centre', origin: centre,
                           size: new Vec2(60, 60), color: new Vec3(1, 1, 1)});
    // localStorage keeps a value between scripts of this run
    localStorage.set('x', 1500);
    thisScene.getLayer('moved by script').origin = new Vec3(localStorage.get('x'), 540, 0);
    // layers can be read and counted
    const count = thisScene.getLayerCount();
    thisScene.getLayer('counted').size = new Vec2(40 * count, 60);
    return value;
}
export function update(value) { return value; }
"""
w.add({"name": "driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.solid('moved by script', 100, 200, 120, 120, '1 0.5 0')
w.solid('counted', 700, 200, 10, 60, '0.5 1 0.5')
w.save()
