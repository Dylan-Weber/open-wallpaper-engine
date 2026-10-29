"""Test wallpapers' binary files: a cube in Wallpaper Engine's static mesh format (MDLV0017), and textures (TEXV0005).
"""
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


def puppet_mdl(material, vertices, indices, bones, animations=(), shading=False, sim_type=0):
    """A skinned mesh with its skeleton (MDLV0013, MDLS0001) and animations (MDLA0001).

    vertices: (x, y, z, u, v, [4 bone numbers], [4 weights]) each; indices: triangles' vertex numbers;
    bones: (name, parent bone number or None, (x, y, z) the bone's place in its parent[, the bone's physics as
    Wallpaper Engine writes it, a JSON string]) each;
    animations: (name, fps, [per bone: [per frame: (position, angle in radians, scale)]]) each, every bone with the
    same number of frames."""
    flags = 0x1 | 0x8 | 0x800000 | 0x1000000  # positions, texture coordinates, bone numbers and weights
    if shading:
        flags |= 0x2 | 0x4  # and a normal and a tangent
    data = b'MDLV0013\0' + struct.pack('<iII', flags, 1, 1)
    data += material.encode() + b'\0' + struct.pack('<I', 0)
    body = b''
    for x, y, z, u, v, blend, weight in vertices:
        body += struct.pack('<3f', x, y, z)
        if shading:
            body += struct.pack('<3f', 0, 0, 1) + struct.pack('<4f', 1, 0, 0, 1)
        body += struct.pack('<4I', *blend) + struct.pack('<4f', *weight)
        body += struct.pack('<2f', u, v)
    data += struct.pack('<I', len(body)) + body
    data += struct.pack('<I', len(indices) * 6)
    for triangle in indices:
        data += struct.pack('<3H', *triangle)

    skeleton = b''
    for name, parent, (x, y, z), *simulation in bones:
        matrix = (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1)  # columns, the translation last
        skeleton += name.encode() + b'\0' + struct.pack('<iII', sim_type, 0xFFFFFFFF if parent is None else parent, 64)
        skeleton += struct.pack('<16f', *matrix) + (simulation[0] if simulation else '').encode() + b'\0'
    head = b'MDLS0001\0'
    data += head + struct.pack('<IHH', len(data) + len(head) + 8 + len(skeleton), len(bones), 0) + skeleton

    if animations:
        track = b''
        for name, fps, bone_frames in animations:
            frames = len(bone_frames[0])
            track += struct.pack('<iI', 1 + animations.index((name, fps, bone_frames)), 0)
            track += name.encode() + b'\0loop\0' + struct.pack('<fiII', fps, frames - 1, 0, len(bones))
            for frames_of_bone in bone_frames:
                values = b''.join(struct.pack('<9f', *position, *angle, *scale)
                                  for position, angle, scale in frames_of_bone)
                track += struct.pack('<iI', 0, len(values)) + values
            track += struct.pack('<I', 0)  # no events
        head = b'MDLA0001\0'
        data += head + struct.pack('<II', len(data) + len(head) + 8 + len(track), len(animations)) + track
    return data
