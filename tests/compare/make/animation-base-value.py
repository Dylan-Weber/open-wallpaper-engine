# 25. animation-base-value: what an animation does to a property that has a value of its own: the animation of a vector
# (origin, scale, colour, angles) is added to it, relative or not; the animation of a number (alpha) replaces it
w = Wallpaper('animation-base-value', 'animations on top of the value of the property', clear='0 0 0')
base_layer(w, 'origin', color='1 0 0', origin={"value": "300 800 0", "animation": constant_animation([100, 0, 0])})
base_layer(w, 'origin relative', color='0 1 0',
           origin={"value": "300 600 0", "animation": constant_animation([100, 0, 0], True)})
base_layer(w, 'scale', color='1 0 0', origin="800 800 0",
           scale={"value": "2 2 1", "animation": constant_animation([1, 1, 1])})
base_layer(w, 'scale relative', color='0 1 0', origin="800 600 0",
           scale={"value": "2 2 1", "animation": constant_animation([1, 1, 1], True)})
base_layer(w, 'alpha', origin="1300 800 0", alpha={"value": 0.5, "animation": constant_animation([0.25])})
base_layer(w, 'alpha relative', origin="1300 600 0", alpha={"value": 0.5, "animation": constant_animation([0.25], True)})
base_layer(w, 'colour', origin="300 300 0", color={"value": "0 0.5 0", "animation": constant_animation([0, 0, 0.5])})
base_layer(w, 'colour relative', origin="800 300 0",
           color={"value": "0 0.5 0", "animation": constant_animation([0, 0, 0.5], True)})
base_layer(w, 'angle', origin="1300 300 0", size="200 40",
           angles={"value": "0 0 0.3", "animation": constant_animation([0, 0, 0.2])})
base_layer(w, 'angle relative', origin="1600 300 0", size="200 40",
           angles={"value": "0 0 0.3", "animation": constant_animation([0, 0, 0.2], True)})
w.save()
