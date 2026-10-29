# 31b. script-value-copy: whether a property script's value is its own copy in each call. Each square starts at
# x = 300 and its script tries to move it 600 to the right in a different way; Puppet Simulation Preview (3227072870)
# keeps the value its init got and returns it later as the place to go back to
w = Wallpaper('script-value-copy', "what a script's value is in each call", clear='0 0 0')
value_scripts = {
    # init keeps its value; update moves the value it got and gives back the one init kept
    'kept from init': """'use strict';
let kept;
export function init(value) { kept = value; return value; }
export function update(value) { value.x = 900; return kept; }
""",
    # update moves its value and gives nothing back
    'moved, nothing returned': """'use strict';
export function update(value) { value.x = 900; }
""",
    # init moves its value and gives nothing back
    'moved in init': """'use strict';
export function init(value) { value.x = 900; }
export function update(value) { return value; }
""",
    # the usual way: update moves its value and gives it back
    'moved and returned': """'use strict';
export function update(value) { value.x = 900; return value; }
""",
}
for row, (name, script) in enumerate(value_scripts.items()):
    y = 200 + row * 220
    w.solid(name + ' start', 300, y, 12, 160, '0.3 0.3 0.3')
    w.solid(name + ' goal', 900, y, 12, 160, '0.3 0.3 0.3')
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "100 100", "color": "1 0.6 0.1", "alpha": 1,
           "origin": {"value": "300 %d 0" % y, "script": script, "scriptproperties": {}},
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})
w.save()
