# video-*: video wallpapers of a still picture of known colours, encoded with each colour tag a Workshop video can carry
# (BT.709 or BT.601, limited or full range, or none), and one 4:3 clip with edge markers for how a video that doesn't
# match the screen is scaled. Nothing is drawn below 80 % of the height: the GPU VM's captures have a black box there.
import shutil
import subprocess

from PIL import Image, ImageDraw

VIDEO_PATCHES = [
    # (r, g, b): primaries, secondaries, greys, skin and dark tones
    (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (0, 255, 255), (255, 0, 255),
    (255, 255, 255), (192, 192, 192), (128, 128, 128), (64, 64, 64), (16, 16, 16), (0, 0, 0),
    (230, 180, 150), (120, 60, 30), (30, 90, 160), (200, 40, 60), (40, 160, 90), (250, 200, 40),
]


def video_still(width, height):
    """The clip's picture: 3 rows of 6 patches in the top 75 %, a white frame one pixel in from each edge and corner
    markers, so both colours and the scaled picture's edges can be read."""
    picture = Image.new('RGB', (width, height), (40, 40, 40))
    draw = ImageDraw.Draw(picture)
    columns, rows = 6, 3
    cell_w, cell_h = width // (columns + 1), int(height * 0.75) // rows
    for i, colour in enumerate(VIDEO_PATCHES):
        x = cell_w // 2 + (i % columns) * cell_w
        y = height // 20 + (i // columns) * cell_h
        draw.rectangle([x + 8, y + 8, x + cell_w - 8, y + cell_h - 8], fill=colour)
    draw.rectangle([1, 1, width - 2, height - 2], outline=(255, 255, 255), width=2)
    for x, y in ((0, 0), (width - 40, 0)):
        draw.rectangle([x, y, x + 39, y + 39], fill=(255, 0, 255))
    return picture


def video_wallpaper(name, title, size, tags):
    still = os.path.join(HERE, name, 'still.png')
    os.makedirs(os.path.dirname(still), exist_ok=True)
    video_still(*size).save(still)
    clip = os.path.join(HERE, name, 'clip.mp4')
    if not os.path.exists(clip) and shutil.which('ffmpeg'):
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-loop', '1', '-i', still, '-t', '6', '-r', '30',
                        '-vf', 'scale=out_color_matrix=%s:out_range=%s,format=yuv420p%s' % (
                            tags['matrix'], tags['range'],
                            ',setparams=colorspace=unknown:color_primaries=unknown:color_trc=unknown'
                            if tags['flags'] is UNTAGGED else ''),
                        '-c:v', 'libx264', '-preset', 'veryslow', '-crf', '4', '-tune', 'stillimage']
                       + tags['flags'] + ['-movflags', '+faststart', clip], check=True)
    write(name + '/project.json', {"file": "clip.mp4", "type": "video", "title": title,
                                   "general": {"properties": {"schemecolor": {"type": "color", "value": "0 0 0",
                                                                              "text": "ui_browse_properties_scheme_color",
                                                                              "order": 0}}}})
    write(name + '/.gitignore', 'clip.mp4\nstill.png\n')
    write(name + '/compare.json', {"video_probe": True})


# no colour description at all in the stream (the encoder would copy the converter's tags otherwise)
UNTAGGED = ['-x264-params', 'colorprim=undef:transfer=undef:colormatrix=undef:range=tv']
BT709 = ['-colorspace', 'bt709', '-color_primaries', 'bt709', '-color_trc', 'bt709']
video_wallpaper('video-bt709', 'colour patches, BT.709 limited range (tagged)', (1920, 1080),
                {'matrix': 'bt709', 'range': 'tv', 'flags': BT709 + ['-color_range', 'tv']})
video_wallpaper('video-bt601', 'colour patches, BT.601 limited range (tagged)', (1920, 1080),
                {'matrix': 'bt601', 'range': 'tv',
                 'flags': ['-colorspace', 'smpte170m', '-color_primaries', 'smpte170m', '-color_trc', 'smpte170m',
                           '-color_range', 'tv']})
video_wallpaper('video-untagged-709', 'colour patches, BT.709 limited range, no colour tags', (1920, 1080),
                {'matrix': 'bt709', 'range': 'tv', 'flags': UNTAGGED})
video_wallpaper('video-untagged-601-sd', 'colour patches, 640x480 BT.601 limited range, no colour tags', (640, 480),
                {'matrix': 'bt601', 'range': 'tv', 'flags': UNTAGGED})
video_wallpaper('video-full-range', 'colour patches, BT.709 full range (tagged)', (1920, 1080),
                {'matrix': 'bt709', 'range': 'pc', 'flags': BT709 + ['-color_range', 'pc']})
video_wallpaper('video-aspect-4x3', 'a 4:3 picture on a 16:9 screen: how a video that does not fit is scaled',
                (1440, 1080), {'matrix': 'bt709', 'range': 'tv', 'flags': BT709 + ['-color_range', 'tv']})
video_wallpaper('video-aspect-21x9', 'a 21:9 picture on a 16:9 screen', (2560, 1080),
                {'matrix': 'bt709', 'range': 'tv', 'flags': BT709 + ['-color_range', 'tv']})
# is Wallpaper Engine's matrix chosen by the picture's size alone? (an HD clip tagged BT.601 plays as BT.709)
video_wallpaper('video-sd-bt709', 'colour patches, 640x480 tagged BT.709 limited range', (640, 480),
                {'matrix': 'bt709', 'range': 'tv', 'flags': BT709 + ['-color_range', 'tv']})
video_wallpaper('video-untagged-720', 'colour patches, 1280x720 BT.709 limited range, no colour tags', (1280, 720),
                {'matrix': 'bt709', 'range': 'tv', 'flags': UNTAGGED})
video_wallpaper('video-untagged-576', 'colour patches, 1024x576 BT.601 limited range, no colour tags', (1024, 576),
                {'matrix': 'bt601', 'range': 'tv', 'flags': UNTAGGED})
video_wallpaper('video-untagged-719', 'colour patches, 1280x719 BT.709 limited range, no colour tags', (1280, 718),
                {'matrix': 'bt709', 'range': 'tv', 'flags': UNTAGGED})
# SD (BT.601) up to 1024x576 and HD above? These two tell a width rule from a height rule.
video_wallpaper('video-untagged-1280x576', 'colour patches, 1280x576 BT.709 limited range, no colour tags', (1280, 576),
                {'matrix': 'bt709', 'range': 'tv', 'flags': UNTAGGED})
video_wallpaper('video-untagged-1024x600', 'colour patches, 1024x600 BT.709 limited range, no colour tags', (1024, 600),
                {'matrix': 'bt709', 'range': 'tv', 'flags': UNTAGGED})
video_wallpaper('video-untagged-1088x576', 'colour patches, 1088x576 BT.709 limited range, no colour tags', (1088, 576),
                {'matrix': 'bt709', 'range': 'tv', 'flags': UNTAGGED})
