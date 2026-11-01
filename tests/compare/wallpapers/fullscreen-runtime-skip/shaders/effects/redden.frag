varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}

void main() {
	vec4 color = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = vec4(1.0, color.g * 0.2, color.b * 0.2, color.a);
}
