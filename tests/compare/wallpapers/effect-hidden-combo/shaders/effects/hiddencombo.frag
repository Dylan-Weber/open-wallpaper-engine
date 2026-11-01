// [COMBO] {"material":"Mode","combo":"MODE","type":"options","default":0,"options":{"a":0,"b":4}}
// [COMBO] {"material":"Sharp","combo":"SHARP","type":"options","default":0,"require":{"MODE":4}}
varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	float k = 0.5 + 0.5 * float(SHARP);
	gl_FragColor = vec4(albedo.rgb * k, albedo.a);
}
