varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec2 g_PointerPosition;

void main() {
	vec2 pointer = g_PointerPosition * 0.4;
	gl_FragColor = vec4(saturate(pointer.x), saturate(pointer.y), v_TexCoord.x, 1.0);
}
