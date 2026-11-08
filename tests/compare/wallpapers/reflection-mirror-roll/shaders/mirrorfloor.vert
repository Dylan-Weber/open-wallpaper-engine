
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
varying vec3 v_ScreenPos;
void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_ScreenPos = gl_Position.xyw;
}
