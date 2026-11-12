# script-callback-probe: which callbacks Wallpaper Engine calls on a text script by itself, in what order, with what
# arguments (resizeScreen, destroy, cursorHitTest, applyGeneralSettings, applyUserProperties, media events). The calls
# are printed as text lines; the picture is read by eye.
w = Wallpaper('script-callback-probe', 'text lines listing the callbacks a script gets', clear='0 0 0')
probe = """'use strict';
const log = [];
const show = (a) => { try { return JSON.stringify(Array.from(a)); } catch (e) { return String(e); } };
let n = 0;
const note = (name, a) => { if (log.length < 30) log.push((n++) + ' ' + name + ' ' + show(a)); };
export function init(value) { note('init', [typeof value]); return value; }
export function resizeScreen() { note('resizeScreen', arguments); }
export function destroy() { note('destroy', arguments); }
export function cursorHitTest() { note('cursorHitTest', arguments); }
export function applyGeneralSettings() { note('applyGeneralSettings', arguments); }
export function applyUserProperties() { note('applyUserProperties', arguments); }
export function mediaStatusChanged() { note('mediaStatusChanged', arguments); }
export function mediaTimelineChanged() { note('mediaTimelineChanged', arguments); }
export function mediaPlaybackChanged() { note('mediaPlaybackChanged', arguments); }
let updates = 0;
export function update(value) { if (updates++ < 1) note('update', []); return log.join('\\n'); }
"""
text_layer(w, 'probe', 20, 1060, {"value": "", "script": probe, "scriptproperties": {}},
           'left', 'top', size='1800 1000', padding=0, pointsize=9, opaquebackground=False)
w.save()
