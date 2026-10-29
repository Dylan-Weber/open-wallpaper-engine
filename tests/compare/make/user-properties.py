# 24. user-properties: values that come from the wallpaper's properties (at their defaults)
w = Wallpaper('user-properties', 'values bound to user properties', clear='0.1 0.1 0.1')
w.properties = {
    "schemecolor": {"order": 0, "text": "Tint", "type": "color", "value": "0.2 0.8 0.4"},
    "opacity": {"order": 1, "text": "Opacity", "type": "slider", "value": 0.4, "min": 0, "max": 1, "step": 0.05},
    "show": {"order": 2, "text": "Show the red square", "type": "bool", "value": False},
    "shown": {"order": 3, "text": "Show the blue square", "type": "bool", "value": True},
    "mode": {"order": 4, "text": "Mode", "type": "combo", "value": "2",
             "options": [{"label": "One", "value": "1"}, {"label": "Two", "value": "2"}]},
    "size": {"order": 5, "text": "Width", "type": "slider", "value": 300, "min": 50, "max": 600, "step": 10},
    "label": {"order": 6, "text": "Label", "type": "textinput", "value": "from a property"},
}
w.solid('tinted', 400, 800, 300, 200, {"user": "schemecolor", "value": "1 1 1"})
w.solid('transparent', 900, 800, 300, 200, '1 1 1', alpha={"user": "opacity", "value": 1.0})
w.solid('hidden red', 1400, 800, 300, 200, '1 0 0', visible={"user": "show", "value": True})
w.solid('shown blue', 400, 500, 300, 200, '0 0 1', visible={"user": "shown", "value": False})
w.solid('mode one', 900, 500, 200, 200, '1 1 0', visible={"user": {"name": "mode", "condition": "1"}, "value": False})
w.solid('mode two', 1400, 500, 200, 200, '0 1 1', visible={"user": {"name": "mode", "condition": "2"}, "value": False})
w.add({"name": "label", "text": {"user": "label", "value": "placeholder"}, "font": "systemfont_arial",
       "pointsize": 64, "size": "700 150", "origin": "1100 200 0", "color": "1 1 1", "alpha": 1, "visible": True,
       "scale": "1 1 1", "angles": "0 0 0", "horizontalalign": "center", "verticalalign": "center", "padding": 10})
w.save()
