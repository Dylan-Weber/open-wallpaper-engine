varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec2 g_Scale; // {"material":"repeat","default":"1 1"}
void main() {
	gl_FragColor = texSample2D(g_Texture0, frac(v_TexCoord.xy * g_Scale));
}
