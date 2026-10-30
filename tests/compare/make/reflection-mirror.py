# reflection-mirror, -low: where Wallpaper Engine's planar reflection puts what it reflects. A flat floor whose shader
# shows _rt_Reflection at its screen position (like fantasticcar's grid, without the bumps), three "reflected" boxes at
# different heights and depths, and a camera looking down at them from the side. The floor is at y 0 (fantasticcar's),
# or at y -1 (-low): whether the mirror is the plane y 0 or the floor.
def reflection_mirror(name, floor_y):
    w = Scene3D(name, 'where the planar reflection puts things', eye='3 4 9')
    red = w.material('red', '1 0.2 0.2')
    green = w.material('green', '0.2 1 0.2')
    blue = w.material('blue', '0.3 0.4 1')
    w.files['materials/floor.json'] = {"passes": [{
        "shader": "mirrorfloor", "blending": "normal", "cullmode": "nocull", "depthtest": "enabled",
        "depthwrite": "enabled", "textures": ["_rt_Reflection"]}]}
    floor = 'models/floor.mdl'
    write_binary(os.path.join(w.name, floor), cube_mdl('materials/floor.json'))
    write(w.name + '/shaders/mirrorfloor.vert', """
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
varying vec3 v_ScreenPos;
void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_ScreenPos = gl_Position.xyw;
}
""")
    write(w.name + '/shaders/mirrorfloor.frag', """
uniform sampler2D g_Texture0;
varying vec3 v_ScreenPos;
void main() {
	vec2 uv = (v_ScreenPos.xy / v_ScreenPos.z) * 0.5 + 0.5;
	gl_FragColor = vec4(texSample2D(g_Texture0, uv).rgb * 0.8 + vec3(0.1, 0.1, 0.12), 1.0);
}
""")
    # the floor's top face at floor_y
    w.add({"name": "floor", "model": floor, "origin": vec(0, floor_y - 0.02, 0), "angles": "0 0 0",
           "scale": "6 0.02 6"})
    w.add({"name": "tall box", "model": red, "origin": vec(-1.5, floor_y + 1.2, 0), "angles": "0 0.3 0",
           "scale": "0.4 1.2 0.4", "reflected": True})
    w.add({"name": "cube", "model": green, "origin": vec(1.5, floor_y + 0.5, -1.5), "angles": "0 0.8 0",
           "scale": "0.5 0.5 0.5", "reflected": True})
    w.add({"name": "floating cube", "model": blue, "origin": vec(0.6, floor_y + 2.2, 1.8), "angles": "0.4 0.4 0",
           "scale": "0.35 0.35 0.35", "reflected": True})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))


reflection_mirror('reflection-mirror', 0)
reflection_mirror('reflection-mirror-low', -1)


# reflection-mirror-cull: which faces the reflection draws. A green cube inside a bigger red one: seen normally only the
# red outside shows; were the reflection to draw back faces (a mirror turns the triangles' winding round), its picture
# would show the red box's inside faces and the green cube's back in front of them.
w = Scene3D('reflection-mirror-cull', 'which faces the planar reflection draws', eye='3 4 9')
red = w.material('red', '1 0.2 0.2')
green = w.material('green', '0.2 1 0.2')
w.files['materials/floor.json'] = {"passes": [{
    "shader": "mirrorfloor", "blending": "normal", "cullmode": "nocull", "depthtest": "enabled",
    "depthwrite": "enabled", "textures": ["_rt_Reflection"]}]}
write_binary(os.path.join(w.name, 'models/floor.mdl'), cube_mdl('materials/floor.json'))
for stage in ('vert', 'frag'):
    with open(os.path.join(HERE, 'reflection-mirror', 'shaders', 'mirrorfloor.' + stage)) as f:
        write(w.name + '/shaders/mirrorfloor.' + stage, f.read())
w.add({"name": "floor", "model": 'models/floor.mdl', "origin": "0 -0.02 0", "angles": "0 0 0", "scale": "6 0.02 6"})
w.add({"name": "outer box", "model": red, "origin": "0 1.5 0", "angles": "0 0.4 0", "scale": "1.2 1.2 1.2",
       "reflected": True})
w.add({"name": "inner cube", "model": green, "origin": "0 1.5 0", "angles": "0 0.4 0", "scale": "0.5 0.5 0.5",
       "reflected": True})
w.save()
with open(os.path.join(HERE, 'reflection-mirror-cull', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'reflection-mirror-cull'))


# reflection-mirror-eye: which eye a reflected model's shader sees in the reflection pass. The cube's colour is its
# direction to g_EyePosition: the main camera's eye and the mirrored one (below the floor) colour its reflection apart.
w = Scene3D('reflection-mirror-eye', 'the eye a reflected shader sees', eye='3 4 9')
w.files['materials/floor.json'] = {"passes": [{
    "shader": "mirrorfloor", "blending": "normal", "cullmode": "nocull", "depthtest": "enabled",
    "depthwrite": "enabled", "textures": ["_rt_Reflection"]}]}
write_binary(os.path.join(w.name, 'models/floor.mdl'), cube_mdl('materials/floor.json'))
for stage in ('vert', 'frag'):
    with open(os.path.join(HERE, 'reflection-mirror', 'shaders', 'mirrorfloor.' + stage)) as f:
        write(w.name + '/shaders/mirrorfloor.' + stage, f.read())
w.files['materials/eyecube.json'] = {"passes": [{
    "shader": "eyecolor", "blending": "normal", "cullmode": "normal", "depthtest": "enabled", "depthwrite": "enabled",
    "textures": [None]}]}
write_binary(os.path.join(w.name, 'models/eyecube.mdl'), cube_mdl('materials/eyecube.json'))
write(w.name + '/shaders/eyecolor.vert', """
uniform mat4 g_ModelMatrix;
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
varying vec3 v_World;
void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_World = mul(vec4(a_Position, 1.0), g_ModelMatrix).xyz;
}
""")
write(w.name + '/shaders/eyecolor.frag', """
uniform vec3 g_EyePosition;
varying vec3 v_World;
void main() {
	gl_FragColor = vec4(normalize(g_EyePosition - v_World) * 0.5 + 0.5, 1.0);
}
""")
w.add({"name": "floor", "model": 'models/floor.mdl', "origin": "0 -0.02 0", "angles": "0 0 0", "scale": "6 0.02 6"})
w.add({"name": "eye cube", "model": 'models/eyecube.mdl', "origin": "0 1.5 0", "angles": "0 0.4 0",
       "scale": "1.2 1.2 1.2", "reflected": True})
w.save()
with open(os.path.join(HERE, 'reflection-mirror-eye', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'reflection-mirror-eye'))


# reflection-mirror-roll: reflection-mirror's boxes seen by a camera whose up is tilted (fantasticcar's camera path
# has such ups): a mirror camera that keeps the up as it is, rather than mirroring it, is rolled.
reflection_mirror('reflection-mirror-roll', 0)
with open(os.path.join(HERE, 'reflection-mirror-roll', 'scene.json')) as f:
    scene = json.load(f)
scene['camera']['up'] = '-0.25 0.92 -0.3'
with open(os.path.join(HERE, 'reflection-mirror-roll', 'scene.json'), 'w') as f:
    json.dump(scene, f, indent=2)
clock_probe.stamp(os.path.join(HERE, 'reflection-mirror-roll'))
