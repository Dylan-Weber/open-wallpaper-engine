"""Test wallpapers' binary files: a cube in Wallpaper Engine's static mesh format (MDLV0017), and textures (TEXV0005).
"""
import math
import struct


def cube_mdl(material, size=1.0, lightmap=False):
    """A cube of edge 2*size centred on the origin, one quad per face. With
    lightmap, vertices carry a second texcoord (vertex flag 39)."""
    faces = [  # normal, tangent, and the 4 corners in counter-clockwise order
        ((0, 0, 1), (1, 0, 0), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
        ((0, 0, -1), (-1, 0, 0), [(1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)]),
        ((1, 0, 0), (0, 0, -1), [(1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)]),
        ((-1, 0, 0), (0, 0, 1), [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)]),
        ((0, 1, 0), (1, 0, 0), [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)]),
        ((0, -1, 0), (1, 0, 0), [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
    ]
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    vertices, indices = b'', b''
    for f, (normal, tangent, corners) in enumerate(faces):
        for corner, uv in zip(corners, uvs):
            position = [c * size for c in corner]
            vertices += struct.pack('<12f', *position, *normal, *tangent, 1.0, *uv)
            if lightmap:
                vertices += struct.pack('<2f', *uv)
        base = f * 4
        for i in (0, 1, 2, 0, 2, 3):
            indices += struct.pack('<H', base + i)

    flags = 39 if lightmap else 15
    data = b'MDLV0017\0' + struct.pack('<iiI', flags, 1, 1)
    data += material.encode() + b'\0' + struct.pack('<i', 0)
    data += struct.pack('<6f', -size, -size, -size, size, size, size) + struct.pack('<i', flags)
    data += struct.pack('<I', len(vertices)) + vertices
    data += struct.pack('<I', len(indices)) + indices
    return data


# texture formats, as Wallpaper Engine numbers them
RGBA8888 = 0
RG88 = 8
R8 = 9


def tex(width, height, pixels, format=RGBA8888):
    """A texture of one uncompressed image without mipmaps: `pixels` row by row, 4 bytes a pixel for RGBA8888, 2 for
    RG88, 1 for R8."""
    channels = {RGBA8888: 4, RG88: 2, R8: 1}[format]
    assert len(pixels) == width * height * channels
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', format, 0, width, height, width, height, 0)
    # one image (FreeImage format -1: raw) with one mipmap, not compressed
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', width, height, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data



def tex_mips(levels):
    """An uncompressed RGBA8888 texture with mipmaps: `levels` are (width, height, pixels) from the largest down."""
    width, height = levels[0][0], levels[0][1]
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', RGBA8888, 0, width, height, width, height, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', len(levels))
    for w, h, pixels in levels:
        assert len(pixels) == w * h * 4
        data += struct.pack('<IIII', w, h, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data

def puppet_mdl(material, vertices, indices, bones, animations=(), shading=False, sim_type=0, morph=None):
    """A skinned mesh with its skeleton (MDLV0013, MDLS0001) and animations (MDLA0001).

    vertices: (x, y, z, u, v, [4 bone numbers], [4 weights]) each; indices: triangles' vertex numbers;
    bones: (name, parent bone number or None, (x, y, z) the bone's place in its parent[, the bone's physics as
    Wallpaper Engine writes it, a JSON string]) each;
    animations: (name, fps, [per bone: [per frame: (position, angle in radians, scale)]][, [per blend shape: [per
    frame: weight]]]) each, every bone with the same number of frames;
    morph: the mesh's blend shapes, as Wallpaper Engine's editor writes them (Reze, Workshop 3686252018), a dict:
    "index" (per vertex: 0, or 1..V for the V vertices the shapes move), "scale" (an offset of 32767 moves this far),
    "flags" (the mesh's morph flags: 0x400 normal offsets, 0x1000 a per-vertex alpha; the editor always sets 0x1000),
    "shapes" (each a dict of "offsets", V (x, y, z) in scene units, and "alpha" and "normals" as the flags need)."""
    flags = 0x1 | 0x8 | 0x800000 | 0x1000000  # positions, texture coordinates, bone numbers and weights
    if shading:
        flags |= 0x2 | 0x4  # and a normal and a tangent
    if morph:
        flags = flags & ~0x1 | 0x10000  # a four-float position in place of the three: w is the blend shape index
    weighted = morph is not None and any(len(animation) > 3 for animation in animations)
    data = b'MDLV0013\0' + struct.pack('<iII', flags, 1, 1)
    data += material.encode() + b'\0' + struct.pack('<I', morph["flags"] if morph else 0)
    body = b''
    for number, (x, y, z, u, v, blend, weight) in enumerate(vertices):
        body += struct.pack('<3f', x, y, z)
        if morph:
            body += struct.pack('<f', morph["index"][number])
        if shading:
            body += struct.pack('<3f', 0, 0, 1) + struct.pack('<4f', 1, 0, 0, 1)
        body += struct.pack('<4I', *blend) + struct.pack('<4f', *weight)
        body += struct.pack('<2f', u, v)
    data += struct.pack('<I', len(body)) + body
    data += struct.pack('<I', len(indices) * 6)
    for triangle in indices:
        data += struct.pack('<3H', *triangle)

    skeleton = b''
    for name, parent, place, *simulation in bones:
        x, y, z = place[:3]
        c, s = (math.cos(place[3]), math.sin(place[3])) if len(place) > 3 else (1, 0)  # the bone's own angle, if given
        matrix = (c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, x, y, z, 1)  # columns, the translation last
        # a bone with physics has simulation type 1 (as in Wallpaper Engine's own files)
        kind = 1 if simulation and simulation[0] else sim_type
        skeleton += name.encode() + b'\0' + struct.pack('<iII', kind, 0xFFFFFFFF if parent is None else parent, 64)
        skeleton += struct.pack('<16f', *matrix) + (simulation[0] if simulation else '').encode() + b'\0'
    head = b'MDLS0001\0'
    data += head + struct.pack('<IHH', len(data) + len(head) + 8 + len(skeleton), len(bones), 0) + skeleton

    if animations:
        track = b''
        for number, (name, fps, bone_frames, *shape_weights) in enumerate(animations):
            frames = len(bone_frames[0])
            track += struct.pack('<iI', 1 + number, 0)
            track += name.encode() + b'\0loop\0' + struct.pack('<fiII', fps, frames - 1, 0, len(bones))
            for frames_of_bone in bone_frames:
                values = b''.join(struct.pack('<9f', *position, *angle, *scale)
                                  for position, angle, scale in frames_of_bone)
                track += struct.pack('<iI', 0, len(values)) + values
            if weighted:
                # MDLA0006 as Reze's: no root motion, no bone curves, then the blend shapes' weight curves (one event
                # whose "time" is the morph scale, a curve a shape, the first without its id), bounds, no scalar curves
                track += struct.pack('<IB', 0, 0)
                if shape_weights:
                    curves = shape_weights[0]
                    track += struct.pack('<BIfHH', 1, 1, morph["scale"], len(curves), 0)
                    for shape, values in enumerate(curves):
                        assert len(values) == frames
                        track += (struct.pack('<H', shape) if shape else b'') + struct.pack('<I', 4 * len(values))
                        track += struct.pack('<%df' % len(values), *values)
                else:
                    track += struct.pack('<B', 0)
                track += struct.pack('<6f', 0, 0, 0, 0, 0, 0) + struct.pack('<B', 0)
            track += struct.pack('<I', 0)  # no events
        head = b'MDLA0006\0' if weighted else b'MDLA0001\0'
        data += head + struct.pack('<II', len(data) + len(head) + 8 + len(track), len(animations)) + track

    if morph:
        # MDMP: per mesh a u16 shape count, the scale and V; per shape its id, a zero, its name, then length-prefixed
        # int16 blocks over 32767: the offsets (in units of the scale), the normal offsets, the alpha
        count = len(morph["index"]) and max(morph["index"])
        block = struct.pack('<HfI', len(morph["shapes"]), morph["scale"], count)
        def int16s(values):
            packed = [max(-32767, min(32767, round(value * 32767))) for value in values]
            return struct.pack('<I', 2 * len(packed)) + struct.pack('<%dh' % len(packed), *packed)
        for number, shape in enumerate(morph["shapes"]):
            name = 'Shape %d' % (number + 1)
            block += struct.pack('<II', 100 + number, 0) + name.encode() + b'\0'
            assert len(shape["offsets"]) == count
            block += int16s([c / morph["scale"] for offset in shape["offsets"] for c in offset])
            if morph["flags"] & 0x400:
                block += int16s([c for normal in shape["normals"] for c in normal])
            if morph["flags"] & 0x1000:
                block += int16s(shape["alpha"])
        head = b'MDMP0001\0'
        data += head + struct.pack('<I', len(data) + len(head) + 4 + len(block)) + block
    return data


def quad_mdl(material, size=1.0, normal=(0, 0, 1), tangent=(1, 0, 0)):
    """A single square of edge 2*size in the xy plane, facing `normal` (counter-clockwise from there): a one-sided
    surface, as a wall or a ceiling is in a game's level."""
    corners = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    vertices, indices = b'', b''
    for corner, uv in zip(corners, uvs):
        position = [c * size for c in corner]
        vertices += struct.pack('<12f', *position, *normal, *tangent, 1.0, *uv)
    for i in (0, 1, 2, 0, 2, 3):
        indices += struct.pack('<H', i)
    data = b'MDLV0017\0' + struct.pack('<iiI', 15, 1, 1)
    data += material.encode() + b'\0' + struct.pack('<I', 0)
    data += struct.pack('<6f', -size, -size, 0, size, size, 0)
    data += struct.pack('<I', 15) + struct.pack('<I', len(vertices)) + vertices
    data += struct.pack('<I', len(indices)) + indices
    return data


def quads_mdl(parts, normal=(0, 0, 1), tangent=(1, 0, 0)):
    """One model of several squares in the xy plane, each its own mesh with its own material: `parts` are
    (material, size, (x, y)) in mesh order, a square of edge 2*size centred at x, y (a game level's model is many
    meshes, one a material)."""
    corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    data = b'MDLV0017\0' + struct.pack('<iiI', 15, 1, len(parts))
    for material, size, (cx, cy) in parts:
        vertices, indices = b'', b''
        for (x, y), uv in zip(corners, uvs):
            vertices += struct.pack('<12f', cx + x * size, cy + y * size, 0, *normal, *tangent, 1.0, *uv)
        for i in (0, 1, 2, 0, 2, 3):
            indices += struct.pack('<H', i)
        data += material.encode() + b'\0' + struct.pack('<I', 0)
        data += struct.pack('<6f', cx - size, cy - size, 0, cx + size, cy + size, 0)
        data += struct.pack('<I', 15) + struct.pack('<I', len(vertices)) + vertices
        data += struct.pack('<I', len(indices)) + indices
    return data
