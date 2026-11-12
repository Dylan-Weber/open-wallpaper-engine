# script-math-probe: what Wallpaper Engine's scripts compute with the vector and matrix classes where the SceneScript
# documentation and its observed behaviour disagree (Vec normalize of zero, perpendicular, equals(), Mat3 axes, angle,
# decompose, toConfigString). The values are printed as text; owe's output is compared by reading both pictures.
w = Wallpaper('script-math-probe', 'text lines with the results of math helper calls', clear='0 0 0')
probe = """'use strict';
import * as WEMath from 'WEMath';
import * as WEVector from 'WEVector';
const f = (v) => {
    if (typeof v === 'number') return Number.isNaN(v) ? 'NaN' : String(+v.toFixed(3));
    if (v === undefined) return 'undefined';
    if (v === null) return 'null';
    if (typeof v === 'string' || typeof v === 'boolean') return String(v);
    if (v.m) return '[' + Array.from(v.m).map(f).join(',') + ']';
    if (v.rotation !== undefined) return 'T' + f(v.translation) + ' R' + f(v.rotation) + ' S' + f(v.scale);
    return '(' + [v.x, v.y, v.z, v.w].filter((c) => c !== undefined).map(f).join(',') + ')';
};
const cases = [
  ['v2 norm 0', () => new Vec2(0, 0).normalize()],
  ['v3 norm 0', () => new Vec3(0, 0, 0).normalize()],
  ['v4 norm 0', () => new Vec4(0, 0, 0, 0).normalize()],
  ['mod normalize v2 0', () => WEVector.normalize(new Vec2(0, 0))],
  ['mod normalize v3 0', () => WEVector.normalize(new Vec3(0, 0, 0))],
  ['mod normalize v2 (3,4)', () => WEVector.normalize(new Vec2(3, 4))],
  ['v2 perp (1,0)', () => new Vec2(1, 0).perpendicular()],
  ['v2 perp (1,2)', () => new Vec2(1, 2).perpendicular()],
  ['v2 equals()', () => new Vec2(1, 2).equals()],
  ['v3 equals()', () => new Vec3(1, 2, 3).equals()],
  ['v4 equals()', () => new Vec4(1, 2, 3, 4).equals()],
  ['v3 equals(num)', () => new Vec3(1, 1, 1).equals(1)],
  ['v2 equals(v3)', () => new Vec2(1, 2).equals(new Vec3(1, 2, 3))],
  ['v2 toConfigString', () => new Vec2(1, 2).toConfigString()],
  ['m3 id right', () => new Mat3().right()],
  ['m3 id up', () => new Mat3().up()],
  ['m3 id forward', () => new Mat3().forward()],
  ['m3 id angle', () => new Mat3().angle()],
  ['m3 rot90 angle', () => Mat3.fromRotation(90).angle()],
  ['m3 rot30 angle', () => Mat3.fromRotation(30).angle()],
  ['m3 rot90 m', () => Mat3.fromRotation(90)],
  ['m3 toConfigString', () => Mat3.fromRotation(30).toConfigString()],
  ['m3 decompose', () => Mat3.compose(new Vec2(1, 2), 30, new Vec2(2, 3)).decompose()],
  ['m3 decompose mirror', () => Mat3.compose(new Vec2(1, 2), 30, new Vec2(-2, 3)).decompose()],
  ['m3 compose m', () => Mat3.compose(new Vec2(1, 2), 30, new Vec2(2, 3))],
  ['m3 translation(v)', () => new Mat3().translation(new Vec2(4, 5))],
  ['m3 multiply vec3', () => Mat3.fromTranslation(new Vec2(1, 2)).multiply(new Vec3(1, 1, 1))],
  ['m4 id right', () => new Mat4().right()],
  ['m4 toConfigString', () => Mat4.fromTranslation(new Vec3(1, 2, 3)).toConfigString()],
  ['m4 decompose', () => Mat4.compose(new Vec3(1, 2, 3), new Vec3(10, 20, 30), new Vec3(2, 3, 4)).decompose()],
  ['m4 decompose mirror', () => Mat4.compose(new Vec3(0, 0, 0), new Vec3(0, 0, 0), new Vec3(-1, 0.5, 1)).decompose()],
  ['m4 multiply vec4', () => Mat4.fromTranslation(new Vec3(1, 2, 3)).multiply(new Vec4(1, 1, 1, 1))],
  ['m4 multiply vec3', () => Mat4.fromTranslation(new Vec3(1, 2, 3)).multiply(new Vec3(1, 1, 1))],
  ['m4 translation(v) ret', () => new Mat4().translation(new Vec3(4, 5, 6))],
  ['m4 euler', () => Mat4.fromEuler(10, 20, 30).extractEuler()],
];
export function init(value) {
    const lines = cases.slice(%d, %d).map(([name, fn]) => {
        let r; try { r = f(fn()); } catch (e) { r = 'THROW ' + e.name; }
        return name + ': ' + r;
    });
    value = lines.join('\\n');
    return value;
}
export function update(value) { return value; }
"""
for column, (start, end) in enumerate(((0, 10), (10, 20), (20, 30), (30, 40))):
    text_layer(w, 'probe %d' % column, 20 + column * 480, 1060, {"value": "", "script": probe % (start, end), "scriptproperties": {}},
               'left', 'top', size='470 1000', padding=0, pointsize=9, opaquebackground=False)
w.save()
