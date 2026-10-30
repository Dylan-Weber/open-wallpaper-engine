# script-model-data: meshes a script builds itself (thisScene.createModelData with a material asset, shown by
# createLayer({name, model})). Three layers: a square; a square whose vertices applyData then moves into a trapezoid;
# the same trapezoid but with the vertex format given with a tangent-free position/normal/uv layout and a layer
# origin set after creation. The mesh uses a picture material, so the cells show clouds on flat shapes.
w = Wallpaper('script-model-data', 'meshes built by a script', clear='0.1 0.1 0.15')
w.files['materials/mesh.json'] = {"passes": [{
    "shader": "genericimage2", "blending": "translucent", "cullmode": "nocull", "depthtest": "disabled",
    "depthwrite": "disabled", "textures": ["util/clouds_256"]}]}
script = """'use strict';
const material = engine.registerAsset('materials/mesh.json');

function quad(w, h, topScale) {
    const v = new Float32Array(32);
    const corners = [[-1, -1, 0, 0], [1, -1, 1, 0], [1, 1, 1, 1], [-1, 1, 0, 1]];
    for (let i = 0; i < 4; ++i) {
        const c = corners[i];
        const s = c[1] > 0 ? topScale : 1;
        v[i * 8] = c[0] * w * 0.5 * s; v[i * 8 + 1] = c[1] * h * 0.5; v[i * 8 + 2] = 0;
        v[i * 8 + 5] = 1;
        v[i * 8 + 6] = c[2]; v[i * 8 + 7] = c[3];
    }
    return v;
}

function make(name, x, y, topScale, later) {
    const vertexBuffer = quad(300, 300, topScale);
    const data = thisScene.createModelData({
        boundingBoxMins: new Vec3(-300, -300, -10), boundingBoxMaxs: new Vec3(300, 300, 10),
        shapes: [{vertexBuffer: vertexBuffer, indexBuffer: new Uint16Array([0, 1, 2, 0, 2, 3]),
                  vertexFormat: [IModelData.POSITION, IModelData.NORMAL, IModelData.UV],
                  material: material, isVertexBufferDynamic: true}]});
    const layer = thisScene.createLayer({name: name, model: data});
    layer.origin = new Vec3(x, y, 0);
    if (later !== undefined) data.applyData({shapes: [{vertexBuffer: quad(300, 300, later)}]});
}

export function init(value) {
    make('square', 400, 540, 1);
    make('trapezoid', 960, 540, 1, 0.4);
    make('wide top', 1520, 540, 1.5);
    return value;
}
export function update(value) { return value; }
"""
w.add({"name": "driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'script-model-data', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-model-data'))
