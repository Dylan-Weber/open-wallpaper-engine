# script-return-type: property scripts that return a number for a vector property (scale, origin, angles), the way OoT's
# interface root's scale script does (`value.x -= 0.022; value = value.x; return value;` while zooming): owe takes the
# number as a uniform vector and shrank the panel to 0.4, Wallpaper Engine's panel kept its size. Also a script that
# shrinks scale.x step by step but returns the vector, and one that returns a string, for comparison.
w = Wallpaper('script-return-type', 'vector property scripts that return a number', clear='0.05 0.05 0.1')
shrink_to_number = """'use strict';
export function update(value) {
    if (value.x > 0.4) {
        value.x -= 0.022;
        if (value.x < 0.4) value.x = 0.4;
        value = value.x;
    }
    return value;
}
"""
number = """'use strict';
export function update(value) {
    return 0.5;
}
"""
shrink_vector = """'use strict';
export function update(value) {
    if (value.x > 0.4) {
        value.x -= 0.022;
        if (value.x < 0.4) value.x = 0.4;
    }
    return value;
}
"""
text = """'use strict';
export function update(value) {
    return '0.5 0.5 1';
}
"""
origin_number = """'use strict';
export function update(value) {
    return 300;
}
"""
angles_number = """'use strict';
export function update(value) {
    return 30;
}
"""
cases = (('scale shrinks, returns a number', 250, 700, 'scale', shrink_to_number),
         ('scale returns 0.5', 650, 700, 'scale', number),
         ('scale shrinks x, returns the vector', 1050, 700, 'scale', shrink_vector),
         ('scale returns a string', 1450, 700, 'scale', text),
         ('origin returns 300', 1750, 300, 'origin', origin_number),
         ('angles returns 30', 1750, 750, 'angles', angles_number))
for name, x, y, prop, script in cases:
    layer = {"name": name, "image": "models/util/solidlayer.json", "size": "200 200", "color": "1 0.6 0.2", "alpha": 1,
             "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
             "visible": True}
    layer[prop] = {"value": layer[prop], "script": script, "scriptproperties": {}}
    w.add(layer)
w.save()
with open(os.path.join(HERE, 'script-return-type', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-return-type'))
