# preset-thunderbolt, preset-dripping-water: two of Wallpaper Engine's shipped particle presets, inserted as its editor
# inserts them: the preset's particle and material files copied into the wallpaper (assets/presets/<group>/), and a
# particle layer with the variant's instanceoverride. Black background, no bloom. Both are random, so each is taken
# as a series of pictures and compared statistically; the -f15 copies run at 15 fps (two operator passes a frame).
# thunderbolt: left, the preset as inserted (control point 1 is worldspace at its offset "0 -450 0"); right, control
# point 1 dragged in the editor (instanceoverride controlpoint1, a world position).
# dripping_water: left at scale 1 as inserted; right at scale 3, to see the drips, splash and droplets.
import shutil

PRESETS = os.path.join(os.environ.get('WPE_ASSETS', os.path.expanduser(
    '~/snap/steam/common/.local/share/Steam/steamapps/common/wallpaper_engine/assets')), 'presets')


def copy_preset(name, group, files):
    for path in files:
        target = os.path.join(HERE, name, path)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copyfile(os.path.join(PRESETS, group, path), target)


def layer(name, particle, x, y, scale, override):
    return {"name": name, "particle": particle, "origin": vec(x, y, 0), "scale": vec(scale, scale, scale),
            "angles": "0 0 0", "instanceoverride": override}


def preset_wallpaper(name, title, group, files, layers):
    for fps in (30, 15):
        wname = name if fps == 30 else name + '-f15'
        w = Wallpaper(wname, title, clear='0 0 0')
        for l in layers:
            w.add(l)
        w.save()
        copy_preset(wname, group, files)
        clock_probe.stamp(os.path.join(HERE, wname))
        path = os.path.join(HERE, wname, 'compare.json')
        settings = json.load(open(path)) if os.path.exists(path) else {}
        settings.update({"probe": True, "fps": fps, "shots": 30, "shot_interval_ms": 200})
        write(wname + '/compare.json', settings)


thunderbolt = ['thunderbolt', 'thunderbolt_child_spawner', 'thunderbolt_beam_child', 'thunderbolt_glow']
preset_wallpaper(
    'preset-thunderbolt', 'shipped preset lightning thunderbolt', 'lightning',
    ['particles/presets/%s.json' % n for n in thunderbolt] + ['materials/presets/%s.json' % n for n in thunderbolt],
    [layer("Thunderbolt", "particles/presets/thunderbolt.json", 480, 900, 1, {"brightness": 5.0}),
     layer("Thunderbolt dragged", "particles/presets/thunderbolt.json", 1250, 950, 1,
           {"brightness": 5.0, "controlpoint1": "1550 150 0"})])

water = ['dripping_water', 'dripping_water_refract', 'dripping_water_splash', 'dripping_water_droplets']
water_override = {"controlpoint1": "22 0 0", "controlpoint2": "-22 0 0",
                  "controlpointangle1": "0 0 -0.5236", "controlpointangle2": "0 0 0.5236"}
preset_wallpaper(
    'preset-dripping-water', 'shipped preset water dripping_water', 'water',
    ['particles/presets/%s.json' % n for n in water] + ['materials/presets/%s.json' % n for n in water],
    [layer("Dripping water", "particles/presets/dripping_water.json", 480, 1000, 1, dict(water_override)),
     layer("Dripping water x3", "particles/presets/dripping_water.json", 1300, 1000, 3, dict(water_override))])
