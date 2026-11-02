# Comparisons with Wallpaper Engine

Test wallpapers of our own, each with a picture the real Wallpaper Engine took of it, and a script that renders them
with SceneViewer and scores the difference.

```
python3 -m venv tests/compare/.venv && tests/compare/.venv/bin/pip install -r tests/compare/requirements.txt
lito build -p owe-sceneviewer --profile debug
tests/compare/.venv/bin/python tests/compare/owe_compare.py              # every wallpaper with a reference
tests/compare/.venv/bin/python tests/compare/owe_compare.py text-size    # only these
```

Wallpaper Engine's assets are read from `tests/assets` (a link to its `assets` folder, as the other tests use) or
`WPE_ASSETS`. Pictures and scores go to `output-owe/` (`report.json`, and for each wallpaper `side-by-side.png`:
reference, owe, and the difference with changed pixels in red). A wallpaper is `ok` at an SSIM of at least 0.98
with no differing region; see `owe_compare.py --help` for the rest.

- `owe_compare.py` renders and scores; `owe_compare.test.py` tests it.
- `make-wallpapers.py` writes the wallpapers in `wallpapers/` from the scripts in `make/`: one script per feature,
  each with fixed values, built only from assets that ship with Wallpaper Engine. `make/lib.py` holds what they're
  built with (layers, effects, cube models, lights, textures).
- `clock-probe.py` stamps a wallpaper with a probe that records, in its pictures, the frame they show and the
  inputs Wallpaper Engine gave it (pointer, audio, date, camera eye), so owe renders the same moment with the same
  inputs. `clock-probe.test.py` tests it.

## Adding a wallpaper

Write `make/NAME.py`, run `tests/compare/make-wallpapers.py NAME`, and have Wallpaper Engine take its
`reference.png`: the wallpaper at 1920x1080, 350 frames (11.7 s) after it loads, or the frame its clock probe
records, with the "shadows" setting at medium (or its `compare.json`'s `shadow_quality`). A fix for a difference
adds a wallpaper that shows it: it differs on the fix's base and is `ok` with the fix.

Wallpapers that aren't ours (from the Workshop) can be compared from `wallpapers/_scratch/`, which is not
committed.
