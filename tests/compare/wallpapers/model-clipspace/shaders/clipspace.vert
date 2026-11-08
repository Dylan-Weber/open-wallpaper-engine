attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec2 v_TexCoord;

void main() {
	gl_Position = vec4(a_TexCoord * 2 - 1, 0.5, 1);
	v_TexCoord = a_TexCoord;
}
