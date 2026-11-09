# preset-*: a Workshop preset (a project.json with no type, naming the item it is made from in "dependency" and the
# values it sets in "preset") over a scene whose layers follow user properties. Wallpaper Engine plays the parent with
# the preset's values; preset-parent alone shows the parent's own values.
w = Wallpaper('preset-parent', 'layers bound to user properties (a preset sets them)', clear='0.1 0.1 0.15')
w.properties = {
    "boxcolor": {"type": "color", "value": "0.2 0.4 1", "text": "box colour", "order": 0},
    "boxalpha": {"type": "slider", "value": 1, "min": 0, "max": 1, "fraction": True, "text": "box alpha", "order": 1},
    "showsecond": {"type": "bool", "value": False, "text": "second box", "order": 2},
}
w.solid('box', 600, 540, 500, 500, color={"user": "boxcolor", "value": "0.2 0.4 1"},
        alpha={"user": "boxalpha", "value": 1})
w.solid('second', 1320, 540, 500, 500, color='1 0.8 0.1', visible={"user": "showsecond", "value": False})
w.save()

# the preset sits beside its parent, as Workshop items do (wallpapers/preset-parent and wallpapers/preset-values)
write('preset-values/project.json', {
    "title": "wpe-test a preset: red, half alpha, second box shown", "dependency": "preset-parent",
    "preset": {"boxcolor": "1 0 0", "boxalpha": 0.5, "showsecond": True, "_d0": None}})
