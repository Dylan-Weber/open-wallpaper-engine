varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}

void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	albedo.a *= 0.5;
	gl_FragColor = albedo;
}
