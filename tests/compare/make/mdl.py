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
