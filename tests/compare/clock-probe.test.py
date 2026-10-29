#!/usr/bin/env python3
"""Tests clock-probe.py: the probe it adds and reading a picture of it.

    .venv/bin/python3 clock-probe.test.py
"""
import importlib.util
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('clock_probe', os.path.join(HERE, 'clock-probe.py'))
probe_tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe_tool)


def draw_strip(picture, box, frame):
    """Draws a strip showing `frame` (None: the scripts haven't run, the markers swapped)."""
    draw = ImageDraw.Draw(picture)
    left, top, right, bottom = box
    cell = (right - left) / probe_tool.CELLS
    for index in range(probe_tool.CELLS):
        if frame is None:
            on = index == probe_tool.CELLS - 1
        else:
            on = index == 0 or (0 < index < probe_tool.CELLS - 1 and frame >> (index - 1) & 1)
        draw.rectangle([left + index * cell, top, left + (index + 1) * cell - 1, bottom - 1],
                       fill=(255, 255, 255) if on else (0, 0, 0))


def draw_probe(frame, size=(1600, 900), which=0, script=None):
    """A picture of the probe (`which`: 0 bottom left, 1 top left) showing `frame` and the scripts' `script`, over a
    grey scene."""
    picture = Image.new('RGB', size, (60, 60, 60))
    draw_strip(picture, probe_tool.strips(size)[which], frame)
    draw_strip(picture, probe_tool.script_strips(size)[which], script)
    return picture


def draw_record(picture, rows, fields, values, started=True):
    """Draws a record (`rows`: its row boxes) of `fields` ((name, bits) pairs) showing `values` by name, three bits
    to a cell (red, green, blue), lowest bit first; `started` False swaps its markers and leaves it empty."""
    draw = ImageDraw.Draw(picture)
    bits = []
    for name, width in fields:
        bits += [values[name] >> bit & 1 for bit in range(width)] if started else [0] * width
    cells = (len(bits) + 2) // 3
    for row, (left, top, right, bottom) in enumerate(rows):
        cell = (right - left) / (probe_tool.ROW_CELLS)
        chunk = bits[row * probe_tool.ROW_CELLS * 3 - row * 6:][:(probe_tool.ROW_CELLS - 2) * 3]
        count = (len(chunk) + 2) // 3
        colours = [(255, 255, 255) if started else (0, 0, 0)]
        for index in range(count):
            triple = chunk[index * 3:index * 3 + 3] + [0, 0]
            colours.append(tuple(255 * b for b in triple[:3]))
        colours.append((0, 0, 0) if started else (255, 255, 255))
        for index, colour in enumerate(colours):
            draw.rectangle([left + index * cell, top, left + (index + 1) * cell - 1, bottom - 1], fill=colour)
    return cells


def inputs_picture(size=(1600, 900), flipped=False, script_started=True, bands=(), band_values=None):
    """A picture of the probe's records over a grey scene, with known inputs."""
    picture = draw_probe(300, size=size, which=1 if flipped else 0, script=290 if script_started else None)
    shader = {'pointer_x': round((0.25 + 4) * 4096), 'pointer_y': round((1.5 + 4) * 4096),
              'eye_x': round((12.5 + 32768) * 256), 'eye_y': round((-3.25 + 32768) * 256),
              'eye_z': round((800 + 32768) * 256),
              'level16l': 0, 'level16r': 0, 'level32l': 0, 'level32r': 0, 'level64l': 255, 'level64r': 128}
    script = {'press': 41, 'date_day': 2493, 'date_centis': 3600 * 100 * 9 + 1234, 'timezone': 1024 + 300,
              'camerax': round((12.5 + 32768) * 256), 'cameray': 16777215, 'cameraz': round((800 + 32768) * 256)}
    draw_record(picture, probe_tool.record_rows(size, 'shader', flipped), probe_tool.SHADER_FIELDS, shader)
    draw_record(picture, probe_tool.record_rows(size, 'script', flipped), probe_tool.SCRIPT_FIELDS, script,
                script_started)
    if bands:
        fields = probe_tool.band_fields(bands)
        draw_record(picture, probe_tool.record_rows(size, 'bands', flipped, bands), fields, band_values)
    return picture


class ClockProbe(unittest.TestCase):
    def test_stamps_a_scene_with_a_probe_drawn_last(self):
        with tempfile.TemporaryDirectory() as folder:
            with open(os.path.join(folder, 'scene.json'), 'w') as f:
                json.dump({'general': {'orthogonalprojection': {'width': 2560, 'height': 1440}},
                           'objects': [{'id': 7, 'name': 'layer'}]}, f)
            probe_tool.stamp(folder)
            probe_tool.stamp(folder)  # stamping again replaces the probe
            with open(os.path.join(folder, 'scene.json')) as f:
                objects = json.load(f)['objects']
            self.assertEqual([o['name'] for o in objects], ['layer', 'owe clock probe'])
            self.assertEqual(objects[-1]['image'], 'models/util/fullscreenlayer.json')
            with open(os.path.join(folder, 'compare.json')) as f:
                self.assertTrue(json.load(f)['probe'])
            for path in ('effects/oweclock/effect.json', 'materials/effects/oweclock.json',
                         'shaders/effects/oweclock.vert', 'shaders/effects/oweclock.frag'):
                self.assertTrue(os.path.exists(os.path.join(folder, path)), path)

    def test_stamps_a_package_inside_it(self):
        # Wallpaper Engine doesn't draw every unpacked Workshop wallpaper alike (Dino Run lost its background):
        # a packaged scene stays packaged, its files kept
        with tempfile.TemporaryDirectory() as folder:
            files = {'scene.json': json.dumps({'objects': [{'id': 3, 'name': 'layer'}]}).encode(),
                     'models/a.json': b'{"x": 1}'}
            with open(os.path.join(folder, 'scene.pkg'), 'wb') as f:
                f.write(probe_tool.pack(files, b'PKGV0024'))
            probe_tool.stamp(folder)
            self.assertFalse(os.path.exists(os.path.join(folder, 'scene.json')))
            version, entries = probe_tool.unpack(os.path.join(folder, 'scene.pkg'))
            self.assertEqual(version, b'PKGV0024')
            self.assertEqual(entries['models/a.json'], b'{"x": 1}')
            objects = json.loads(entries['scene.json'])['objects']
            self.assertEqual([o['name'] for o in objects], ['layer', 'owe clock probe'])
            self.assertIn('shaders/effects/oweclock.frag', entries)
            self.assertIn('effects/oweclock/effect.json', entries)

    def test_stamps_the_scene_project_json_names(self):
        with tempfile.TemporaryDirectory() as folder:
            with open(os.path.join(folder, 'project.json'), 'w') as f:
                json.dump({'file': 'car.json'}, f)
            with open(os.path.join(folder, 'car.pkg'), 'wb') as f:
                f.write(probe_tool.pack({'car.json': json.dumps({'objects': []}).encode()}, b'PKGV0024'))
            probe_tool.stamp(folder)
            _, entries = probe_tool.unpack(os.path.join(folder, 'car.pkg'))
            self.assertEqual(json.loads(entries['car.json'])['objects'][-1]['name'], 'owe clock probe')
            self.assertNotIn('scene.json', entries)

    def test_reads_the_frame_a_picture_shows(self):
        for which in (0, 1):
            for frame in (0, 1, 350, 2731, 4095):
                self.assertEqual(probe_tool.read(draw_probe(frame, which=which)), frame)
        self.assertEqual(probe_tool.read(draw_probe(82, size=(1920, 1080))), 82)
        # no probe: the markers don't read
        self.assertIsNone(probe_tool.read(Image.new('RGB', (1600, 900))))

    def test_reads_the_scripts_frame(self):
        # scripts run on a clock of their own (they start after loading in Wallpaper Engine)
        for which in (0, 1):
            picture = draw_probe(400, which=which, script=362)
            self.assertEqual(probe_tool.read(picture), 400)
            self.assertEqual(probe_tool.read_script(picture), 362)
        # scripts that haven't run yet
        self.assertEqual(probe_tool.read_script(draw_probe(120, script=None)), probe_tool.NOT_STARTED)
        self.assertIsNone(probe_tool.read_script(Image.new('RGB', (1600, 900))))

    def test_stamps_a_script_that_writes_its_frame(self):
        with tempfile.TemporaryDirectory() as folder:
            with open(os.path.join(folder, 'scene.json'), 'w') as f:
                json.dump({'objects': []}, f)
            probe_tool.stamp(folder)
            with open(os.path.join(folder, 'scene.json')) as f:
                probe = json.load(f)['objects'][-1]
            value = probe['effects'][0]['passes'][0]['constantshadervalues']['scriptframe']
            self.assertEqual(value['value'], -1)
            self.assertIn('engine.runtime', value['script'])
            with open(os.path.join(folder, 'shaders/effects/oweclock.frag')) as f:
                self.assertIn('u_ScriptFrame', f.read())

    def test_reads_the_inputs_the_shaders_saw(self):
        for flipped in (False, True):
            inputs = probe_tool.read_inputs(inputs_picture(flipped=flipped))
            self.assertEqual(inputs['pointer'], (0.25, 1.5))
            self.assertEqual(inputs['eye'], (12.5, -3.25, 800.0))
            self.assertEqual(inputs['levels'], {16: (0.0, 0.0), 32: (0.0, 0.0), 64: (1.0, 128 / 255)})
            self.assertEqual(inputs['flipped'], flipped)   # which side the records are on (left out of comparisons)

    def test_reads_through_a_bright_bloom(self):
        # Demon Core's yellow bloom lifts a black cell to (140, 55, 4); white stays white: each row's markers say
        # where a channel's one starts
        picture = inputs_picture()
        bloomed = picture.copy()
        pixels = bloomed.load()
        source = picture.load()
        for x in range(picture.size[0]):
            for y in range(picture.size[1]):
                pixels[x, y] = tuple(255 if s == 255 else min(255, s + g)
                                     for s, g in zip(source[x, y], (150, 70, 40)))
        inputs = probe_tool.read_inputs(bloomed)
        self.assertEqual(inputs['pointer'], (0.25, 1.5))
        self.assertEqual(inputs['timezone'], 300)

    def test_reads_what_the_scripts_saw(self):
        inputs = probe_tool.read_inputs(inputs_picture())
        self.assertEqual(inputs['press'], 40)   # stored one more than the frame: 0 is no press
        # 2493 days after 2020-01-01 (2026-10-28) at 09:00:12.34 UTC, 300 minutes behind UTC
        self.assertEqual(inputs['date'], (18262 + 2493) * 86400000 + 9 * 3600000 + 12340)
        self.assertEqual(inputs['timezone'], 300)
        self.assertEqual(inputs['camera'], (12.5, None, 800.0))   # all ones: the script couldn't tell
        before = probe_tool.read_inputs(inputs_picture(script_started=False))
        self.assertEqual(before['scripts'], probe_tool.NOT_STARTED)
        self.assertNotIn('date', before)
        self.assertEqual(before['pointer'], (0.25, 1.5))

    def test_reads_the_audio_bands(self):
        values = {}
        for resolution in (16, 64):
            for side in 'lr':
                for index in range(resolution):
                    values['band%d%s%d' % (resolution, side, index)] = (index * 7 + (side == 'r')) % 256
        inputs = probe_tool.read_inputs(inputs_picture(bands=(16, 64), band_values=values), bands=(16, 64))
        self.assertEqual(sorted(inputs['bands']), [16, 64])
        self.assertEqual(inputs['bands'][64]['left'][3], 21 / 255)
        self.assertEqual(inputs['bands'][16]['right'][2], 15 / 255)
        self.assertEqual(len(inputs['bands'][64]['right']), 64)

    def test_no_records_read_as_nothing(self):
        self.assertEqual(probe_tool.read_inputs(Image.new('RGB', (1600, 900))), {})

    def test_records_stay_small(self):
        # the records are left out of the comparison: they mustn't hide much of the picture
        size = (1600, 900)
        area = sum((r - l) * (b - t) for l, t, r, b in probe_tool.probe_boxes(size))
        self.assertLess(area / (size[0] * size[1]), 0.05)

    def test_stamps_the_inputs_records(self):
        with tempfile.TemporaryDirectory() as folder:
            with open(os.path.join(folder, 'scene.json'), 'w') as f:
                json.dump({'objects': []}, f)
            probe_tool.stamp(folder)
            with open(os.path.join(folder, 'shaders/effects/oweclock.frag')) as f:
                shader = f.read()
            for uniform in ('g_PointerPosition', 'g_EyePosition', 'g_AudioSpectrum64Left'):
                self.assertIn(uniform, shader)
            # Wallpaper Engine's HLSL has no one-scalar vector constructors
            self.assertIsNone(re.search(r'vec[234]\([^,()]*(\([^()]*\))?[^,()]*\)', shader))
            with open(os.path.join(folder, 'scene.json')) as f:
                constants = json.load(f)['objects'][-1]['effects'][0]['passes'][0]['constantshadervalues']
            self.assertIn('input.cursorLeftDown', constants['press']['script'])
            self.assertIn('getCameraTransforms', constants['camerax']['script'])
            self.assertIn('getTimezoneOffset', constants['timezone']['script'])
            with open(os.path.join(folder, 'compare.json')) as f:
                self.assertNotIn('probe_bands', json.load(f))

    def test_stamps_the_bands_of_a_wallpaper_that_comes_with_a_sound(self):
        with tempfile.TemporaryDirectory() as folder:
            with open(os.path.join(folder, 'scene.json'), 'w') as f:
                json.dump({'objects': []}, f)
            os.makedirs(os.path.join(folder, 'scripts'))
            with open(os.path.join(folder, 'scripts', 'bars.js'), 'w') as f:
                f.write('const audio = engine.registerAudioBuffers(engine.AUDIO_RESOLUTION_64);\n')
            open(os.path.join(folder, 'audio.wav'), 'wb').close()
            probe_tool.stamp(folder)
            probe_tool.stamp(folder)   # the probe's own shader reads every band: it doesn't count
            with open(os.path.join(folder, 'compare.json')) as f:
                self.assertEqual(json.load(f)['probe_bands'], [64])
            with open(os.path.join(folder, 'shaders/effects/oweclock.frag')) as f:
                self.assertIn('g_AudioSpectrum64Right[', f.read())

    @unittest.skipUnless(shutil.which('node'), 'needs node')
    def test_the_scripts_parse(self):
        # a script that doesn't parse leaves its field at 0 in owe (and who knows what in Wallpaper Engine)
        with tempfile.TemporaryDirectory() as folder:
            for name, script in [('scriptframe', probe_tool.SCRIPT)] + list(probe_tool.FIELD_SCRIPTS.items()):
                path = os.path.join(folder, name + '.mjs')
                with open(path, 'w') as f:
                    f.write(script)
                result = subprocess.run(['node', '--check', path], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, name + ': ' + result.stderr)

    def test_stamps_the_bands_of_every_wallpaper_that_listens(self):
        # Wallpaper Engine hears something in the VM even with nothing playing (Demon Core's bands read 1.0)
        with tempfile.TemporaryDirectory() as folder:
            with open(os.path.join(folder, 'scene.json'), 'w') as f:
                json.dump({'objects': []}, f)
            os.makedirs(os.path.join(folder, 'shaders'))
            with open(os.path.join(folder, 'shaders', 'core.vert'), 'w') as f:
                f.write('uniform float g_AudioSpectrum16Left[16];\n')
            probe_tool.stamp(folder)
            with open(os.path.join(folder, 'compare.json')) as f:
                self.assertEqual(json.load(f)['probe_bands'], [16])

    def test_stamps_both_sides_when_scripts_make_layers(self):
        # layers a script creates are drawn after the probe (the audio tests' bars covered its records)
        with tempfile.TemporaryDirectory() as folder:
            with open(os.path.join(folder, 'scene.json'), 'w') as f:
                json.dump({'objects': [{'id': 1, 'name': 'bars', 'origin': {
                    'value': '0 0 0', 'script': 'thisScene.createLayer("x");'}}]}, f)
            probe_tool.stamp(folder)
            with open(os.path.join(folder, 'compare.json')) as f:
                self.assertTrue(json.load(f)['probe_mirrored'])
            with open(os.path.join(folder, 'shaders/effects/oweclock.frag')) as f:
                self.assertIn('row < 22.0', f.read())

    def test_reads_the_side_that_is_not_covered(self):
        picture = inputs_picture()
        # the same records beside the top strips, and the bottom ones covered by a bar
        top = inputs_picture(flipped=True)
        for kind in ('shader', 'script'):
            for box in probe_tool.record_rows(picture.size, kind, True):
                picture.paste(top.crop(tuple(int(v) for v in box)), tuple(int(v) for v in box[:2]))
        ImageDraw.Draw(picture).rectangle([0, 30 * 900 // 45, 1600, 40 * 900 // 45], fill=(50, 160, 255))
        inputs = probe_tool.read_inputs(picture)
        self.assertEqual(inputs['pointer'], (0.25, 1.5))
        self.assertEqual(inputs['timezone'], 300)

    def test_takes_each_bit_from_the_side_a_bar_does_not_cross(self):
        # the audio tests' bars cross a cell here and there on either side: each cell is read where its colour is
        # plainer (a bar is light blue, half way in green)
        picture = inputs_picture()
        top = inputs_picture(flipped=True)
        for kind in ('shader', 'script'):
            for box in probe_tool.record_rows(picture.size, kind, True):
                picture.paste(top.crop(tuple(int(v) for v in box)), tuple(int(v) for v in box[:2]))
        draw = ImageDraw.Draw(picture)
        for x in (200, 560):
            draw.rectangle([x, 450, x + 12, 900], fill=(50, 160, 255))
        for x in (380, 740):
            draw.rectangle([x, 0, x + 12, 450], fill=(50, 160, 255))
        inputs = probe_tool.read_inputs(picture, mirrored=True)
        self.assertEqual(inputs['pointer'], (0.25, 1.5))
        self.assertEqual(inputs['eye'], (12.5, -3.25, 800.0))
        self.assertEqual(inputs['levels'][64], (1.0, 128 / 255))
        self.assertEqual(inputs['date'], (18262 + 2493) * 86400000 + 9 * 3600000 + 12340)

    def test_reads_one_side_of_a_probe_drawn_on_one(self):
        # beach's scene, where a record would be beside the top strips, happens to look like one: a probe drawn on
        # one side is read from the first side that reads, never mixed
        picture = inputs_picture()
        # a little glow over the real record's cells, none over the look-alike
        left, top, right, bottom = probe_tool.record_rows(picture.size, 'script')[0]
        cell = (right - left) / probe_tool.ROW_CELLS
        pixels = picture.load()
        payload = -(-sum(width for _, width in probe_tool.SCRIPT_FIELDS) // 3)
        for x in range(int(left + cell), int(left + (payload + 1) * cell)):
            for y in range(int(top), int(bottom)):
                pixels[x, y] = tuple(min(255, v + 40) for v in pixels[x, y])
        draw_record(picture, probe_tool.record_rows(picture.size, 'script', True), probe_tool.SCRIPT_FIELDS,
                    {name: (1 << width) - 2 for name, width in probe_tool.SCRIPT_FIELDS})
        inputs = probe_tool.read_inputs(picture)
        self.assertEqual(inputs['press'], 40)
        self.assertEqual(inputs['timezone'], 300)


if __name__ == '__main__':
    unittest.main()
