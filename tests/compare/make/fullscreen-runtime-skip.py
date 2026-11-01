# fullscreen-runtime-skip: a fullscreen layer with a material of its own and one effect that a script turns off
# when the scene starts. A layer left with no effect is skipped only if it just passes the frame through; this one
# still draws its material (owe #39).
w = Wallpaper('fullscreen-runtime-skip', 'a fullscreen layer whose effects a script turns off', clear='0.05 0.05 0.1')
local_effect(w, 'redden', """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}

void main() {
	vec4 color = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = vec4(1.0, color.g * 0.2, color.b * 0.2, color.a);
}
""", {})
model = w.texture_model('sky', 'util/clouds_256', blending='normal', model_extra={"fullscreen": True})
script = """'use strict';

export function init() {
	thisLayer.getEffect('redden').visible = false;
}
"""
w.add({"name": "sky", "image": model, "origin": "960 540 0", "size": "1920 1080", "color": "0.4 0.6 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1",
       "visible": {"value": True, "script": script, "scriptproperties": {}},
       "effects": [use('redden', 700, {})]})
w.solid('marker', 960, 540, 300, 300, '1 0.5 0')
w.save()
