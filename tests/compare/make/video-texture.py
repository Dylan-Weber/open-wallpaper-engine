# videotex-*: the video-* probe clips as a scene's video texture (an MP4 inside a .tex, as the editor writes one for a
# layer made from a video file), drawn over the whole screen, so SceneViewer can be compared with Wallpaper Engine on
# the same YUV matrix choices as the video wallpapers.
import struct
import subprocess


def video_tex(width, height, mp4):
    """A .tex holding an MP4 the way the editor stores one (Workshop 3122339805): TEXB0003 with image format -1, flags
    0x23 and one uncompressed mip of the video's size whose data is the file itself."""
    header = b'TEXV0005\0TEXI0001\0' + struct.pack('<7i', 0, 0x23, width, height, width, height, 0)
    body = b'TEXB0003\0' + struct.pack('<3i', 1, -1, 1) + struct.pack('<5i', width, height, 0, 0, len(mp4))
    return header + body + mp4


def video_texture_wallpaper(name, clip, title):
    folder = os.path.join(HERE, clip)
    path = os.path.join(folder, 'clip.mp4')
    if not os.path.exists(path):
        return  # make video-probe first
    probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height',
                            '-of', 'csv=p=0', path], capture_output=True, text=True, check=True).stdout.split(',')
    width, height = int(probe[0]), int(probe[1])
    w = Wallpaper(name, title, clear='0 0 0')
    w.files['models/video.json'] = {"material": "materials/video.json", "width": width, "height": height,
                                    "nopadding": True}
    w.files['materials/video.json'] = {"passes": [{
        "blending": "normal", "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
        "shader": "genericimage2", "textures": ["video"]}]}
    # as the editor makes it: the layer as big as the video, scaled to the screen
    w.image('video', 'models/video.json', WIDTH / 2, HEIGHT / 2, width, height,
            scale=vec(WIDTH / width, HEIGHT / height, 1))
    w.save()
    write_binary(os.path.join(name, 'materials', 'video.tex'), video_tex(width, height, open(path, 'rb').read()))
    # made from the clip whenever it is missing, so not committed
    write(name + '/.gitignore', 'materials/video.tex\n')


for clip, what in (('video-bt709', '1920x1080 tagged BT.709'), ('video-bt601', '1920x1080 tagged BT.601'),
                   ('video-sd-bt709', '640x480 tagged BT.709'), ('video-untagged-601-sd', '640x480 untagged'),
                   ('video-untagged-576', '1024x576 untagged'), ('video-untagged-1024x600', '1024x600 untagged')):
    video_texture_wallpaper('videotex-' + clip[len('video-'):], clip, 'a video texture: ' + what)
