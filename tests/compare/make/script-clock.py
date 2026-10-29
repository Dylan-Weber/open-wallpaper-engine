# 51. script-clock: bars as long as the hour, the minute and the second the scripts' Date shows (owe #59). Stamped with
# the clock probe, which records the date and time zone for owe_compare to give the viewer.
w = Wallpaper('script-clock', 'bars as long as the hour, minute and second of the scripts\' Date')
for row, (part, unit) in enumerate((('getHours', 24), ('getMinutes', 60), ('getSeconds', 60))):
    script = """'use strict';
export function update(value) {
	value.x = new Date().%s() / %d;
	return value;
}
""" % (part, unit)
    w.solid('%s frame' % part, 960, 760 - row * 220, 1604, 124, '0.25 0.25 0.25')
    w.solid(part, 960, 760 - row * 220, 1600, 120, ('1 0.4 0.3', '0.3 1 0.4', '0.3 0.5 1')[row],
            scale={"value": "1 1 1", "script": script, "scriptproperties": {}})
w.save()
clock_probe.stamp(os.path.join(HERE, 'script-clock'))
