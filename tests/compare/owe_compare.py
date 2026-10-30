#!/usr/bin/env python3
"""Compares open-wallpaper-engine's renders of the test wallpapers with the real Wallpaper Engine's pictures.

    tests/compare/.venv/bin/python tests/compare/owe_compare.py [wallpaper ...] [--viewer PATH]

Every folder of wallpapers/ with a reference.png (a picture the real Wallpaper Engine took of it at 1920x1080) is
rendered with SceneViewer at the reference's size and compared with it. Name wallpapers to compare only those (also
folders below, e.g. _scratch/arsenal: _scratch/ is not committed, for wallpapers that aren't ours).

The viewer renders the frame the reference shows offscreen, without a display (--capture): frame 350 at 30 a second
(11.7 s, when the references are taken), or the "frames" of the wallpaper's compare.json. owe's clock advances one
frame time a frame, so loading time doesn't matter and the frame is exact.
A wallpaper stamped with clock-probe.py records the frame its reference shows; the viewer renders that frame
instead, owe's probe must read the same, and the probe's strip is left out of the scores (the report's "probe").
The probe's second strip shows the scripts' frame: scripts read g_Time, but a heavy wallpaper's may not have run yet
(the viewer then starts them after the frame, --script-start). Wallpaper Engine also starts camera paths later than
g_Time, by about what that load took; a compare.json's "camera_path_delay" (frames, or "search" to find it each run)
delays the viewer's (--camera-path-delay). "search" first looks in one run's --camera-eye-log for the moment owe's
camera stands where the probe's eye says Wallpaper Engine's was, and else scores pictures at one delay after another.
The probe's records give the viewer the rest of what Wallpaper Engine gave the scene: the pointer (--mouse-position),
the spectrum (--audio-spectrum, for a wallpaper stamped with its bands), and the scripts' date and time zone
(--script-clock, TZ).
A compare.json's "shadow_quality" (low, medium, high, ultra) is the Wallpaper Engine "shadows" setting its reference
was taken at (--shadow-quality). A viewer without one of these options renders without it.
Video wallpapers are skipped (the viewer draws scenes), and so are those with an audio.wav whose bands weren't
recorded: their bars follow a test sound the viewer does not hear.

Each wallpaper scores:
  ssim      structural similarity, 1 is identical
  mae       mean absolute difference per channel, 0 to 255
  changed   share of pixels whose largest channel differs by more than --tolerance
  regions   areas that differ, at least 5 pixels across
and is "ok" with an ssim of at least --threshold and no region, or "near" when it is at least as close to the
reference as a second take of it (reference-2.png) taken within 3 frames of it: Wallpaper Engine's own randomness.
Takes further apart are only reported (what moved between them counts too). Pictures go to output-owe/<name>/:
reference.png, owe.png, diff.png and side-by-side.png (reference | owe | difference, changed pixels in red), with
the viewer's log in engine/viewer.log; report.json has every score. A wallpaper's compare.json "bar_jitter" (pixels)
leaves out regions no wider than that: script-driven audio bars read the sound a moment after the probe recorded it.

Needs: SceneViewer (--viewer, default the debug build: lito build -p owe-sceneviewer), Wallpaper Engine's assets
(WPE_ASSETS, default tests/assets) and the venv of tests/compare/ (python3 -m venv .venv &&
.venv/bin/pip install -r requirements.txt).
"""
import argparse
import functools
import importlib.util
import json
import math
import os
import subprocess
import sys

import numpy as np
from PIL import Image
from skimage.measure import label, regionprops
from skimage.metrics import structural_similarity
from skimage.morphology import footprint_rectangle, opening

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
WALLPAPERS = os.path.join(HERE, 'wallpapers')
OUTPUT = os.path.join(HERE, 'output-owe')
VIEWER = os.path.join(REPO, 'build', 'debug', 'bin', 'owe-sceneviewer', 'SceneViewer')
# Wallpaper Engine's assets: the tests' assets folder (a link to them) unless WPE_ASSETS says otherwise
ASSETS = os.environ.get('WPE_ASSETS', os.path.join(REPO, 'tests', 'assets'))
# the reference is taken when the wallpaper has run for about 11.7 s (350 frames at 30 a second)
REFERENCE_FRAMES = 350

# a wallpaper stamped with clock-probe.py records the frame its pictures show
_spec = importlib.util.spec_from_file_location('clock_probe', os.path.join(HERE, 'clock-probe.py'))
clock_probe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(clock_probe)


def settings_of(name):
    path = os.path.join(WALLPAPERS, name, 'compare.json')
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        return json.load(f)


def probe_boxes(size, inputs=None, bands=(), mirrored=False):
    """What the probe draws over in a picture of `size`, a little larger (left, top, right, bottom): its strips, and
    its records on the side `inputs` (clock_probe.read_inputs) found them."""
    boxes = []
    for left, top, right, bottom in clock_probe.probe_boxes(size, bool((inputs or {}).get('flipped')), bands,
                                                            mirrored):
        margin = (bottom - top) * 0.5
        boxes.append((max(int(left - margin), 0), max(int(top - margin), 0), min(int(right + margin) + 1, size[0]),
                      min(int(bottom + margin) + 1, size[1])))
    return boxes


def skipped(name):
    """Why the wallpaper is left out, or None."""
    with open(os.path.join(WALLPAPERS, name, 'project.json')) as f:
        kind = json.load(f).get('type', 'scene').lower()
    if kind != 'scene':
        return 'a %s wallpaper' % kind
    if os.path.exists(os.path.join(WALLPAPERS, name, 'audio.wav')) and \
            not settings_of(name).get('probe_bands'):
        return 'its bars follow a test sound the viewer does not hear (stamp it to record the bands)'
    return None


@functools.cache
def viewer_help(viewer):
    result = subprocess.run([viewer, '--help'], capture_output=True, text=True, check=False)
    return result.stdout + result.stderr


def has_capture(viewer):
    """Whether the viewer can render a frame offscreen (--capture)."""
    return '--capture' in viewer_help(viewer)


def scene_package(folder):
    """The package the viewer is given: named after project.json's "file" (X.json: X.pkg), which it reads as the
    scene; without the package it mounts the folder itself."""
    with open(os.path.join(folder, 'project.json')) as f:
        scene_file = json.load(f).get('file', 'scene.json')
    return os.path.join(folder, os.path.splitext(scene_file)[0] + '.pkg')


def shader_cache(viewer):
    """A shader cache of the viewer's own: owe's cache keys don't cover the shader code the engine injects, so a
    cache from another build can hand back stale shaders."""
    stat = os.stat(viewer)
    return os.path.join(OUTPUT, 'shader-cache', '%d-%d' % (stat.st_mtime_ns, stat.st_size))


def render(name, size, viewer, frames=None, script_start=0.0, camera_path_delay=0.0, inputs=(), env_extra=None):
    """Renders the wallpaper offscreen with SceneViewer --capture at `size` (at `frames`, or its compare.json's or
    the usual moment; scripts from `script_start` seconds, camera paths `camera_path_delay` seconds behind, the
    viewer given the reference's `inputs` arguments and environment `env_extra`) and returns the picture's path."""
    out = os.path.join(OUTPUT, name, 'engine')
    if frames is None:
        frames = settings_of(name).get('frames', REFERENCE_FRAMES)
    # in a memory cgroup of its own, so a viewer that runs away is killed, not the machine
    capped = ['systemd-run', '--user', '--scope', '-q', '-p', 'MemoryMax=' + os.environ.get('WPE_TEST_MEMORY', '3G'),
              '-p', 'MemorySwapMax=0']
    os.makedirs(out, exist_ok=True)
    frame = os.path.join(out, 'frame.ppm')
    if os.path.exists(frame):
        os.remove(frame)
    env = {k: v for k, v in os.environ.items() if k not in ('WAYLAND_DISPLAY', 'DISPLAY')}
    env.update(env_extra or {})
    # the "shadows" setting the reference was taken at (medium by default), for a viewer that has the setting
    shadows = ['--shadow-quality', settings_of(name)['shadow_quality']] \
        if 'shadow_quality' in settings_of(name) and '--shadow-quality' in viewer_help(viewer) else []
    command = capped + ['timeout', '900', viewer, '-C', shader_cache(viewer), '--resolution', '%dx%d' % size,
                        '--fps', '30', '--capture', frame, '--capture-frame', str(frames)] + (
                           ['--script-start', '%.4f' % script_start] if script_start > 0 else []) + (
                           ['--camera-path-delay', '%.4f' % camera_path_delay] if camera_path_delay > 0
                           else []) + shadows + list(inputs) + [ASSETS, scene_package(os.path.join(WALLPAPERS, name))]
    with open(os.path.join(out, 'viewer.log'), 'w') as log:
        subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=False)
    if not os.path.exists(frame):
        raise RuntimeError('no picture (see %s/viewer.log)' % out)
    return frame


def input_arguments(inputs, frames, help_text, folder):
    """The viewer arguments and environment that give owe what a reference's clock probe recorded (`inputs`, from
    clock_probe.read_inputs, at frame `frames`), as far as the viewer (its `help_text`) takes them, and notes on
    what it can't be given. A spectrum is written to spectrum.json in `folder`."""
    arguments, env, notes = [], {}, []
    if 'pointer' in inputs and '--mouse-position' in help_text:
        arguments += ['--mouse-position', '%.4f,%.4f' % tuple(inputs['pointer'])]
    if 'bands' in inputs:
        if '--audio-spectrum' in help_text:
            path = os.path.join(folder, 'spectrum.json')
            with open(path, 'w') as f:
                json.dump({str(resolution): sides for resolution, sides in inputs['bands'].items()}, f)
            arguments += ['--audio-spectrum', path]
        else:
            notes.append('the viewer has no --audio-spectrum: it hears nothing')
    if inputs.get('scripts') == 'started' and 'date' in inputs:
        if '--script-clock' in help_text:
            # the scripts' clock as the scene starts, `frames` before the date they saw
            arguments += ['--script-clock', '%d' % round(inputs['date'] - frames * 1000 / 30)]
            # getTimezoneOffset() counts minutes behind UTC, as POSIX's TZ counts hours
            minutes = inputs.get('timezone', 0)
            env['TZ'] = 'OWE%s%02d:%02d' % ('+' if minutes >= 0 else '-', abs(minutes) // 60, abs(minutes) % 60)
        else:
            notes.append('the viewer has no --script-clock: scripts see this machine\'s clock')
    if inputs.get('press') is not None:
        notes.append('the reference\'s scripts saw a left press at frame %d, which owe isn\'t given' % inputs['press'])
    return arguments, env, notes


def camera_delay_from_eye_log(log, eye, frames):
    """The camera path delay (seconds) at which owe's camera, as its --camera-eye-log shows (lines "t x y z"),
    stands where the reference's eye was at frame `frames`, and how far off that is; None without a log. Of eyes as
    near (a camera that holds still), the shortest delay."""
    best = None
    for line in log.splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        time, x, y, z = (float(part) for part in parts)
        if time > frames / 30 + 1e-6:
            continue
        distance = math.dist((x, y, z), eye)
        # owe draws whole frames of 1/30 s
        delay = max(round((frames / 30 - time) * 30), 0) / 30
        # the reference's eye is kept in 1/256ths: nearer than half of that is as near
        if best is None or distance < best[1] - 1 / 512 or (distance <= best[1] + 1 / 512 and delay < best[0]):
            best = (delay, min(distance, best[1]) if best else distance)
    return best


def camera_delays_near_eye(log, eye, frames):
    """The camera path delays (seconds) whose eye, in owe's --camera-eye-log, the reference's could have been: it is
    kept in 1/256ths, so on a slow path several frames' eyes round alike (the nearest isn't the likeliest). A camera
    that holds still leaves only the shortest delay: its pictures are all alike."""
    nearest = camera_delay_from_eye_log(log, eye, frames)
    if nearest is None:
        return []
    delays = set()
    for line in log.splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        time, x, y, z = (float(part) for part in parts)
        if time > frames / 30 + 1e-6:
            continue
        # each coordinate is off by up to half of 1/256
        if math.dist((x, y, z), eye) <= nearest[1] + math.sqrt(3) / 512:
            delays.add(max(round((frames / 30 - time) * 30), 0))
    if max(delays) - min(delays) > 15:
        return [nearest[0]]
    return [delay / 30 for delay in sorted(delays)]


def eye_camera_delay(name, size, viewer, frames, script_start, inputs, env, eye):
    """The camera path delays (seconds) at which owe's camera stands where the reference's eye was, from one run's
    --camera-eye-log (camera_delays_near_eye); None when no moment comes near it."""
    log = os.path.join(OUTPUT, name, 'camera-eye.log')
    if os.path.exists(log):
        os.remove(log)
    render(name, size, viewer, frames, script_start, 0.0, list(inputs) + ['--camera-eye-log', log], env)
    if not os.path.exists(log):
        return None
    with open(log) as f:
        text = f.read()
    found = camera_delay_from_eye_log(text, eye, frames)
    if found is None or found[1] > 0.05:
        print('     %-28s no moment of owe\'s camera comes near the reference\'s eye %s%s' % (
            name, tuple(round(v, 3) for v in eye), ' (nearest %.3f off)' % found[1] if found else ''), flush=True)
        return None
    delays = camera_delays_near_eye(text, eye, frames)
    print('     %-28s camera paths %s frames behind (its eye %.4f off)' % (
        name, '%d' % round(found[0] * 30) if len(delays) == 1 else
        '%d to %d' % (round(delays[0] * 30), round(delays[-1] * 30)), found[1]), flush=True)
    return delays


def without_bar_jitter(regions, bar_width):
    """The regions wider than `bar_width` pixels (compare.json "bar_jitter"): a script-driven bar can read the audio
    buffer a moment after the probe recorded it, so a region no wider than one bar is not a difference."""
    return [r for r in regions if r['width'] > bar_width]


def near_floor(ssim, floor, frames_apart=3):
    """Whether owe (`ssim`) is at least as close to the reference as a second take of it (`floor`: its 'ssim' and
    both takes' 'frames') taken at about the same moment: further apart, what moved between them counts too."""
    if not floor or None in floor['frames'] or abs(floor['frames'][0] - floor['frames'][1]) > frames_apart:
        return False
    return ssim >= floor['ssim']


def score_camera_delay(name, reference, viewer, frames, script_start, tolerance, delay, inputs=(), env=None,
                       masked=()):
    """How like the reference owe's picture is with camera paths `delay` seconds behind."""
    frame = render(name, reference.size, viewer, frames, script_start, delay, inputs, env)
    return compare(reference, Image.open(frame), tolerance, masked)[0]['ssim']


def best_camera_delay(name, reference, viewer, frames, script_start, tolerance, delays, inputs=(), env=None,
                      masked=None):
    """Of `delays` (seconds), the one whose picture matches the reference best."""
    if len(delays) == 1:
        return delays[0]
    masked = masked if masked is not None else probe_boxes(reference.size)
    scores = {delay: score_camera_delay(name, reference, viewer, frames, script_start, tolerance, delay, inputs, env,
                                        masked) for delay in delays}
    best = max(scores, key=scores.get)
    print('     %-28s of those, %d frames behind (ssim %.4f)' % (name, round(best * 30), scores[best]), flush=True)
    return best


def search_camera_delay(name, reference, viewer, frames, script_start, tolerance, inputs=(), env=None,
                        masked=None):
    """The camera path delay (seconds) whose picture matches the reference best: every 15 frames up to 5 s,
    then every 2 frames around the best."""
    masked = masked if masked is not None else probe_boxes(reference.size)

    def score(delay_frames):
        return score_camera_delay(name, reference, viewer, frames, script_start, tolerance, delay_frames / 30,
                                  inputs, env, masked)

    scores = {d: score(d) for d in range(0, 151, 15)}
    best = max(scores, key=scores.get)
    for d in range(max(best - 12, 0), best + 13, 2):
        if d not in scores:
            scores[d] = score(d)
    best = max(scores, key=scores.get)
    print('     %-28s camera paths %d frames behind (ssim %.4f)' % (name, best, scores[best]), flush=True)
    return best / 30


def compare(reference, owe, tolerance, masked=()):
    """Scores owe's picture against the reference, leaving out the `masked` rectangles (the clock probe)."""
    a = np.asarray(reference.convert('RGB'), dtype=np.int16)
    b = np.asarray(owe.convert('RGB'), dtype=np.int16).copy()
    for left, top, right, bottom in masked:
        b[top:bottom, left:right] = a[top:bottom, left:right]
    difference = np.abs(a - b)
    changed = difference.max(axis=2) > tolerance
    score = structural_similarity(a.astype(np.uint8), b.astype(np.uint8), channel_axis=2)
    # edges differ by a pixel between any two renderers: only areas a few pixels across count
    areas = opening(changed, footprint_rectangle((5, 5)))
    regions = []
    for region in regionprops(label(areas, connectivity=2)):
        if region.area >= 20:
            top, left, bottom, right = region.bbox
            regions.append({'x': int(left), 'y': int(top), 'width': int(right - left), 'height': int(bottom - top),
                            'pixels': int(region.area)})
    regions.sort(key=lambda r: -r['pixels'])
    metrics = {'ssim': float(score), 'mae': float(difference.mean()), 'changed': float(changed.mean()),
               'changed_areas': float(areas.mean()), 'regions': regions}
    return metrics, difference, changed


def save_pictures(name, reference, owe, difference, changed):
    folder = os.path.join(OUTPUT, name)
    shown = np.clip(difference * 4, 0, 255).astype(np.uint8)
    shown[changed] = [255, 0, 0]
    pictures = [reference.convert('RGB'), owe.convert('RGB'), Image.fromarray(shown)]
    for picture, file in zip(pictures, ('reference.png', 'owe.png', 'diff.png')):
        picture.save(os.path.join(folder, file))
    width, height = reference.size
    sheet = Image.new('RGB', (width * 3, height))
    for i, picture in enumerate(pictures):
        sheet.paste(picture, (i * width, 0))
    sheet.save(os.path.join(folder, 'side-by-side.png'))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('wallpapers', nargs='*', help='names of the wallpapers (all with a reference.png by default)')
    parser.add_argument('--viewer', default=os.environ.get('OWE_VIEWER', VIEWER), help='the SceneViewer binary')
    parser.add_argument('--threshold', type=float, default=0.98, help='lowest acceptable ssim')
    parser.add_argument('--tolerance', type=int, default=24, help='channel difference that counts as a changed pixel')
    parser.add_argument('--no-render', action='store_true', help='compare the pictures of the last run again')
    args = parser.parse_args()

    if not args.no_render:
        if not has_capture(args.viewer):
            parser.error('%s has no --capture: build owe-sceneviewer (lito build -p owe-sceneviewer)' % args.viewer)
        print('viewer: %s' % args.viewer, flush=True)
    names = args.wallpapers or sorted(
        n for n in (os.listdir(WALLPAPERS) if os.path.isdir(WALLPAPERS) else ())
        if os.path.exists(os.path.join(WALLPAPERS, n, 'reference.png')))
    results, counts = {}, {'ok': 0, 'near': 0, 'differ': 0, 'failed': 0, 'skipped': 0}
    help_text = '' if args.no_render else viewer_help(args.viewer)
    for name in names:
        why = skipped(name)
        if why:
            print('skip %-28s %s' % (name, why), flush=True)
            counts['skipped'] += 1
            continue
        os.makedirs(os.path.join(OUTPUT, name), exist_ok=True)
        reference = Image.open(os.path.join(WALLPAPERS, name, 'reference.png'))
        probe = settings_of(name).get('probe')
        bands = settings_of(name).get('probe_bands', ())
        frames, stamp, script_start = None, {}, 0.0
        inputs, input_args, input_env = {}, [], {}
        # Wallpaper Engine starts camera paths later than g_Time, by about what the wallpaper took to load: no
        # picture shows a camera path's clock, so a compare.json gives it in frames, or "search" finds it
        camera_setting = settings_of(name).get('camera_path_delay', 0)
        search_camera = camera_setting == 'search'
        camera_path_delay = 0.0 if search_camera else camera_setting / 30
        if probe:
            frames = clock_probe.read(reference)
            stamp['reference'] = frames
            if frames is None:
                print('     %-28s the reference has no readable clock probe: the usual moment' % name, flush=True)
            else:
                # scripts read g_Time, but a heavy wallpaper's may not have started yet
                scripts = clock_probe.read_script(reference)
                stamp['reference_scripts'] = scripts
                if scripts == clock_probe.NOT_STARTED:
                    script_start = (frames + 1) / 30
                # what Wallpaper Engine gave the scene: the pointer, the spectrum, the scripts' clock, the eye
                inputs = clock_probe.read_inputs(reference, bands, settings_of(name).get('probe_mirrored', False))
                stamp['reference_inputs'] = {key: value for key, value in inputs.items() if key != 'bands'}
                if inputs and not args.no_render:
                    input_args, input_env, notes = input_arguments(
                        inputs, frames, help_text, os.path.join(OUTPUT, name))
                    for note in notes:
                        print('     %-28s %s' % (name, note), flush=True)
        if (script_start > 0 or camera_path_delay > 0) and not args.no_render and \
                '--script-start' not in help_text:
            print('     %-28s the viewer has no --script-start: scripts and camera paths on its clock' % name,
                  flush=True)
            script_start = camera_path_delay = 0.0
        try:
            if args.no_render:
                engine = os.path.join(OUTPUT, name, 'engine')
                frame = os.path.join(engine, 'frame.ppm')
            else:
                if search_camera and '--camera-path-delay' in help_text:
                    # the reference's eye says where its camera path was: one run finds that moment in owe's;
                    # without it, pictures at one delay after another are scored
                    found = None
                    masked = probe_boxes(reference.size, inputs, bands, settings_of(name).get('probe_mirrored', False))
                    if 'eye' in inputs and '--camera-eye-log' in help_text:
                        found = eye_camera_delay(name, reference.size, args.viewer, frames, script_start,
                                                 input_args, input_env, inputs['eye'])
                    camera_path_delay = best_camera_delay(
                        name, reference, args.viewer, frames, script_start, args.tolerance, found, input_args,
                        input_env, masked) if found else search_camera_delay(
                        name, reference, args.viewer, frames, script_start, args.tolerance, input_args, input_env,
                        masked)
                    stamp['camera_path_delay'] = round(camera_path_delay * 30)
                frame = render(name, reference.size, args.viewer, frames, script_start, camera_path_delay,
                               input_args, input_env)
            owe = Image.open(frame)
            if owe.size != reference.size:
                raise RuntimeError('sizes differ: reference %s, owe %s' % (reference.size, owe.size))
        except Exception as error:
            print('FAIL %-28s %s' % (name, error), flush=True)
            results[name] = {'error': str(error)}
            counts['failed'] += 1
            continue
        masked = ()
        if probe:
            stamp['owe'] = clock_probe.read(owe)
            stamp['owe_scripts'] = clock_probe.read_script(owe)
            masked = probe_boxes(reference.size, inputs, bands, settings_of(name).get('probe_mirrored', False))
            owe_inputs = clock_probe.read_inputs(owe, bands, settings_of(name).get('probe_mirrored', False))
            stamp['owe_inputs'] = {key: value for key, value in owe_inputs.items() if key != 'bands'}
            if stamp['owe_scripts'] != stamp.get('reference_scripts'):
                print('     %-28s owe\'s scripts read frame %s, the reference\'s %s' % (
                    name, stamp['owe_scripts'], stamp.get('reference_scripts')), flush=True)
            if stamp['owe'] != stamp['reference']:
                print('     %-28s owe\'s probe reads frame %s, the reference\'s %s' % (
                    name, stamp['owe'], stamp['reference']), flush=True)
        metrics, difference, changed = compare(reference, owe, args.tolerance, masked)
        if probe:
            metrics['probe'] = stamp
        # a second take (reference-2.png) shows how far Wallpaper Engine is from itself: randomness, and whatever
        # moved between the two takes' frames
        floor = None
        second = os.path.join(WALLPAPERS, name, 'reference-2.png')
        if os.path.exists(second):
            other = Image.open(second)
            if other.size == reference.size:
                floor = compare(reference, other, args.tolerance, masked)[0]
                metrics['floor'] = {'ssim': floor['ssim'], 'mae': floor['mae'], 'regions': len(floor['regions']),
                                    'frames': [frames, clock_probe.read(other) if probe else None]}
        if settings_of(name).get('bar_jitter'):
            metrics['regions'] = without_bar_jitter(metrics['regions'], settings_of(name)['bar_jitter'])
        save_pictures(name, reference, owe, difference, changed)
        results[name] = metrics
        ok = metrics['ssim'] >= args.threshold and not metrics['regions']
        near = not ok and near_floor(metrics['ssim'], metrics.get('floor'))
        counts['ok' if ok else 'near' if near else 'differ'] += 1
        print('%s %-28s ssim %.4f  mae %.2f  changed %.3f%%  regions %d%s' % (
            'ok  ' if ok else 'near' if near else 'DIFF', name, metrics['ssim'], metrics['mae'],
            metrics['changed'] * 100, len(metrics['regions']),
            '  (Wallpaper Engine against itself %.4f, frames %s and %s)' % (
                floor['ssim'], *metrics['floor']['frames']) if floor else ''), flush=True)

    os.makedirs(OUTPUT, exist_ok=True)
    with open(os.path.join(OUTPUT, 'report.json'), 'w') as f:
        json.dump(results, f, indent=2)
    print('\n%(ok)d ok, %(near)d near, %(differ)d differ, %(failed)d failed, %(skipped)d skipped' % counts)
    print('pictures and report.json: %s' % OUTPUT)
    return 0 if counts['differ'] == counts['failed'] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
