# script-user-properties: a script that reads two user properties in its applyUserProperties, as the Outer Wilds sun's
# supernova script does (a loop switch and a duration). Wallpaper Engine calls it once with both when the scene
# starts: the first square is then scaled 2, the second (whose script needs the switch AND a duration over 10) 3.
# owe told scripts one property at a time at start, so each script missed the other property and left them at 1.
w = Wallpaper('script-user-properties', 'a script reading two user properties', clear='0.1 0.1 0.15')
w.properties = {
    "loop": {"type": "bool", "value": True, "text": "Loop", "order": 0},
    "duration": {"type": "slider", "value": 20, "min": 1, "max": 100, "text": "Duration", "order": 1},
}
SCRIPT = """'use strict';
let ready = false;
export function applyUserProperties(p) {
    ready = %s;
}
export function update(value) {
    value.x = ready ? %s : 1;
    value.y = ready ? %s : 1;
    return value;
}
"""
for i, (cond, size) in enumerate((("p.loop === true", 2), ("p.loop === true && p.duration > 10", 3))):
    script = SCRIPT % (cond, size, size)
    w.add({"name": "square %d" % i, "image": "models/util/solidlayer.json", "origin": "%d 540 0" % (500 + i * 700),
           "size": "200 200", "color": "1 0.6 0.2", "alpha": 1, "angles": "0 0 0",
           "scale": {"value": "1 1 1", "script": script, "scriptproperties": {}}, "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'script-user-properties', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-user-properties'))
