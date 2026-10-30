varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = vec4(v_TexCoord.x, v_TexCoord.y, 0.0, 1.0) + albedo * 0.0;
}
