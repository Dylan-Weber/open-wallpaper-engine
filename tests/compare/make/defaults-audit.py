# defaults-audit: every shipped effect on a picture, once with no settings (its defaults) and once with the defaults
# its shaders' annotations declare written out in each pass; the two should render alike. 12 effects to a wallpaper:
# defaults-audit-a-N (defaults) and defaults-audit-b-N (written out).
import re as _re
import json as _json
_assets = os.environ.get('WPE_ASSETS')
_skip = {'_empty', 'cursorripple', 'fluidsimulation'}
_names = sorted(n for n in os.listdir(os.path.join(_assets, 'effects')) if n not in _skip)


def _find(effect, relative):
    for root in (os.path.join(_assets, 'effects', effect), _assets):
        path = os.path.join(root, relative)
        if os.path.exists(path):
            return path
    return None


def _annotations(effect, shader):
    """material key -> default of the uniforms the shader's files declare with both."""
    found = {}
    for ext in ('vert', 'frag'):
        path = _find(effect, os.path.join('shaders', shader + '.' + ext))
        if not path:
            continue
        for line in open(path, errors='replace'):
            m = _re.match(r'\s*uniform\s+(\w+)\s+(\w+)\s*(?:\[\d+\])?\s*;\s*//\s*(\{.*\})', line)
            if not m or m.group(1) == 'sampler2D':
                continue
            try:
                j = _json.loads(m.group(3))
            except ValueError:
                continue
            if 'material' in j and 'default' in j:
                found[j['material']] = j['default']
    return found


def _passes(effect, written):
    details = _json.load(open(os.path.join(_assets, 'effects', effect, 'effect.json')))
    out = []
    for n, p in enumerate(details['passes']):
        instance = {"id": 5000 + n}
        if written and p.get('material'):
            material_path = _find(effect, p['material'])
            if not material_path:
                out.append(instance)
                continue
            shader = _json.load(open(material_path))['passes'][0]['shader']
            constants = _annotations(effect, shader)
            if constants:
                instance["constantshadervalues"] = constants
        out.append(instance)
    return out


for group in range(0, len(_names), 12):
    chunk = _names[group:group + 12]
    for mode in ('a', 'b'):
        name = 'defaults-audit-%s-%d' % (mode, group // 12)
        cells = []
        for effect in chunk:
            cells.append((effect, {"file": "effects/%s/effect.json" % effect, "id": 0, "name": "", "visible": True,
                                   "passes": _passes(effect, mode == 'b')}))
        effect_grid(name, 'shipped effects, defaults %s' % ('written out' if mode == 'b' else 'left out'), cells)
        with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
            _json.dump({"fps": 30}, f)
