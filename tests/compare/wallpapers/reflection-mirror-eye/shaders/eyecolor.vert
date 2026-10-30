
uniform mat4 g_ModelMatrix;
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
varying vec3 v_World;
void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_World = mul(vec4(a_Position, 1.0), g_ModelMatrix).xyz;
}
