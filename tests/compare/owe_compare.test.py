#!/usr/bin/env python3
"""Tests owe_compare.py's handling of what a reference's clock probe recorded.

    tests/compare/.venv/bin/python3 tests/compare/owe_compare.test.py
"""
import importlib.util
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
spec = importlib.util.spec_from_file_location('owe_compare', os.path.join(HERE, 'owe_compare.py'))
owe_compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(owe_compare)

EVERY_FLAG = '--mouse-position --audio-spectrum --script-clock --camera-eye-log --script-start'


class Inputs(unittest.TestCase):
    def test_gives_the_viewer_what_the_reference_saw(self):
        inputs = {'pointer': (1.19, 1.16), 'scripts': 'started', 'date': 1793226531600, 'timezone': 420,
                  'bands': {16: {'left': [0.5] * 16, 'right': [0.25] * 16}}}
        with tempfile.TemporaryDirectory() as folder:
            arguments, env, notes = owe_compare.input_arguments(inputs, 351, EVERY_FLAG, folder)
            self.assertEqual(arguments[arguments.index('--mouse-position') + 1], '1.1900,1.1600')
            # the scripts' clock as the scene starts: the date the reference's scripts saw, 351 frames earlier
            self.assertEqual(arguments[arguments.index('--script-clock') + 1], '%d' % (1793226531600 - 11700))
            # 420 minutes behind UTC; POSIX counts the hours the other way round
            self.assertEqual(env['TZ'], 'OWE+07:00')
            spectrum = arguments[arguments.index('--audio-spectrum') + 1]
            with open(spectrum) as f:
                self.assertEqual(json.load(f), {'16': {'left': [0.5] * 16, 'right': [0.25] * 16}})
            self.assertEqual(notes, [])

    def test_a_time_zone_ahead_of_utc(self):
        _, env, _ = owe_compare.input_arguments({'scripts': 'started', 'date': 0, 'timezone': -330}, 0,
                                                EVERY_FLAG, tempfile.gettempdir())
        self.assertEqual(env['TZ'], 'OWE-05:30')

    def test_scripts_not_started_give_no_clock(self):
        arguments, env, _ = owe_compare.input_arguments({'pointer': (0.5, 0.5), 'scripts': 'not started'}, 79,
                                                        EVERY_FLAG, tempfile.gettempdir())
        self.assertNotIn('--script-clock', arguments)
        self.assertNotIn('TZ', env)

    def test_an_older_viewer_gets_what_it_takes(self):
        inputs = {'pointer': (0.25, 0.75), 'scripts': 'started', 'date': 5000, 'timezone': 0,
                  'bands': {64: {'left': [0.0] * 64, 'right': [0.0] * 64}}}
        arguments, env, notes = owe_compare.input_arguments(inputs, 30, '--mouse-position', tempfile.gettempdir())
        self.assertEqual(arguments, ['--mouse-position', '0.2500,0.7500'])
        self.assertEqual(len(notes), 2)   # no spectrum, no script clock

    def test_a_press_the_reference_saw_is_noted(self):
        _, _, notes = owe_compare.input_arguments({'scripts': 'started', 'press': 40, 'date': 0, 'timezone': 0},
                                                  60, EVERY_FLAG, tempfile.gettempdir())
        self.assertTrue(any('press' in note for note in notes))


class CameraDelay(unittest.TestCase):
    def test_finds_the_moment_whose_eye_the_reference_shows(self):
        # the camera moves along x by 0.1 a frame: the reference at frame 100 shows the eye of frame 58
        log = ''.join('%g %g 0 12\n' % (frame / 30, frame * 0.1) for frame in range(101))
        delay, distance = owe_compare.camera_delay_from_eye_log(log, (5.8, 0.0, 12.0), 100)
        self.assertAlmostEqual(delay, 42 / 30)
        self.assertAlmostEqual(distance, 0.0)

    def test_a_camera_that_holds_takes_the_shortest_delay(self):
        # still from frame 40 on: any delay up to 60 frames shows it, the shortest is the likeliest
        log = ''.join('%g %g 0 12\n' % (frame / 30, min(frame, 40) * 0.1) for frame in range(101))
        delay, _ = owe_compare.camera_delay_from_eye_log(log, (4.0, 0.0, 12.0), 100)
        self.assertAlmostEqual(delay, 0.0)

    def test_no_log_gives_nothing(self):
        self.assertIsNone(owe_compare.camera_delay_from_eye_log('', (0, 0, 0), 10))


class Floor(unittest.TestCase):
    def test_a_second_take_at_about_the_same_moment_is_a_floor(self):
        self.assertTrue(owe_compare.near_floor(0.95, {'ssim': 0.94, 'frames': [300, 302]}))
        self.assertFalse(owe_compare.near_floor(0.93, {'ssim': 0.94, 'frames': [300, 302]}))

    def test_a_second_take_at_another_moment_is_not(self):
        # the two takes' difference is then mostly what moved between them
        self.assertFalse(owe_compare.near_floor(0.98, {'ssim': 0.79, 'frames': [227, 278]}))
        self.assertFalse(owe_compare.near_floor(0.98, {'ssim': 0.79, 'frames': [227, None]}))
        self.assertFalse(owe_compare.near_floor(0.98, None))


class Layout(unittest.TestCase):
    def test_finds_the_wallpapers_viewer_and_assets_in_the_repository(self):
        repo = os.path.dirname(os.path.dirname(HERE))
        self.assertEqual(owe_compare.WALLPAPERS, os.path.join(HERE, 'wallpapers'))
        self.assertEqual(owe_compare.VIEWER,
                         os.path.join(repo, 'build', 'debug', 'bin', 'owe-sceneviewer', 'SceneViewer'))
        if 'WPE_ASSETS' not in os.environ:
            self.assertEqual(owe_compare.ASSETS, os.path.join(repo, 'tests', 'assets'))


class Render(unittest.TestCase):
    def command(self, help_text, settings):
        """The viewer command render() runs for a viewer whose --help says `help_text`."""
        commands = []
        with tempfile.TemporaryDirectory() as folder, \
                mock.patch.object(owe_compare, 'OUTPUT', folder), \
                mock.patch.object(owe_compare, 'viewer_help', lambda viewer: help_text), \
                mock.patch.object(owe_compare, 'settings_of', lambda name: settings), \
                mock.patch.object(owe_compare, 'shader_cache', lambda viewer: 'cache'), \
                mock.patch.object(owe_compare, 'scene_package', lambda folder: 'scene.pkg'), \
                mock.patch.object(owe_compare.subprocess, 'run', lambda command, **kw: commands.append(command)):
            with self.assertRaises(RuntimeError):  # the fake viewer saves no picture
                owe_compare.render('w', (64, 32), 'viewer')
        return commands[0]

    def test_renders_the_frame_offscreen(self):
        command = self.command('--capture --capture-frame', {'frames': 93})
        self.assertEqual(command[command.index('--capture-frame') + 1], '93')
        self.assertEqual(command[command.index('--resolution') + 1], '64x32')

    def test_gives_the_shadows_setting_only_to_a_viewer_that_has_it(self):
        settings = {'shadow_quality': 'medium'}
        self.assertNotIn('--shadow-quality', self.command('--capture --capture-frame', settings))
        command = self.command('--capture --capture-frame --shadow-quality', settings)
        self.assertEqual(command[command.index('--shadow-quality') + 1], 'medium')


if __name__ == '__main__':
    unittest.main()
