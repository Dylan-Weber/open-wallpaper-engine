varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec4 g_Texture0Resolution;

float hi(float v) { return floor(v / 64.0) / 128.0; }
float lo(float v) { return frac(v / 64.0); }

void main() {
	float c = floor(v_TexCoord.x * 8.0);
	float v = 0.0;
	if (c < 2.0) v = g_Texture0Resolution.x;
	else if (c < 4.0) v = g_Texture0Resolution.y;
	else if (c < 6.0) v = g_Texture0Resolution.z;
	else v = g_Texture0Resolution.w;
	float g = (c == 0.0 || c == 2.0 || c == 4.0 || c == 6.0) ? hi(v) : lo(v);
	gl_FragColor = vec4(g, g, g, 1.0);
}
