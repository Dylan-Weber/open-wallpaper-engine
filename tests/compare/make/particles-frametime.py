# particles-frametime-f15/-f30/-f60: the drag-rest throws (drag 0.2, 0.5, 1, 2; D = 300, 600, 900) next to a script that
# prints Wallpaper Engine's first 400 frame times (engine.frametime, ms). Its drag compensation uses each frame's own time,
# dt2 = dt x min(0.025 / dt, 1)^0.7, so its stops depend on the frames it happened to render: replaying the printed times
# through that rule gives the stops of the same take (frametimes.txt next to each reference). owe steps exactly 1/fps.
def thrown(drag, distance):
    speed = drag / 2 * distance
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 50, "max": 50},
            {"id": 4, "name": "colorrandom", "min": "255 220 80", "max": "255 220 80"},
            {"id": 5, "name": "velocityrandom", "min": "%g 0 0" % speed, "max": "%g 0 0" % speed},
        ],
        "operator": [{"id": 20, "name": "movement", "drag": drag, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


recorder = """'use strict';
let times = [];
let frames = 0;
export function update(value) {
    frames++;
    if (times.length < 400) times.push(engine.frametime);
    let lines = [];
    for (let i = 0; i < times.length; i += 10) {
        lines.push(times.slice(i, i + 10).map(t => (t * 1000).toFixed(1)).join(' '));
    }
    return 'n ' + frames + ' t ' + engine.runtime.toFixed(3) + '\\n' + lines.join('\\n');
}
"""

for fps in (15, 30, 60):
    name = 'particles-frametime-f%d' % fps
    w = Wallpaper(name, 'frame times and particles at rest under drag, %d fps' % fps, clear='0 0 0')
    w.add({"name": "frametimes", "text": {"value": "x", "script": recorder, "scriptproperties": {}},
           "font": "systemfont_arial", "pointsize": 5, "size": "660 1070", "origin": "1290 1065 0", "color": "1 1 1",
           "alpha": 1, "scale": "1 1 1", "angles": "0 0 0", "horizontalalign": "left", "verticalalign": "top",
           "padding": 4, "opaquebackground": True, "backgroundcolor": "0 0 0"})
    for k, drag in enumerate((0.2, 0.5, 1, 2)):
        for j, distance in enumerate((300, 600, 900)):
            part = 'drag%d_%d' % (k, j)
            w.files['particles/%s.json' % part] = thrown(drag, distance)
            w.add({"name": part, "particle": "particles/%s.json" % part,
                   "origin": "250 %d 0" % (920 - 170 * k - 40 * j), "scale": "1 1 1", "angles": "0 0 0"})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": fps}, f)
    clock_probe.stamp(os.path.join(HERE, name))
