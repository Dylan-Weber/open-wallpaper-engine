# 23. texture-animation: a sprite sheet (5 frames) whose animation a script stops on a chosen frame
w = Wallpaper('texture-animation', 'sprite sheet frames chosen by a script', clear='0.1 0.1 0.2')
petals = w.texture_model('petals', 'particle/nature/rosepetals')
for frame in range(5):
    script = """'use strict';
export function init(value) {
    const animation = thisLayer.getTextureAnimation();
    if (animation !== undefined) {
        animation.pause();
        animation.setFrame(%d);
    }
    return value;
}
export function update(value) { return value; }
""" % frame
    w.add({"name": "frame %d" % frame, "image": petals, "size": "205 256", "color": "1 1 1", "alpha": 1,
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
           "origin": {"value": vec(250 + frame * 350, 600, 0), "script": script, "scriptproperties": {}}})
w.save()
