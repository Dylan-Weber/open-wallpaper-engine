varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	gl_FragColor = texSample2D(g_Texture0, frac(v_TexCoord.xy * vec2(7.0, 1.0)));
}
