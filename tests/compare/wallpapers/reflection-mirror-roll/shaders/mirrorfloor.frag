
uniform sampler2D g_Texture0;
varying vec3 v_ScreenPos;
void main() {
	vec2 uv = (v_ScreenPos.xy / v_ScreenPos.z) * 0.5 + 0.5;
	gl_FragColor = vec4(texSample2D(g_Texture0, uv).rgb * 0.8 + vec3(0.1, 0.1, 0.12), 1.0);
}
