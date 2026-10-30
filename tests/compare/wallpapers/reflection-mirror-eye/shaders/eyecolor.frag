
uniform vec3 g_EyePosition;
varying vec3 v_World;
void main() {
	gl_FragColor = vec4(normalize(g_EyePosition - v_World) * 0.5 + 0.5, 1.0);
}
