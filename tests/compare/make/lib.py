"""What the test wallpapers' scripts (make/*.py) are made with: layers, effects, 3D scenes of cubes, lights,
textures, and the clock probe. make-wallpapers.py runs each script with these names to hand."""
import importlib.util
import json
import math
import os
import re
import struct

# the wallpapers are written next to their reference images, in wallpapers/
HERE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'wallpapers')
MAKE = os.path.dirname(os.path.abspath(__file__))
WIDTH, HEIGHT = 1920, 1080

_ids = {}


_probe_spec = importlib.util.spec_from_file_location(
    'clock_probe', os.path.join(os.path.dirname(MAKE), 'clock-probe.py'))
clock_probe = importlib.util.module_from_spec(_probe_spec)
_probe_spec.loader.exec_module(clock_probe)


def write(path, content):
    full = os.path.join(HERE, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, 'w') as f:
        f.write(content if isinstance(content, str) else json.dumps(content, indent=2) + '\n')


def vec(*v):
    return ' '.join('%g' % x for x in v)


# scene.json stores angles in radians (as the files Wallpaper Engine writes do); the helpers below take degrees


class Wallpaper:
    def __init__(self, name, title, clear='0.1 0.1 0.15', size=None):
        self.name = name
        self.title = title
        self.clear = clear
        # the projection, when it is not the usual WIDTH x HEIGHT
        self.size = size or (WIDTH, HEIGHT)
        self.objects = []
        self.next_id = 1
        self.files = {}
        self.properties = {}

    def add(self, obj):
        obj = dict(obj)
        obj['id'] = self.next_id
        self.next_id += 1
        obj.setdefault('visible', True)
        self.objects.append(obj)
        return obj['id']

    def solid(self, name, x, y, w, h, color='1 1 1', alpha=1, angle=0, scale='1 1 1', **extra):
        """A flat colour rectangle centred on (x, y); y counts up from the bottom like in scene.json."""
        return self.add({
            "name": name, "image": "models/util/solidlayer.json", "origin": vec(x, y, 0), "size": vec(w, h),
            "color": color, "alpha": alpha, "angles": vec(0, 0, math.radians(angle)), "scale": scale, "alignment": "center",
            **extra})

    def group(self, name, x, y, angle=0, scale='1 1 1', **extra):
        return self.add({"name": name, "origin": vec(x, y, 0), "angles": vec(0, 0, math.radians(angle)), "scale": scale, **extra})

    def image(self, name, model, x, y, w, h, angle=0, scale='1 1 1', **extra):
        return self.add({
            "name": name, "image": model, "origin": vec(x, y, 0), "size": vec(w, h), "color": "1 1 1",
            "alpha": 1, "angles": vec(0, 0, math.radians(angle)), "scale": scale, "alignment": "center", **extra})

    def texture_model(self, model, texture, constants=None, blending="translucent", combos=None, model_extra=None):
        """A model and material that show a texture of the shipped assets, drawn flat."""
        self.files['models/%s.json' % model] = {"material": "materials/%s.json" % model, **(model_extra or {})}
        material = {"blending": blending, "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
                    "shader": "genericimage2", "textures": [texture]}
        if constants:
            material["constantshadervalues"] = constants
        if combos:
            material["combos"] = combos
        self.files['materials/%s.json' % model] = {"passes": [material]}
        return 'models/%s.json' % model

    def save(self):
        write(self.name + '/project.json', {
            "title": "wpe-test " + self.title, "type": "scene", "file": "scene.json", "version": 1,
            "general": {"properties": self.properties}})
        write(self.name + '/scene.json', {
            "camera": {"center": "0 0 0", "eye": "0 0 1", "up": "0 1 0"},
            "general": {
                "orthogonalprojection": {"width": self.size[0], "height": self.size[1]},
                "clearcolor": self.clear, "clearenabled": True, "cameraparallax": False, "camerashake": False,
                "bloom": False, "hdr": False, "zoom": 1.0,
                "ambientcolor": "0.3 0.3 0.3", "skylightcolor": "0.3 0.3 0.3"},
            "objects": self.objects, "version": 1})
        for path, content in self.files.items():
            write('%s/%s' % (self.name, path), content)

# 4. effect-mix: effects written for this test (a colour mix and an opacity scale), so the wallpaper needs nothing
# but the shader pipeline: constants from the scene, several effects on a layer, and effects in a chain
VERT = """uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec4 v_TexCoord;

void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_TexCoord = a_TexCoord.xyxy;
}
"""


def local_effect(w, name, frag, constants):
    """An effect with its own material and shader; `constants` are the uniforms its fragment shader declares."""
    w.files['effects/%s/effect.json' % name] = {
        "version": 1, "name": name, "passes": [{"material": "materials/effects/%s.json" % name}]}
    w.files['materials/effects/%s.json' % name] = {"passes": [{
        "shader": "effects/" + name, "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull"}]}
    w.files['shaders/effects/%s.vert' % name] = VERT
    w.files['shaders/effects/%s.frag' % name] = frag


def use(name, id, values):
    return {"file": "effects/%s/effect.json" % name, "id": id, "name": name, "visible": True,
            "passes": [{"id": id + 100, "constantshadervalues": values}]}

# 14. particles-static: particle systems whose particles do not move: placed by sequence initializers and fixed values
def particle_system(initializers, count, lifetime=100000, size=40, color='1 1 1', alpha=1.0, extra_operators=None,
                    controlpoints=None, distance='0 0 0', emitter='boxrandom'):  # sphere emitters take a number
    return {
        "material": "materials/particle/halo.json", "maxcount": max(count + 10, 20), "starttime": 0, "flags": 0,
        "controlpoint": controlpoints or [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": emitter, "distancemin": distance, "distancemax": distance,
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": count,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": lifetime, "max": lifetime},
            {"id": 3, "name": "sizerandom", "min": size, "max": size},
            {"id": 4, "name": "colorrandom", "min": color, "max": color},
            {"id": 5, "name": "alpharandom", "min": alpha, "max": alpha},
        ] + [dict(i, id=10 + n) for n, i in enumerate(initializers)],
        "operator": extra_operators or [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }

# 16. animation-final: timeline animations that have finished (single mode) by the time of the picture
def animation(channels, length=6, mode='single', relative=False):
    names = ['c0', 'c1', 'c2', 'c3']
    data = {}
    for n, keys in enumerate(channels):
        data[names[n]] = [{"frame": frame, "value": value,
                           "back": {"enabled": True, "x": -1, "y": 0}, "front": {"enabled": True, "x": 1, "y": 0}}
                          for frame, value in keys]
    data["options"] = {"fps": 30, "length": length, "mode": mode, "wraploop": None}
    data["relative"] = relative
    return data

# 17. text-layout: text boxes (the font differs between machines, so look at layout and not at glyph shapes)
def text_layer(w, name, x, y, text, horizontal, vertical, size='500 200', padding=20, pointsize=48, **extra):
    return w.add({"name": name, "text": text, "font": "systemfont_arial", "pointsize": pointsize, "size": size,
                  "origin": vec(x, y, 0), "color": "1 1 1", "alpha": 1, "visible": True, "scale": "1 1 1",
                  "angles": "0 0 0", "horizontalalign": horizontal, "verticalalign": vertical, "padding": padding,
                  "opaquebackground": True, "backgroundcolor": "0 0.2 0.5", **extra})


# ---------------------------------------------------------------------------------------------------------------
# Effects that ship with Wallpaper Engine, copied from its assets into the wallpaper when the generator runs (not
# committed); only effects that do not depend on time, noise or the pointer
# ---------------------------------------------------------------------------------------------------------------
SHIPPED_PASSES = {'tint': 1, 'opacity': 1, 'colorkey': 1, 'blur': 4, 'blurprecise': 2, 'chromaticaberration': 1,
                  'fisheye': 1, 'twirl': 1, 'skew': 1, 'transform': 1, 'perspective': 1, 'edgedetection': 1,
                  'localcontrast': 4, 'blend': 1, 'blendgradient': 1}


def copy_shipped_effects(wallpaper, names):
    """Copies the files of Wallpaper Engine's own effects into the wallpaper, the way its editor saves them (the real
    engine ignores an effect whose files are not in the wallpaper). The files are Wallpaper Engine's and are not
    committed: they come from the local Wallpaper Engine installation (WPE_ASSETS, or tests/assets), and the
    wallpaper's .gitignore leaves them out."""
    import shutil
    assets = os.environ.get('WPE_ASSETS') or os.path.join(os.path.dirname(os.path.dirname(MAKE)), 'assets')
    if not os.path.isdir(os.path.join(assets, 'effects')):
        print('  (no Wallpaper Engine assets at %s: %s keeps no effect files)' % (assets, wallpaper.name))
        return
    root = os.path.join(HERE, wallpaper.name)
    for name in names:
        source = os.path.join(assets, 'effects', name)
        os.makedirs(os.path.join(root, 'effects', name), exist_ok=True)
        shutil.copy(os.path.join(source, 'effect.json'), os.path.join(root, 'effects', name, 'effect.json'))
        for sub in ('materials', 'shaders'):
            for folder, _, files in os.walk(os.path.join(source, sub)):
                target = os.path.join(root, os.path.relpath(folder, source))
                os.makedirs(target, exist_ok=True)
                for file in files:
                    shutil.copy(os.path.join(folder, file), os.path.join(target, file))
    write(wallpaper.name + '/.gitignore', 'effects/\nmaterials/effects/\nshaders/\n')


def shipped(effect, constants=None, combos=None, id=0):
    """An instance of a shipped effect; `constants` and `combos` go to its first pass."""
    first = {"id": id * 10 + 1}
    if constants:
        first["constantshadervalues"] = constants
    if combos:
        first["combos"] = combos
    passes = [first] + [{"id": id * 10 + 2 + n} for n in range(SHIPPED_PASSES[effect] - 1)]
    return {"file": "effects/%s/effect.json" % effect, "id": id, "name": "", "visible": True, "passes": passes}


def effect_grid(name, title, cells, texture='util/clouds_256', clear='0.12 0.12 0.12'):
    """Cells of (label, effect instance) shown on 300x300 pictures of a texture, four to a row."""
    w = Wallpaper(name, title, clear=clear)
    model = w.texture_model('picture', texture)
    for i, (label, instance) in enumerate(cells):
        x = 260 + (i % 4) * 460
        y = 880 - (i // 4) * 340
        instance = dict(instance, id=100 + i * 10)
        instance['passes'] = [dict(p, id=1000 + i * 10 + n) for n, p in enumerate(instance['passes'])]
        w.image(label, model, x, y, 256, 256, scale='1.1 1.1 1', effects=[instance])
    w.save()
    used = sorted({i['file'].split('/')[1] for _, i in cells if i.get('file')})
    copy_shipped_effects(w, used)
    return w


# ---------------------------------------------------------------------------------------------------------------
# 3D scenes: cubes as Wallpaper Engine's static mesh format (MDLV0017), seen through a perspective camera
# ---------------------------------------------------------------------------------------------------------------
_binary_spec = importlib.util.spec_from_file_location('binary', os.path.join(MAKE, 'mdl.py'))
binary = importlib.util.module_from_spec(_binary_spec)
_binary_spec.loader.exec_module(binary)
cube_mdl = binary.cube_mdl
puppet_mdl = binary.puppet_mdl
quad_mdl = binary.quad_mdl
quads_mdl = binary.quads_mdl


class Scene3D(Wallpaper):
    def __init__(self, name, title, ambient='0.2 0.2 0.2', clear='0.05 0.05 0.1', eye='0 0 12'):
        super().__init__(name, title, clear=clear)
        self.ambient = ambient
        self.eye = eye
        # more settings of scene.json's general
        self.general = {}
        # the camera's paths (scripts/camera_00.json), if it has any
        self.camera_paths = None

    def material(self, name, color='1 1 1', lighting=False, blending='normal', alpha=1.0, depth=True, combos=None):
        """`combos` replaces the material's combos (left out, generic4 takes its defaults: LIGHTING 1)"""
        path = 'materials/%s.json' % name
        self.files[path] = {"passes": [{
            "shader": "generic4", "blending": blending, "cullmode": "normal",
            "depthtest": "enabled" if depth else "disabled", "depthwrite": "enabled" if depth else "disabled",
            "combos": combos if combos is not None else
            {"LIGHTING": 1 if lighting else 0, "REFLECTION": 0, "RIMLIGHTING": 0, "SHADINGGRADIENT": 0},
            "constantshadervalues": {"color": color, "alpha": alpha}, "textures": [None]}]}
        mdl = 'models/%s.mdl' % name
        write_binary(os.path.join(self.name, mdl), cube_mdl(path))
        return mdl

    def cube(self, name, mdl, x, y, z=0, angles=(0, 0, 0), scale='1 1 1'):
        return self.add({"name": name, "model": mdl, "origin": vec(x, y, z), "angles": vec(*angles), "scale": scale})

    def light_config(self):
        """How many lights of each kind the scene has: without it the real engine lights nothing"""
        kinds = [o.get('light') for o in self.objects if 'light' in o]
        return {name: kinds.count(key) for name, key in
                [('point', 'lpoint'), ('spot', 'lspot'), ('tube', 'ltube'), ('directional', 'ldirectional')]}

    def light(self, name, kind, x, y, z, color='1 1 1', intensity=2, radius=20, angles=(0, 0, 0), **extra):
        angles = angles if isinstance(angles, dict) else vec(*angles)
        return self.add({"name": name, "light": kind, "origin": vec(x, y, z), "color": color, "intensity": intensity,
                         "radius": radius, "angles": angles, "scale": "1 1 1", **extra})

    def save(self):
        write(self.name + '/project.json', {
            "title": "wpe-test " + self.title, "type": "scene", "file": "scene.json", "version": 1,
            "general": {"properties": self.properties}})
        camera = {"center": "0 0 0", "eye": self.eye, "up": "0 1 0"}
        if self.camera_paths:
            camera["paths"] = ["scripts/camera_00.json"]
            self.files['scripts/camera_00.json'] = {"paths": self.camera_paths}
        write(self.name + '/scene.json', {
            "camera": camera,
            "general": {"orthogonalprojection": None, "fov": 50, "nearz": 0.1, "farz": 100,
                        "clearcolor": self.clear, "clearenabled": True, "cameraparallax": False, "bloom": False,
                        "zoom": 1, "ambientcolor": self.ambient, "skylightcolor": self.ambient,
                        "lightconfig": self.light_config(), **self.general},
            "objects": self.objects, "version": 3})
        for path, content in self.files.items():
            write('%s/%s' % (self.name, path), content)


def write_binary(path, data):
    full = os.path.join(HERE, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, 'wb') as f:
        f.write(data)


# 20-22. model-light-*: one kind of light on a row of lit cubes
def lit_row(name, title, light):
    w = Scene3D(name, title, ambient='0.05 0.05 0.05')
    lit = w.material('lit', '1 1 1', lighting=True)
    for i in range(3):
        w.cube('cube %d' % i, lit, -4 + i * 4, 0, 0, (0.3, 0.6, 0), '1.3 1.3 1.3')
    light(w)
    w.save()


# 22e. camera-fade, -on, -off: a camera path that holds still for 10 seconds, cuts 3 units to the side and holds again
# for 10, then moves back 6 units over 10; camerafade left out, true and false. Stamped with a clock probe: how far the
# camera has come after the cut, and how far it trails the moving segment, at each probed frame.
def camera_fade(name, title, fade):
    w = Scene3D(name, title)
    w.cube('cube', w.material('white', '1 1 1'), 0, 0, 0)
    w.cube('marker', w.material('red', '1 0.2 0.2'), 0, 2, 0, scale='0.3 0.3 0.3')

    def key(x, timestamp):
        return {"eye": vec(x, 0, 12), "center": vec(x, 0, 0), "up": "0 1 0", "timestamp": timestamp}
    w.camera_paths = [
        {"duration": 10, "name": "", "transforms": [key(0, 0), key(0, 10)]},
        {"duration": 10, "name": "", "transforms": [key(3, 0), key(3, 10)]},
        {"duration": 10, "name": "", "transforms": [key(3, 0), key(-3, 10)]},
    ]
    if fade is not None:
        w.general['camerafade'] = fade
    w.save()
    clock_probe.stamp(os.path.join(HERE, name))


# 22f. model-shadow-*: a cube in front of a lit wall, its light off to one side casting a shadow on the wall (the light
# config counts one shadowed light of that kind)
def shadow_scene(name, title, add_light, config):
    w = Scene3D(name, title, ambient='0.05 0.05 0.05')
    lit = w.material('lit', '1 1 1', lighting=True)
    w.cube('wall', lit, 0, 0, -1, (0, 0, 0), '12 7 0.2')
    w.cube('cube', lit, 0, 0, 1, (0.3, 0.5, 0), '0.8 0.8 0.8')
    add_light(w)
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 0, **config}
    w.save()


# 25. animation-base-value: what an animation does to a property that has a value of its own: the animation of a vector
# (origin, scale, colour, angles) is added to it, relative or not; the animation of a number (alpha) replaces it
def constant_animation(values, relative=False):
    return animation([[(0, v), (6, v)] for v in values], relative=relative)


def base_layer(w, name, **extra):
    return w.add({"name": name, "image": "models/util/solidlayer.json", "size": "100 100", "color": "1 1 1",
                  "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "origin": "960 540 0", "alignment": "center",
                  "visible": True, **extra})


# ---------------------------------------------------------------------------------------------------------------
# Video wallpapers: still test pictures as H.264 (made with ffmpeg here, so nothing is downloaded): colour bars for
# the colour conversion, a grid of squares for the aspect ratio, and a one-pixel frame for what is cut off
# ---------------------------------------------------------------------------------------------------------------
def video_wallpaper(name, title, width, height, seconds=2):
    import shutil
    import subprocess
    folder = os.path.join(HERE, name)
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, 'video.mp4')
    if not os.path.exists(path):
        if not shutil.which('ffmpeg'):
            print('  (no ffmpeg: %s has no video)' % name)
            return
        cell = max(8, height // 9)
        picture = ('smptehdbars=size=%dx%d:rate=30,drawgrid=width=%d:height=%d:thickness=2:color=white@0.6,'
                   'drawbox=x=0:y=0:w=iw:h=ih:thickness=1:color=red') % (width, height, cell, cell)
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i', picture, '-t', str(seconds),
                        '-c:v', 'libx264', '-preset', 'veryslow', '-crf', '12', '-pix_fmt', 'yuv420p',
                        '-colorspace', 'bt709', '-color_primaries', 'bt709', '-color_trc', 'bt709',
                        '-color_range', 'tv', '-movflags', '+faststart', path], check=True)
    write(name + '/project.json', {"title": "wpe-test " + title, "type": "video", "file": "video.mp4",
                                   "general": {"properties": {}}})


# ---------------------------------------------------------------------------------------------------------------
# Audio: wallpapers whose bars show the sound Wallpaper Engine hears (engine.registerAudioBuffers), each with the
# sound to play while the picture is taken (audio.wav, made with ffmpeg: steady tones, so the bars stand still)
# ---------------------------------------------------------------------------------------------------------------
AUDIO_BARS = """'use strict';
const audio = engine.registerAudioBuffers(engine.AUDIO_RESOLUTION_64);
const bars = [];
export function init(value) {
    // the left channel's 64 bands at the top, the right channel's at the bottom, growing up from a line
    for (const [row, base] of [[0, 600], [1, 100]]) {
        for (let i = 0; i < 64; i++) {
            bars.push(thisScene.createLayer({image: 'models/util/solidlayer.json', name: 'bar ' + row + ' ' + i,
                origin: new Vec3(60 + i * 28, base, 0), size: new Vec2(20, 2),
                color: row === 0 ? new Vec3(1, 0.6, 0.2) : new Vec3(0.2, 0.7, 1)}));
        }
    }
    return value;
}
export function update(value) {
    for (let i = 0; i < 64; i++) {
        for (const [row, values] of [[0, audio.left], [1, audio.right]]) {
            const bar = bars[row * 64 + i];
            const height = Math.max(2, values[i] * 400);
            bar.scale = new Vec3(1, height / 2, 1);
            bar.origin = new Vec3(bar.origin.x, (row === 0 ? 600 : 100) + height / 2, 0);
        }
    }
    return value;
}
"""


def audio_wallpaper(name, title, sound):
    import shutil
    import subprocess
    w = Wallpaper(name, title, clear='0 0 0')
    w.add({"name": "driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
           "origin": {"value": "10 10 0", "script": AUDIO_BARS, "scriptproperties": {}}, "angles": "0 0 0",
           "scale": "1 1 1", "alignment": "center"})
    w.solid('left baseline', 960, 598, 1800, 2, '0.3 0.3 0.3')
    w.solid('right baseline', 960, 98, 1800, 2, '0.3 0.3 0.3')
    w.save()
    # Wallpaper Engine listens to the sound only for wallpapers that say they use it
    project_path = os.path.join(HERE, name, 'project.json')
    project = json.load(open(project_path))
    project['general']['supportsaudioprocessing'] = True
    write(name + '/project.json', project)
    path = os.path.join(HERE, name, 'audio.wav')
    if not os.path.exists(path) and shutil.which('ffmpeg'):
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i', sound, '-t', '4',
                        '-ar', '48000', '-c:a', 'pcm_s16le', path], check=True)
    # made again from the line above whenever it is missing, so it is not committed
    write(name + '/.gitignore', 'audio.wav\n')
    # the bars follow the script's reading of the sound, a moment after the probe's record of it: one bar's width
    # of difference is not a difference
    write(name + '/compare.json', {"bar_jitter": 20})
    # the probe records the bands Wallpaper Engine heard, which the viewer is then given
    clock_probe.stamp(os.path.join(HERE, name))


# ---------------------------------------------------------------------------------------------------------------
# Regression wallpapers for owe fixes: each shows one difference found in a Workshop wallpaper, in a scene of our own
# ---------------------------------------------------------------------------------------------------------------
def tex_file(w, name, width, height, pixels, format=binary.RGBA8888):
    """A texture of the wallpaper's own (materials/NAME.tex), as a material names it ("NAME")."""
    write_binary(os.path.join(w.name, 'materials', name + '.tex'), binary.tex(width, height, pixels, format))
    return name


def textured_material(w, name, textures, combos=None, constants=None):
    """A generic4 material with `textures` (names, or None for the default) and a cube model wearing it."""
    path = 'materials/%s.json' % name
    w.files[path] = {"passes": [{
        "shader": "generic4", "blending": "normal", "cullmode": "normal", "depthtest": "enabled",
        "depthwrite": "enabled", "combos": combos if combos is not None else {"LIGHTING": 1},
        "constantshadervalues": constants or {}, "textures": textures}]}
    model = 'models/%s.mdl' % name
    write_binary(os.path.join(w.name, model), cube_mdl(path))
    return model


# 48-49. effect-eye-3d, effect-eye-2d: a fullscreen effect painting g_EyePosition (each component's tenths and
# thousandths): Wallpaper Engine gives an effect the scene camera's eye, the camera path's in 3D and 2000 in front of
# the middle in 2D, where owe's effects saw 0 (owe #61)
EYE_FRAG = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec3 g_EyePosition;
uniform float u_Scale; // {"material":"scale","default":1}

void main() {
	vec3 eye = abs(g_EyePosition) * u_Scale;
	vec3 colour = v_TexCoord.x < 0.5 ? frac(eye) : frac(eye * 0.01);
	gl_FragColor = vec4(colour, 1.0);
}
"""


def eye_wallpaper(w, scale):
    local_effect(w, 'eye', EYE_FRAG, {})
    w.add({"name": "eye", "image": "models/util/fullscreenlayer.json", "origin": "0 0 0", "scale": "1 1 1",
           "angles": "0 0 0", "color": "1 1 1", "alpha": 1, "effects": [use('eye', 900, {"scale": scale})]})
    w.save()


# 50. pointer-outside: a fullscreen effect painting g_PointerPosition; the capture parks the pointer in the screen's
# corner, outside the window, and Wallpaper Engine reports it there unclamped (owe #62). Stamped with the clock probe,
# which records the pointer for owe_compare to give the viewer.
POINTER_FRAG = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec2 g_PointerPosition;

void main() {
	vec2 pointer = g_PointerPosition * 0.4;
	gl_FragColor = vec4(saturate(pointer.x), saturate(pointer.y), v_TexCoord.x, 1.0);
}
"""


def strip_mesh(width=600.0, height=100.0, columns=9, bones=2):
    """A strip of two rows of `columns` vertices centred on the origin, skinned along its length to `bones` bones
    (each weighted across its own stretch, blended with the next): its vertices and triangles for puppet_mdl()."""
    vertices, indices = [], []
    for row in range(2):
        for column in range(columns):
            t = column / (columns - 1) * (bones - 1)
            first = min(int(t), bones - 2)
            blend = t - first
            vertices.append((-width / 2 + width * column / (columns - 1), (row - 0.5) * height, 0,
                             column / (columns - 1), row, [first, first + 1, 0, 0], [1 - blend, blend, 0, 0]))
    for column in range(columns - 1):
        a, b, c, d = column, column + 1, columns + column, columns + column + 1
        indices += [(a, b, c), (b, d, c)]
    return vertices, indices
