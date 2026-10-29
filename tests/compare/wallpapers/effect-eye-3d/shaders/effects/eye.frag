varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec3 g_EyePosition;
uniform float u_Scale; // {"material":"scale","default":1}

void main() {
	vec3 eye = abs(g_EyePosition) * u_Scale;
	vec3 colour = v_TexCoord.x < 0.5 ? frac(eye) : frac(eye * 0.01);
	gl_FragColor = vec4(colour, 1.0);
}
