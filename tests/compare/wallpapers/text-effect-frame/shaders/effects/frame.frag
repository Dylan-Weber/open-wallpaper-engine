varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = albedo + (1.0 - albedo.a) * vec4(0.35, 0.08, 0.08, 0.6);
}
