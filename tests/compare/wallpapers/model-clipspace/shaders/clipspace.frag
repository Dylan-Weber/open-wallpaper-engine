varying vec2 v_TexCoord;

void main() {
	gl_FragColor = vec4(1.0 - v_TexCoord.y, v_TexCoord.x * 0.5, v_TexCoord.y, 1.0);
}
