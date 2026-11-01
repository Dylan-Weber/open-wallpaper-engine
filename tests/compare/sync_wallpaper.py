#!/usr/bin/env python3
"""Turns a wallpaper (synthetic, or a Workshop one with a scene.pkg) into a "synced" one: its image layers whose texture is a
sprite sheet are removed from the scene, and a script creates them again from their asset (createLayer(path), then sets the
layer's place, scale, angles, colour and alpha) at one known moment of the scene's clock. A created layer's sprite animation
starts when it is created, in both engines (showing a hidden layer does not restart it, texture-animation-shown), so the
animations start together at that moment, whatever time the assets took to load (Wallpaper Engine's sprite clock starts when
its texture is ready, owe's with the scene). The assets are registered first (engine.registerAsset) so they are loaded before
the moment.

    sync_wallpaper.py WALLPAPERS_DIR NAME [NAME ...] [--time 2.0] [--suffix sync]    writes WALLPAPERS_DIR/NAME-sync

Layers left as they are (reported): ones with a parent or children, with a visibility, origin, scale, angles, colour or alpha
that isn't a plain value (a user property or a script decides it), with effects (owe creates a layer from a configuration only
while the scene loads), and ones whose texture isn't a sprite sheet. A wallpaper with no sprite-sheet layer comes out
unchanged but for the name.
"""
import argparse
import json
import os
import shutil
import struct
import sys

ASSETS = os.environ.get('WPE_ASSETS', os.path.expanduser(
    '~/snap/steam/common/.local/share/Steam/steamapps/common/wallpaper_engine/assets'))


def unpack_pkg(path, out):
    """Writes the files of a scene.pkg (PKGV00xx) into `out`."""
    data = open(path, 'rb').read()
    pos = 0

    def string():
        nonlocal pos
        n = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        value = data[pos:pos + n]
        pos += n
        return value

    string()
    count = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    entries = []
    for _ in range(count):
        name = string().decode()
        offset, size = struct.unpack_from('<II', data, pos)
        pos += 8
        entries.append((name, offset, size))
    base = pos
    for name, offset, size in entries:
        target = os.path.join(out, name)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, 'wb') as f:
            f.write(data[base + offset:base + offset + size])


def read_json(folder, relative):
    path = os.path.join(folder, relative)
    if not os.path.exists(path):
        path = os.path.join(ASSETS, relative)
    if not os.path.exists(path):
        return None
    try:
        return json.load(open(path, encoding='utf-8-sig'))
    except ValueError:
        return None


def is_sprite_sheet(folder, name):
    """Whether the texture `name` (a material texture) is a .tex with an animation of several frames (a TEXS block)."""
    for root in (folder, ASSETS):
        path = os.path.join(root, 'materials', name + '.tex')
        if not os.path.exists(path):
            continue
        with open(path, 'rb') as f:
            data = f.read()
        i = data.rfind(b'TEXS')
        if i > 0 and i + 13 <= len(data):
            frames = struct.unpack_from('<I', data, i + 9)[0]
            return frames > 1
        return False
    return False


def image_texture(folder, image):
    model = read_json(folder, image)
    if not model or 'material' not in model:
        return None
    material = read_json(folder, model['material'])
    if not material:
        return None
    for p in material.get('passes', []):
        textures = p.get('textures') or []
        if textures and textures[0]:
            return textures[0]
    return None


def natural_size(folder, image, texture):
    """The size of a layer created from `image` by its path: the model's width and height, or (autosize) its texture's (a
    sprite sheet's frame)."""
    model = read_json(folder, image) or {}
    if 'width' in model and 'height' in model:
        return float(model['width']), float(model['height'])
    for root in (folder, ASSETS):
        path = os.path.join(root, 'materials', texture + '.tex')
        if os.path.exists(path):
            data = open(path, 'rb').read()
            i = data.rfind(b'TEXS')
            if i > 0:
                frames, width, height = struct.unpack_from('<III', data, i + 9)
                if frames > 1 and width > 0 and height > 0:
                    return float(width), float(height)
            _, _, _, _, width, height = struct.unpack_from('<6I', data, 18)[0:6]
            return float(width), float(height)
    return None


def numbers(value, count):
    if not isinstance(value, str):
        return None
    parts = value.split()
    if len(parts) != count:
        return None
    return [float(x) for x in parts]


DRIVER = """'use strict';
// written by sync_wallpaper.py: the sprite-sheet layers of this wallpaper start together, at %(time)g s
const layers = %(layers)s;
layers.forEach(l => engine.registerAsset(l.image));
let done = false;
export function update(value) {
    if (!done && engine.runtime >= %(time)g) {
        done = true;
        layers.forEach(l => {
            const created = thisScene.createLayer(l.image);
            created.origin = new Vec3(l.origin[0], l.origin[1], l.origin[2]);
            created.scale = new Vec3(l.scale[0], l.scale[1], l.scale[2]);
            created.angles = new Vec3(l.angles[0], l.angles[1], l.angles[2]);
            created.color = new Vec3(l.color[0], l.color[1], l.color[2]);
            created.alpha = l.alpha;
            created.alignment = l.alignment;
            if (l.index >= 0) thisScene.sortLayer(created, l.index);
        });
    }
    return value;
}
"""


def sync(wallpapers, name, time, suffix='sync'):
    source = os.path.join(wallpapers, name)
    target = os.path.join(wallpapers, name + '-' + suffix)
    # the pictures Wallpaper Engine made of the synced version stay
    kept = {}
    if os.path.exists(target):
        for file in os.listdir(target):
            if file.startswith('reference') and file.endswith('.png'):
                kept[file] = open(os.path.join(target, file), 'rb').read()
        shutil.rmtree(target)
    shutil.copytree(source, target, ignore=shutil.ignore_patterns('reference*.png', 'preview*', 'output*'))
    for file, content in kept.items():
        open(os.path.join(target, file), 'wb').write(content)
    pkg = os.path.join(target, 'scene.pkg')
    if os.path.exists(pkg):
        unpack_pkg(pkg, target)
        os.remove(pkg)
    scene_path = os.path.join(target, 'scene.json')
    scene = json.load(open(scene_path, encoding='utf-8-sig'))
    objects = scene['objects']
    parents = {o.get('parent') for o in objects if o.get('parent') is not None}
    layers = []
    keep = []
    for index, o in enumerate(objects):
        if 'image' not in o or o.get('particle') is not None:
            keep.append(o)
            continue
        texture = image_texture(target, o['image'])
        if texture is None or not is_sprite_sheet(target, texture):
            keep.append(o)
            continue
        why = None
        origin, scale, angles = numbers(o.get('origin'), 3), numbers(o.get('scale', '1 1 1'), 3), numbers(o.get('angles', '0 0 0'), 3)
        color = numbers(o.get('color', '1 1 1'), 3)
        size = numbers(o.get('size'), 2)
        natural = natural_size(target, o['image'], texture)
        if o.get('parent') is not None:
            why = 'has a parent'
        elif o['id'] in parents:
            why = 'has children'
        elif o.get('visible', True) is not True:
            why = 'its visibility is not a plain true'
        elif o.get('effects'):
            why = 'has effects'
        elif None in (origin, scale, angles, color) or not isinstance(o.get('alpha', 1.0), (int, float)):
            why = 'a place, colour or alpha that is not a plain value'
        elif size is None or natural is None:
            why = 'no size to compare with the asset'
        if why:
            print('  left as it is: %s (%s)' % (o.get('name'), why))
            keep.append(o)
            continue
        # the created layer has the asset's size: the authored size and scale go into its scale
        scale = [scale[0] * size[0] / natural[0], scale[1] * size[1] / natural[1], scale[2]]
        layers.append({'image': o['image'], 'origin': origin, 'scale': scale, 'angles': angles, 'color': color,
                       'alpha': float(o.get('alpha', 1.0)), 'alignment': o.get('alignment', 'center'), 'index': len(keep)})
        print('  synced: %s (%s, texture %s)' % (o.get('name'), o['image'], texture))
    objects[:] = keep
    for l in layers:
        l['index'] = min(l['index'], len(objects))
    if layers:
        next_id = max(o['id'] for o in objects) + 1
        position = len(objects) - 1 if objects and objects[-1].get('name') == 'owe clock probe' else len(objects)
        objects.insert(position, {
            'id': next_id, 'name': '__sync_driver', 'image': 'models/util/solidlayer.json', 'size': '10 10',
            'color': '0 0 0', 'alpha': 0, 'angles': '0 0 0', 'scale': '1 1 1', 'alignment': 'center', 'visible': True,
            'origin': {'value': '10 10 0', 'scriptproperties': {},
                       'script': DRIVER % {'time': time, 'layers': json.dumps(layers)}}})
    json.dump(scene, open(scene_path, 'w'), indent=2)
    project_path = os.path.join(target, 'project.json')
    project = json.load(open(project_path, encoding='utf-8-sig'))
    project['title'] = project.get('title', name) + ' (synced)'
    json.dump(project, open(project_path, 'w'), indent=2)
    return target, len(layers)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('wallpapers')
    parser.add_argument('names', nargs='+')
    parser.add_argument('--time', type=float, default=2.0, help="the scene's second at which the layers start")
    parser.add_argument('--suffix', default='sync', help='the written wallpaper is NAME-SUFFIX')
    args = parser.parse_args()
    for name in args.names:
        print(name)
        target, count = sync(args.wallpapers, name, args.time, args.suffix)
        print('  -> %s (%d layers)' % (target, count))


if __name__ == '__main__':
    main()
