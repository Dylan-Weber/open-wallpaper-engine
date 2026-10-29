#!/usr/bin/env python3
"""Stamps a test wallpaper with a clock probe, so its pictures record the moment they were taken.

    ./clock-probe.py FOLDER [FOLDER ...]

The probe is a fullscreen layer drawn last, whose effect passes the scene through and paints a strip of 14 cells in
the picture's bottom left corner and again in its top left (whichever way the engine's screen coordinates run): a
white cell, the wallpaper's frame (g_Time at 30 a second) in 12 bits, black for 0 and white for 1, lowest bit first,
and a black cell. A second strip, two rows further in, shows the scripts' frame (engine.runtime, written by a
script), which Wallpaper Engine starts after loading: before the scripts run its markers are swapped. Each cell is 1/80 of the picture wide and 1/45 high, one cell in from the edges, so it works in 2D
and 3D scenes alike. It is read from the cells' centres, so the bloom around it doesn't matter.
Beside the bottom strips (or the top ones, if the picture is upside down) are records of the inputs Wallpaper Engine
gave the scene, three bits to a cell (red, green and blue), between the same markers: the shaders' pointer
(g_PointerPosition), eye (g_EyePosition) and the loudest band of each audio spectrum; and, from scripts, the last left
press, the date and time zone the scripts see, and the camera's eye (thisScene.getCameraTransforms()). A wallpaper
that listens to sound also gets a record of every band its shaders and scripts use (compare.json "probe_bands"), so
owe can be given the very same spectrum.
owe_compare.py reads a reference's probe, renders owe at that frame, checks owe's probe reads the same,
and leaves the strips out of the comparison.

The scene is the file project.json names (scene.json by default). A scene in a package (scene.pkg, or X.pkg for X.json:
a Workshop wallpaper) is stamped inside it: Wallpaper Engine doesn't draw every unpacked one alike.
"probe": true goes to the folder's compare.json. Workshop wallpapers stay out of git (wallpapers/_scratch).
"""
import json
import os
import re
import struct
import sys

CELLS = 14
BITS = 12

VERT = """uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec4 v_TexCoord;

void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_TexCoord = a_TexCoord.xyxy;
}
"""

# a record's row: a marker, up to 64 cells of bits and a marker, one cell in from the left
ROW_CELLS = 66
ROW_BITS = (ROW_CELLS - 2) * 3

# the records' fields, (name, bits) in order (no names ending in _x or _y: Wallpaper Engine's HLSL takes those
# for vectors' parts). Pointer coordinates are kept from -4 to 12 in 1/4096ths, eye coordinates
# from -32768 in 1/256ths, levels and bands from 0 to 1 in 1/255ths; a field whose bits are all ones is unknown
SHADER_FIELDS = [('pointer_x', 16), ('pointer_y', 16), ('eye_x', 24), ('eye_y', 24), ('eye_z', 24)] + [
    ('level%d%s' % (resolution, side), 8) for resolution in (16, 32, 64) for side in 'lr']
SCRIPT_FIELDS = [('press', 12), ('date_day', 16), ('date_centis', 24), ('timezone', 11), ('camerax', 24),
                 ('cameray', 24), ('cameraz', 24)]
RESOLUTIONS = (16, 32, 64)

# the records' first rows, counted like the strips' (the bands' rows go on towards the middle)
RECORD_ROWS = {'shader': 39, 'script': 37, 'bands': 35}

# the date's days count from 2020-01-01 (18262 days after 1970-01-01)
EPOCH_DAYS = 18262


def band_fields(bands):
    """The bands record's fields: each resolution's left bands, then its right."""
    return [('band%d%s%d' % (resolution, side, index), 8)
            for resolution in sorted(bands) for side in 'lr' for index in range(resolution)]


def spectrum(resolution, side):
    return 'g_AudioSpectrum%d%s' % (resolution, 'Left' if side == 'l' else 'Right')


def packed(resolution, side, index):
    return '%s[%d][%d]' % (spectrum(resolution, side), index // 4, index % 4)


def bit_function(name, fields, values):
    """GLSL returning bit `b` of a record of `fields`, whose values are the GLSL `values` by name."""
    lines = ['float %s(float b) {' % name]
    offset = 0
    for field, width in fields:
        lines.append('\tif (b < %d.0) return bitOf(%s, b - %d.0);' % (offset + width, values(field), offset))
        offset += width
    return '\n'.join(lines + ['\treturn 0.0;', '}'])


def record_block(kind, bits, bit, started, mirrored=False):
    """GLSL drawing a record of `bits` bits from its first row on (each row going towards the middle), and with
    `mirrored` beside the top strips too."""
    rows = -(-bits // ROW_BITS)
    cells = -(-bits // 3)
    mirror = '\t\tif (row < 22.0) j = row - %d.0;\n' % (44 - RECORD_ROWS[kind]) if mirrored else ''
    return ('\t{\n\t\tfloat j = %d.0 - row;\n' + mirror +
            '\t\tif (j > -0.5 && j < %d.5 && cell > -0.5) {\n'
            '\t\t\tfloat count = min(%d.0, %d.0 - j * %d.0);\n'
            '\t\t\tfloat started = %s;\n'
            '\t\t\tif (cell < 0.5) colour = vec4(started, started, started, 1.0);\n'
            '\t\t\telse if (cell < count + 0.5) {\n'
            '\t\t\t\tfloat b = (j * %d.0 + cell - 1.0) * 3.0;\n'
            '\t\t\t\tcolour = vec4(vec3(%s(b), %s(b + 1.0), %s(b + 2.0)) * started, 1.0);\n'
            '\t\t\t} else if (cell < count + 1.5) colour = vec4(1.0 - started, 1.0 - started, 1.0 - started, 1.0);\n'
            '\t\t}\n\t}\n') % (RECORD_ROWS[kind], rows - 1, ROW_CELLS - 2, cells, ROW_CELLS - 2, started,
                                ROW_CELLS - 2, bit, bit, bit)


def quantised(expression, scale, offset, bits):
    return 'floor(clamp((%s + %s) * %s + 0.5, 0.0, %d.0))' % (expression, offset, scale, (1 << bits) - 1)


def frag(bands=(), mirrored=False):
    """The probe's fragment shader: the scene, the strips and the records (with the bands of `bands`; with
    `mirrored`, beside both the bottom and the top strips)."""
    uniforms = ['uniform vec2 g_PointerPosition;', 'uniform vec3 g_EyePosition;'] + [
        'uniform float %s[%d];' % (spectrum(resolution, side), resolution)
        for resolution in RESOLUTIONS for side in 'lr'] + [
        'uniform float u_%s; // {"material":"%s","default":-1}' % (name, name) for name, _ in SCRIPT_FIELDS]
    # in a fragment shader Wallpaper Engine packs these arrays four to a vector, so each band is read as
    # [band / 4][band % 4] with constant indices (owe flattens that again)
    peaks = ['float peak%d%s() {\n\tfloat m = 0.0;\n%s\treturn m;\n}' % (
        resolution, side, ''.join('\tif (%s > m) m = %s;\n' % (
            packed(resolution, side, index), packed(resolution, side, index)) for index in range(resolution)))
        for resolution in RESOLUTIONS for side in 'lr']

    def shader_value(field):
        if field.startswith('pointer'):
            return quantised('g_PointerPosition.' + field[-1], '4096.0', '4.0', 16)
        if field.startswith('eye'):
            return quantised('g_EyePosition.' + field[-1], '256.0', '32768.0', 24)
        return quantised('peak%s()' % field[5:], '255.0', '0.0', 8)

    functions = peaks + [bit_function('shaderBit', SHADER_FIELDS, shader_value),
                         bit_function('scriptBit', SCRIPT_FIELDS,
                                      lambda field: '(u_%s < 0.0 ? 0.0 : u_%s)' % (field, field))]
    blocks = [record_block('shader', sum(w for _, w in SHADER_FIELDS), 'shaderBit', '1.0', mirrored),
              record_block('script', sum(w for _, w in SCRIPT_FIELDS), 'scriptBit', 'step(-0.5, u_press)', mirrored)]
    if bands:
        fields = band_fields(bands)
        lookup = ['float band(float i) {']
        offset = 0
        for resolution in sorted(bands):
            for side in 'lr':
                for index in range(resolution):
                    lookup.append('\tif (i < %d.5) return %s;' % (offset + index, packed(resolution, side, index)))
                offset += resolution
        functions += ['\n'.join(lookup + ['\treturn 0.0;', '}']),
                      'float bandBit(float b) {\n\tif (b >= %d.0) return 0.0;\n'
                      '\treturn bitOf(%s, mod(b, 8.0));\n}' % (
                          len(fields) * 8, quantised('band(floor(b / 8.0))', '255.0', '0.0', 8))]
        blocks.append(record_block('bands', len(fields) * 8, 'bandBit', '1.0', mirrored))
    return FRAG_TEMPLATE % {
        'uniforms': '\n'.join(uniforms), 'functions': '\n\n'.join(functions), 'records': ''.join(blocks),
        'last': CELLS - 1, 'bits': BITS}


# the scene, and the strips: cell 0 white, cells 1..12 a frame's bits, cell 13 black. Rows 1 and 43 (of 45) show
# the shader's frame (g_Time), rows 3 and 41 the scripts' (engine.runtime, written by a script into u_ScriptFrame;
# -1 until the scripts run, shown with the markers swapped). Then the records
FRAG_TEMPLATE = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform float g_Time;
uniform float u_ScriptFrame; // {"material":"scriptframe","default":-1}
%(uniforms)s

float bitOf(float value, float b) {
	return mod(floor(value / exp2(b)), 2.0);
}

%(functions)s

void main() {
	vec2 uv = v_TexCoord.xy;
	vec4 scene = texSample2D(g_Texture0, uv);
	float cell = floor(uv.x * 80.0) - 1.0;
	float row = floor(uv.y * 45.0);
	float across = step(0.0, cell) * step(cell, %(last)d.5);
	float shader = across * (step(abs(row - 1.0), 0.1) + step(abs(row - 43.0), 0.1));
	float script = across * (step(abs(row - 3.0), 0.1) + step(abs(row - 41.0), 0.1));
	float frame = script > 0.5 ? u_ScriptFrame : floor(g_Time * 30.0 + 0.5);
	float started = step(-0.5, frame);
	float bit = mod(floor(max(frame, 0.0) / pow(2.0, max(cell - 1.0, 0.0))), 2.0);
	float value = cell < 0.5 ? started : (cell > %(bits)d.5 ? 1.0 - started : bit * started);
	vec4 colour = mix(scene, vec4(value, value, value, 1.0), saturate(shader + script));
%(records)s	gl_FragColor = colour;
}
"""

FRAG = frag()

# the script that writes the scripts' frame
SCRIPT = """'use strict';

export function update(value) {
	return Math.floor(engine.runtime * 30 + 0.5);
}
"""

# the scripts that write the records' script fields: each returns its field, all ones when it can't tell
QUANTISE_JS = """function quantise(value, offset, scale, bits) {
	const most = Math.pow(2, bits) - 1;
	return Math.min(Math.max(Math.floor((value + offset) * scale + 0.5), 0), most);
}
"""
FIELD_SCRIPTS = {
    'press': """'use strict';

let last = 0;
let wasDown = false;

export function update(value) {
	try {
		const down = !!input.cursorLeftDown;
		if (down && !wasDown) last = Math.min(Math.floor(engine.runtime * 30 + 0.5) + 1, 4094);
		wasDown = down;
		return last;
	} catch (e) {
		return 4095;
	}
}
""",
    'date_day': """'use strict';

export function update(value) {
	return Math.floor(Date.now() / 86400000) - %d;
}
""" % EPOCH_DAYS,
    'date_centis': """'use strict';

export function update(value) {
	return Math.floor((Date.now() % 86400000) / 10);
}
""",
    'timezone': """'use strict';

export function update(value) {
	return new Date().getTimezoneOffset() + 1024;
}
""",
}
for axis in 'xyz':
    FIELD_SCRIPTS['camera' + axis] = """'use strict';

%s
export function update(value) {
	try {
		return quantise(thisScene.getCameraTransforms().eye.%s, 32768, 256, 24);
	} catch (e) {
		return 16777215;
	}
}
""" % (QUANTISE_JS, axis)

# what read_script gives for a probe whose scripts haven't run yet
NOT_STARTED = 'not started'


def scene_file(folder):
    """The scene's file, as project.json names it (scene.json by default); its package is X.pkg."""
    project = os.path.join(folder, 'project.json')
    if os.path.exists(project):
        with open(project) as f:
            return json.load(f).get('file', 'scene.json')
    return 'scene.json'


def unpack(package):
    """A package's version (b'PKGV0024') and its files, by name."""
    with open(package, 'rb') as f:
        data = f.read()
    length = struct.unpack_from('<I', data, 0)[0]
    version = data[4:4 + length]
    position = 4 + length
    count = struct.unpack_from('<I', data, position)[0]
    position += 4
    entries = []
    for _ in range(count):
        length = struct.unpack_from('<I', data, position)[0]
        position += 4
        name = data[position:position + length].decode('utf-8')
        position += length
        offset, size = struct.unpack_from('<II', data, position)
        position += 8
        entries.append((name, offset, size))
    return version, {name: data[position + offset:position + offset + size] for name, offset, size in entries}


def pack(files, version):
    """A package of `files` (name: bytes)."""
    header = struct.pack('<I', len(version)) + version + struct.pack('<I', len(files))
    data = b''
    for name, content in files.items():
        encoded = name.encode('utf-8')
        header += struct.pack('<I', len(encoded)) + encoded + struct.pack('<II', len(data), len(content))
        data += content
    return header + data


def write(folder, path, content):
    target = os.path.join(folder, path)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, 'w') as f:
        f.write(content if isinstance(content, str) else json.dumps(content, indent=2) + '\n')


def probe_files(scene, bands=(), mirrored=False):
    """The scene with the probe added (replacing an earlier one), and the probe's own files (recording the audio
    `bands`, the records `mirrored` beside the top strips too)."""
    objects = [o for o in scene.get('objects', []) if o.get('name') != 'owe clock probe']
    ids = [o.get('id', 0) for o in objects if isinstance(o.get('id', 0), int)]
    objects.append({
        "id": max(ids + [0]) + 1, "name": "owe clock probe", "image": "models/util/fullscreenlayer.json",
        "origin": "0 0 0", "scale": "1 1 1", "angles": "0 0 0", "color": "1 1 1", "alpha": 1, "visible": True,
        "effects": [{"file": "effects/oweclock/effect.json", "id": max(ids + [0]) + 2, "name": "oweclock",
                     "visible": True, "passes": [{"id": max(ids + [0]) + 3, "constantshadervalues": {
                         "scriptframe": {"script": SCRIPT, "value": -1},
                         **{name: {"script": script, "value": -1} for name, script in FIELD_SCRIPTS.items()}}}]}]})
    scene = dict(scene, objects=objects)
    as_json = lambda value: (json.dumps(value, indent=2) + '\n').encode()
    return scene, {
        'effects/oweclock/effect.json': as_json({
            "version": 1, "name": "oweclock", "passes": [{"material": "materials/effects/oweclock.json"}]}),
        'materials/effects/oweclock.json': as_json({"passes": [{
            "shader": "effects/oweclock", "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
            "cullmode": "nocull"}]}),
        'shaders/effects/oweclock.vert': VERT.encode(),
        'shaders/effects/oweclock.frag': frag(bands, mirrored).encode(),
    }


def used_bands(contents):
    """The audio spectrum resolutions that shaders (g_AudioSpectrum64Left) or scripts (registerAudioBuffers) use in
    `contents` (bytes)."""
    found = set()
    for content in contents:
        found.update(int(n) for n in re.findall(rb'g_AudioSpectrum(16|32|64)', content))
        found.update(int(n) for n in re.findall(rb'registerAudioBuffers\([^)]*?(16|32|64)\s*\)', content))
    return sorted(found)


def wallpaper_file(path):
    """Whether a file is the wallpaper's own (shader, script or JSON), not the probe's or the comparison's."""
    return path.endswith(('.frag', '.vert', '.js', '.json')) and 'oweclock' not in path and \
        os.path.basename(path) != 'compare.json'


def folder_contents(folder):
    for root, _, names in os.walk(folder):
        for name in names:
            if wallpaper_file(os.path.join(root, name)):
                with open(os.path.join(root, name), 'rb') as f:
                    yield f.read()


def stamp(folder):
    name = scene_file(folder)
    package = os.path.join(folder, os.path.splitext(name)[0] + '.pkg')
    packaged = not os.path.exists(os.path.join(folder, name)) and os.path.exists(package)
    # a wallpaper that listens gets its bands recorded, so owe can be given the same spectrum (Wallpaper Engine
    # hears something in the VM even with nothing playing)
    contents = list(folder_contents(folder))
    if packaged:
        contents += [content for path, content in unpack(package)[1].items() if wallpaper_file(path)]
    bands = used_bands(contents)
    # layers a script creates are drawn after the probe and may cover its records: they go on both sides then
    mirrored = any(b'createLayer' in content for content in contents)
    if packaged:
        # Wallpaper Engine doesn't draw every unpacked Workshop wallpaper alike: stamp inside the package
        version, files = unpack(package)
        scene, extra = probe_files(json.loads(files[name]), bands, mirrored)
        files[name] = (json.dumps(scene, indent=2) + '\n').encode()
        files.update(extra)
        with open(package, 'wb') as f:
            f.write(pack(files, version))
    else:
        with open(os.path.join(folder, name)) as f:
            scene, extra = probe_files(json.load(f), bands, mirrored)
        write(folder, name, scene)
        for path, content in extra.items():
            write(folder, path, content.decode())
    settings_path = os.path.join(folder, 'compare.json')
    settings = {}
    if os.path.exists(settings_path):
        with open(settings_path) as f:
            settings = json.load(f)
    settings['probe'] = True
    if bands:
        settings['probe_bands'] = bands
    else:
        settings.pop('probe_bands', None)
    if mirrored:
        settings['probe_mirrored'] = True
    else:
        settings.pop('probe_mirrored', None)
    write(folder, 'compare.json', settings)
    print('%s: stamped' % folder)


def strips(size):
    """The strips' rectangles in a picture of `size` (left, top, right, bottom): bottom left, then top left."""
    width, height = size
    left, right = width / 80, (CELLS + 1) * width / 80
    return [(left, height - 2 * height / 45, right, height - height / 45), (left, height / 45, right, 2 * height / 45)]


def script_strips(size):
    """The scripts' strips, two rows in from the shader's: bottom left, then top left."""
    width, height = size
    left, right = width / 80, (CELLS + 1) * width / 80
    return [(left, height - 4 * height / 45, right, height - 3 * height / 45),
            (left, 3 * height / 45, right, 4 * height / 45)]


def read_strips(picture, boxes):
    """A frame, NOT_STARTED (the markers swapped), or None when no strip reads."""
    pixels = picture.convert('L')
    for left, top, right, bottom in boxes:
        cell = (right - left) / CELLS
        values = [pixels.getpixel((int(left + (index + 0.5) * cell), int((top + bottom) / 2))) > 127
                  for index in range(CELLS)]
        if values[0] and not values[-1]:
            return sum(1 << bit for bit, on in enumerate(values[1:-1]) if on)
        if values[-1] and not any(values[:-1]):
            return NOT_STARTED
    return None


def read(picture):
    """The shader's frame (g_Time) a picture's probe shows, or None when no strip reads as a probe."""
    frame = read_strips(picture, strips(picture.size))
    return frame if isinstance(frame, int) else None


def read_script(picture):
    """The scripts' frame (engine.runtime) a picture's probe shows, NOT_STARTED, or None."""
    return read_strips(picture, script_strips(picture.size))


def record_rows(size, kind, flipped=False, bands=()):
    """A record's rows (left, top, right, bottom), each ROW_CELLS wide: beside the bottom strips, or the top ones in
    a picture upside down."""
    width, height = size
    bits = {'shader': sum(w for _, w in SHADER_FIELDS), 'script': sum(w for _, w in SCRIPT_FIELDS),
            'bands': len(band_fields(bands)) * 8}[kind]
    boxes = []
    for j in range(-(-bits // ROW_BITS)):
        row = RECORD_ROWS[kind] - j
        if flipped:
            row = 44 - row
        boxes.append((width / 80, row * height / 45, (ROW_CELLS + 1) * width / 80, (row + 1) * height / 45))
    return boxes


def read_record(picture, rows, bits, plainness=False):
    """A record's bits (lowest first), NOT_STARTED (the markers swapped), or None when it doesn't read. With
    `plainness`, each bit comes with how plainly its cell reads (0 half way between the markers, 1 at one)."""
    pixels = picture.convert('RGB')
    found = []
    state = None
    for j, (left, top, right, bottom) in enumerate(rows):
        cell = (right - left) / ROW_CELLS
        count = min(ROW_CELLS - 2, -(-bits // 3) - j * (ROW_CELLS - 2))
        y = int((top + bottom) / 2)
        colours = [pixels.getpixel((int(left + (index + 0.5) * cell), y)) for index in range(count + 2)]
        first, last = colours[0], colours[-1]
        # a bright bloom lifts the black cells (Demon Core's to (140, 55, 4)): a channel's one is nearer the white
        # marker than the black
        if all(f - l > 60 for f, l in zip(first, last)):
            row_state, white, black = 'started', first, last
        elif all(l - f > 60 for f, l in zip(first, last)):
            row_state, white, black = NOT_STARTED, last, first
        else:
            return None
        middle = [(w + b) / 2 for w, b in zip(white, black)]
        plain = [[abs(channel - m) * 2 / max(w - b, 1) for channel, m, w, b in zip(colour, middle, white, black)]
                 for colour in colours[1:-1]]
        colours = [tuple(channel > m for channel, m in zip(colour, middle)) for colour in colours[1:-1]]
        if row_state == NOT_STARTED and any(any(c) for c in colours):
            return None
        if plainness:
            # a cell is as plain as its least plain channel: a light blue bar over it is half way in green
            colours = [tuple((bit, min(amounts)) for bit in colour) for colour, amounts in zip(colours, plain)]
        if state not in (None, row_state):
            return None
        state = row_state
        for colour in colours:
            found += list(colour)
    return NOT_STARTED if state == NOT_STARTED else found[:bits]


def read_either(picture, rows_of, bits, mirrored=False):
    """A record read from whichever side it shows on, `rows_of(flipped)` giving its rows there (beside the bottom
    strips first). A probe drawn on both sides (`mirrored`) has each bit from the side whose cell reads more
    plainly, where both read: a layer drawn after the probe may cross a cell. The bits and the side (True: beside
    the top strips), NOT_STARTED, or None."""
    sides = {flipped: read_record(picture, rows_of(flipped), bits, plainness=True) for flipped in (False, True)}
    read = {flipped: found for flipped, found in sides.items() if isinstance(found, list)}
    if not read:
        return (NOT_STARTED, None) if NOT_STARTED in sides.values() else (None, None)
    if len(read) == 1 or not mirrored:
        flipped, found = next(iter(read.items()))
        return [bit for bit, _ in found], flipped
    return [a[0] if a[1] >= b[1] else b[0] for a, b in zip(read[False], read[True])], False


def fields_of(bits, fields):
    values, offset = {}, 0
    for name, width in fields:
        values[name] = sum(1 << bit for bit, on in enumerate(bits[offset:offset + width]) if on)
        offset += width
    return values


def read_inputs(picture, bands=(), mirrored=False):
    """The inputs a picture's records show: 'flipped' (the records are beside the top strips), 'pointer' (x, y),
    'eye' (x, y, z), 'levels' {resolution: (left, right)};
    from the scripts 'press' (the frame of the last left press, or None), 'date' (milliseconds since 1970, UTC),
    'timezone' (minutes behind UTC) and 'camera' (x, y, z, None where unknown), or 'scripts' NOT_STARTED; and with
    `bands`, 'bands' {resolution: {'left': [...], 'right': [...]}}; `mirrored` for a probe drawn on both sides
    (compare.json "probe_mirrored"). Empty when there are no records."""
    size = picture.size
    shader, flipped = read_either(picture, lambda side: record_rows(size, 'shader', side),
                                  sum(w for _, w in SHADER_FIELDS), mirrored)
    if not isinstance(shader, list):
        return {}
    values = fields_of(shader, SHADER_FIELDS)
    unknown = lambda name, width: values[name] == (1 << width) - 1
    inputs = {
        'flipped': flipped,
        'pointer': (values['pointer_x'] / 4096 - 4, values['pointer_y'] / 4096 - 4),
        'eye': tuple(values['eye_' + axis] / 256 - 32768 for axis in 'xyz'),
        'levels': {resolution: (values['level%dl' % resolution] / 255, values['level%dr' % resolution] / 255)
                   for resolution in RESOLUTIONS},
    }
    script, _ = read_either(picture, lambda side: record_rows(size, 'script', side),
                            sum(w for _, w in SCRIPT_FIELDS), mirrored)
    if script == NOT_STARTED:
        inputs['scripts'] = NOT_STARTED
    elif script is not None:
        values = fields_of(script, SCRIPT_FIELDS)
        inputs['scripts'] = 'started'
        if not unknown('press', 12):
            inputs['press'] = values['press'] - 1 if values['press'] else None
        inputs['date'] = (values['date_day'] + EPOCH_DAYS) * 86400000 + values['date_centis'] * 10
        inputs['timezone'] = values['timezone'] - 1024
        inputs['camera'] = tuple(None if unknown('camera' + axis, 24) else values['camera' + axis] / 256 - 32768
                                 for axis in 'xyz')
    if bands:
        fields = band_fields(bands)
        found, _ = read_either(picture, lambda side: record_rows(size, 'bands', side, bands), len(fields) * 8,
                               mirrored)
        if isinstance(found, list):
            values = fields_of(found, fields)
            inputs['bands'] = {resolution: {side: [values['band%d%s%d' % (resolution, side[0], index)] / 255
                                                   for index in range(resolution)] for side in ('left', 'right')}
                               for resolution in sorted(bands)}
    return inputs


def probe_boxes(size, flipped=False, bands=(), mirrored=False):
    """Every rectangle the probe draws over (left out of comparisons): the strips, and the records on the side
    `flipped` says (both with `mirrored`), as wide as they are drawn."""
    boxes = list(strips(size)) + list(script_strips(size))
    if mirrored:
        return boxes + [box for side in (False, True) for box in probe_boxes(size, side, bands)[4:]]
    for kind, bits in (('shader', sum(w for _, w in SHADER_FIELDS)), ('script', sum(w for _, w in SCRIPT_FIELDS)),
                       ('bands', len(band_fields(bands)) * 8)):
        for j, (left, top, right, bottom) in enumerate(record_rows(size, kind, flipped, bands)):
            count = min(ROW_CELLS - 2, -(-bits // 3) - j * (ROW_CELLS - 2))
            boxes.append((left, top, left + (count + 2) * (right - left) / ROW_CELLS, bottom))
    return boxes


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for folder in sys.argv[1:]:
        stamp(folder)
