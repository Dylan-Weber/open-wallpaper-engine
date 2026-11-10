# shadow-matrix-dump: the directional light's three cascade matrices and atlas transforms as Wallpaper Engine and owe
# give them to a shader (for the thin-caster differences: shadow-thin-sweep, the courtyard stripe). A quad facing the
# camera, its shader declaring WE's g_LFeature_ShadowProjection / g_LFeature_ShadowProjectionTransform and owe's
# g_OweShadowViewProjections / g_OweShadowAtlasTransforms and adding them (each engine leaves the other's at 0), writes
# an 8 x 8 grid read row by row from the top left: cells 0-47 the matrices (cascade c, entry dot(mul(e_i, M), e_j) at
# 16c + 4i + j), cells 48-59 the transforms (4 a cascade). Each value m is red 0.5 + m / 64, green fract(m), blue
# fract(256 m). Same camera, sun and cascades (30 / 31 / 100) as shadow-thin-sweep, medium shadows.

WE_PREAMBLE = """// [COMBO] {"material":"ui_editor_properties_lighting","combo":"LIGHTING","default":1}
#if LIGHTS_SHADOW_MAPPING
#define SHADOW_ATLAS_SAMPLER g_Texture6
#define SHADOW_ATLAS_TEXEL g_Texture6Texel
uniform sampler2DComparison g_Texture6; // {"hidden":true,"default":"_rt_shadowAtlas"}
uniform vec4 g_Texture6Texel;
#endif
#include "common_fragment.h"
#include "common_pbr_2.h"
"""
VERT = """uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec2 v_TexCoord;
void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_TexCoord = a_TexCoord;
}
"""
FRAG = """varying vec2 v_TexCoord;
uniform mat4 g_LFeature_ShadowProjection[3];
uniform vec4 g_LFeature_ShadowProjectionTransform[3];
uniform mat4 g_OweShadowViewProjections[24];
uniform vec4 g_OweShadowAtlasTransforms[24];

vec4 basis(float k) {
	return vec4(1.0, 1.0, 1.0, 1.0) - min(abs(vec4(k, k, k, k) - vec4(0.0, 1.0, 2.0, 3.0)), vec4(1.0, 1.0, 1.0, 1.0));
}
float entry(mat4 m, float i, float j) {
	return dot(mul(basis(i), m), basis(j));
}
void main() {
	vec2 cell = floor(saturate(vec2(v_TexCoord.x, 1.0 - v_TexCoord.y)) * 7.999);
	float n = cell.y * 8.0 + cell.x;
	float v = 0.0;
	if (n < 48.0) {
		float c = floor(n / 16.0);
		float k = n - 16.0 * c;
		float i = floor(k / 4.0);
		float j = k - 4.0 * i;
		mat4 m0 = g_LFeature_ShadowProjection[0] + g_OweShadowViewProjections[0];
		mat4 m1 = g_LFeature_ShadowProjection[1] + g_OweShadowViewProjections[1];
		mat4 m2 = g_LFeature_ShadowProjection[2] + g_OweShadowViewProjections[2];
		vec4 pc = basis(c);
		v = entry(m0, i, j) * pc.x + entry(m1, i, j) * pc.y + entry(m2, i, j) * pc.z;
	} else if (n < 60.0) {
		float k = n - 48.0;
		float c = floor(k / 4.0);
		float j = k - 4.0 * c;
		vec4 pc = basis(c);
		vec4 t = (g_LFeature_ShadowProjectionTransform[0] + g_OweShadowAtlasTransforms[0]) * pc.x +
		         (g_LFeature_ShadowProjectionTransform[1] + g_OweShadowAtlasTransforms[1]) * pc.y +
		         (g_LFeature_ShadowProjectionTransform[2] + g_OweShadowAtlasTransforms[2]) * pc.z;
		v = dot(t, basis(j));
	}
	gl_FragColor = vec4(saturate(0.5 + v / 64.0), frac(v), frac(v * 256.0), 1.0);
}
"""

def make(name, owe=True):
  frag = FRAG if owe else (WE_PREAMBLE + FRAG.replace('uniform mat4 g_OweShadowViewProjections[24];\n', '')
                           .replace('uniform mat4 g_LFeature_ShadowProjection[3];\n', '')
                           .replace('uniform vec4 g_LFeature_ShadowProjectionTransform[3];\n', '')
                           .replace('gl_FragColor = vec4(', 'gl_FragColor = CAST4(0.0) * PerformLighting_V1(CAST3(0.0), CAST3(1.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 1.0, 0.0), CAST3(1.0), CAST3(0.04), 0.5, 0.0).rgbr + vec4(')
                           .replace('uniform vec4 g_OweShadowAtlasTransforms[24];\n', '')
                           .replace(' + g_OweShadowViewProjections[0]', '').replace(' + g_OweShadowViewProjections[1]', '')
                           .replace(' + g_OweShadowViewProjections[2]', '').replace(' + g_OweShadowAtlasTransforms[0]', '')
                           .replace(' + g_OweShadowAtlasTransforms[1]', '').replace(' + g_OweShadowAtlasTransforms[2]', ''))
  w = Scene3D(name, 'the cascade matrices a shader sees (%s)' % name, ambient='0.1 0.1 0.1', eye='0 6 2')
  lit = w.material('lit', '1 1 1', lighting=True)
  w.cube('floor', lit, 0, -0.6, 0, (0, 0, 0), '4 0.1 4')
  w.cube('post', lit, 0, -0.25, 0.8, (0, 0, 0), '0.015 0.25 0.015')
  w.files['materials/dump.json'] = {"passes": [{"shader": "dump", "blending": "normal", "cullmode": "nocull",
                                                "depthtest": "enabled", "depthwrite": "enabled", "textures": [None],
                                                "combos": {"LIGHTING": 1}}]}
  w.files['shaders/dump.vert'] = VERT
  w.files['shaders/dump.frag'] = frag
  write_binary(os.path.join(w.name, 'models', 'dump.mdl'), cube_mdl('materials/dump.json'))
  # the +y face toward the camera above, filling most of the view, a little above the floor
  w.cube('dump', 'models/dump.mdl', 0, 0.5, 0, (0, 0, 0), '2.4 0.01 2.4')
  w.light('sun', 'ldirectional', 0, 2, 0, '1 1 1', 3.09, 5, (0.0, 0.32301, -2.06849), castshadow=True,
          cascadedistance0=30.0, cascadedistance1=31.0, cascadedistance2=100.0)
  w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
  w.save()
  with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
      json.dump({"fps": 30, "shadow_quality": "medium"}, f)


make('shadow-matrix-dump')
# Wallpaper Engine: its shader declares the light uniforms itself, for a shader calling PerformLighting_V1 (as
# generic4.frag does); the grid reads WE's names alone
make('shadow-matrix-dump-we', owe=False)
