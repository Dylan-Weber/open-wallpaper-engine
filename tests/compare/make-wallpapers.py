#!/usr/bin/env python3
"""Generates the test wallpapers in wallpapers/: each script in make/ (but lib.py and mdl.py) writes one or a few.

    python3 make-wallpapers.py [SCRIPT ...]     every script, or only the ones named (make/NAME.py)

Each wallpaper is a scene of our own that tests one feature with fixed values (no time, randomness, audio or
pointer, unless it is stamped with the clock probe to record them), built from assets that ship with Wallpaper
Engine, so Wallpaper Engine and owe should draw the same picture. A script runs with make/lib.py's names to hand and
nothing from another script.
"""
import importlib.util
import os
import sys

MAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'make')


def load_lib():
    spec = importlib.util.spec_from_file_location('lib', os.path.join(MAKE, 'lib.py'))
    lib = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(lib)
    return lib


def scripts(names):
    if names:
        return [name if name.endswith('.py') else name + '.py' for name in names]
    return sorted(name for name in os.listdir(MAKE) if name.endswith('.py') and name not in ('lib.py', 'mdl.py'))


def main():
    lib = load_lib()
    for name in scripts(sys.argv[1:]):
        path = os.path.join(MAKE, name)
        namespace = {key: value for key, value in vars(lib).items() if not key.startswith('__')}
        namespace['__file__'] = path
        exec(compile(open(path).read(), path, 'exec'), namespace)


if __name__ == '__main__':
    main()
