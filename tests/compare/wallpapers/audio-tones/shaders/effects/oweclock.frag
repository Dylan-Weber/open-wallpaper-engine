varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform float g_Time;
uniform float u_ScriptFrame; // {"material":"scriptframe","default":-1}
uniform vec2 g_PointerPosition;
uniform vec3 g_EyePosition;
uniform float g_AudioSpectrum16Left[16];
uniform float g_AudioSpectrum16Right[16];
uniform float g_AudioSpectrum32Left[32];
uniform float g_AudioSpectrum32Right[32];
uniform float g_AudioSpectrum64Left[64];
uniform float g_AudioSpectrum64Right[64];
uniform float u_press; // {"material":"press","default":-1}
uniform float u_date_day; // {"material":"date_day","default":-1}
uniform float u_date_centis; // {"material":"date_centis","default":-1}
uniform float u_timezone; // {"material":"timezone","default":-1}
uniform float u_camerax; // {"material":"camerax","default":-1}
uniform float u_cameray; // {"material":"cameray","default":-1}
uniform float u_cameraz; // {"material":"cameraz","default":-1}

float bitOf(float value, float b) {
	return mod(floor(value / exp2(b)), 2.0);
}

float peak16l() {
	float m = 0.0;
	if (g_AudioSpectrum16Left[0][0] > m) m = g_AudioSpectrum16Left[0][0];
	if (g_AudioSpectrum16Left[0][1] > m) m = g_AudioSpectrum16Left[0][1];
	if (g_AudioSpectrum16Left[0][2] > m) m = g_AudioSpectrum16Left[0][2];
	if (g_AudioSpectrum16Left[0][3] > m) m = g_AudioSpectrum16Left[0][3];
	if (g_AudioSpectrum16Left[1][0] > m) m = g_AudioSpectrum16Left[1][0];
	if (g_AudioSpectrum16Left[1][1] > m) m = g_AudioSpectrum16Left[1][1];
	if (g_AudioSpectrum16Left[1][2] > m) m = g_AudioSpectrum16Left[1][2];
	if (g_AudioSpectrum16Left[1][3] > m) m = g_AudioSpectrum16Left[1][3];
	if (g_AudioSpectrum16Left[2][0] > m) m = g_AudioSpectrum16Left[2][0];
	if (g_AudioSpectrum16Left[2][1] > m) m = g_AudioSpectrum16Left[2][1];
	if (g_AudioSpectrum16Left[2][2] > m) m = g_AudioSpectrum16Left[2][2];
	if (g_AudioSpectrum16Left[2][3] > m) m = g_AudioSpectrum16Left[2][3];
	if (g_AudioSpectrum16Left[3][0] > m) m = g_AudioSpectrum16Left[3][0];
	if (g_AudioSpectrum16Left[3][1] > m) m = g_AudioSpectrum16Left[3][1];
	if (g_AudioSpectrum16Left[3][2] > m) m = g_AudioSpectrum16Left[3][2];
	if (g_AudioSpectrum16Left[3][3] > m) m = g_AudioSpectrum16Left[3][3];
	return m;
}

float peak16r() {
	float m = 0.0;
	if (g_AudioSpectrum16Right[0][0] > m) m = g_AudioSpectrum16Right[0][0];
	if (g_AudioSpectrum16Right[0][1] > m) m = g_AudioSpectrum16Right[0][1];
	if (g_AudioSpectrum16Right[0][2] > m) m = g_AudioSpectrum16Right[0][2];
	if (g_AudioSpectrum16Right[0][3] > m) m = g_AudioSpectrum16Right[0][3];
	if (g_AudioSpectrum16Right[1][0] > m) m = g_AudioSpectrum16Right[1][0];
	if (g_AudioSpectrum16Right[1][1] > m) m = g_AudioSpectrum16Right[1][1];
	if (g_AudioSpectrum16Right[1][2] > m) m = g_AudioSpectrum16Right[1][2];
	if (g_AudioSpectrum16Right[1][3] > m) m = g_AudioSpectrum16Right[1][3];
	if (g_AudioSpectrum16Right[2][0] > m) m = g_AudioSpectrum16Right[2][0];
	if (g_AudioSpectrum16Right[2][1] > m) m = g_AudioSpectrum16Right[2][1];
	if (g_AudioSpectrum16Right[2][2] > m) m = g_AudioSpectrum16Right[2][2];
	if (g_AudioSpectrum16Right[2][3] > m) m = g_AudioSpectrum16Right[2][3];
	if (g_AudioSpectrum16Right[3][0] > m) m = g_AudioSpectrum16Right[3][0];
	if (g_AudioSpectrum16Right[3][1] > m) m = g_AudioSpectrum16Right[3][1];
	if (g_AudioSpectrum16Right[3][2] > m) m = g_AudioSpectrum16Right[3][2];
	if (g_AudioSpectrum16Right[3][3] > m) m = g_AudioSpectrum16Right[3][3];
	return m;
}

float peak32l() {
	float m = 0.0;
	if (g_AudioSpectrum32Left[0][0] > m) m = g_AudioSpectrum32Left[0][0];
	if (g_AudioSpectrum32Left[0][1] > m) m = g_AudioSpectrum32Left[0][1];
	if (g_AudioSpectrum32Left[0][2] > m) m = g_AudioSpectrum32Left[0][2];
	if (g_AudioSpectrum32Left[0][3] > m) m = g_AudioSpectrum32Left[0][3];
	if (g_AudioSpectrum32Left[1][0] > m) m = g_AudioSpectrum32Left[1][0];
	if (g_AudioSpectrum32Left[1][1] > m) m = g_AudioSpectrum32Left[1][1];
	if (g_AudioSpectrum32Left[1][2] > m) m = g_AudioSpectrum32Left[1][2];
	if (g_AudioSpectrum32Left[1][3] > m) m = g_AudioSpectrum32Left[1][3];
	if (g_AudioSpectrum32Left[2][0] > m) m = g_AudioSpectrum32Left[2][0];
	if (g_AudioSpectrum32Left[2][1] > m) m = g_AudioSpectrum32Left[2][1];
	if (g_AudioSpectrum32Left[2][2] > m) m = g_AudioSpectrum32Left[2][2];
	if (g_AudioSpectrum32Left[2][3] > m) m = g_AudioSpectrum32Left[2][3];
	if (g_AudioSpectrum32Left[3][0] > m) m = g_AudioSpectrum32Left[3][0];
	if (g_AudioSpectrum32Left[3][1] > m) m = g_AudioSpectrum32Left[3][1];
	if (g_AudioSpectrum32Left[3][2] > m) m = g_AudioSpectrum32Left[3][2];
	if (g_AudioSpectrum32Left[3][3] > m) m = g_AudioSpectrum32Left[3][3];
	if (g_AudioSpectrum32Left[4][0] > m) m = g_AudioSpectrum32Left[4][0];
	if (g_AudioSpectrum32Left[4][1] > m) m = g_AudioSpectrum32Left[4][1];
	if (g_AudioSpectrum32Left[4][2] > m) m = g_AudioSpectrum32Left[4][2];
	if (g_AudioSpectrum32Left[4][3] > m) m = g_AudioSpectrum32Left[4][3];
	if (g_AudioSpectrum32Left[5][0] > m) m = g_AudioSpectrum32Left[5][0];
	if (g_AudioSpectrum32Left[5][1] > m) m = g_AudioSpectrum32Left[5][1];
	if (g_AudioSpectrum32Left[5][2] > m) m = g_AudioSpectrum32Left[5][2];
	if (g_AudioSpectrum32Left[5][3] > m) m = g_AudioSpectrum32Left[5][3];
	if (g_AudioSpectrum32Left[6][0] > m) m = g_AudioSpectrum32Left[6][0];
	if (g_AudioSpectrum32Left[6][1] > m) m = g_AudioSpectrum32Left[6][1];
	if (g_AudioSpectrum32Left[6][2] > m) m = g_AudioSpectrum32Left[6][2];
	if (g_AudioSpectrum32Left[6][3] > m) m = g_AudioSpectrum32Left[6][3];
	if (g_AudioSpectrum32Left[7][0] > m) m = g_AudioSpectrum32Left[7][0];
	if (g_AudioSpectrum32Left[7][1] > m) m = g_AudioSpectrum32Left[7][1];
	if (g_AudioSpectrum32Left[7][2] > m) m = g_AudioSpectrum32Left[7][2];
	if (g_AudioSpectrum32Left[7][3] > m) m = g_AudioSpectrum32Left[7][3];
	return m;
}

float peak32r() {
	float m = 0.0;
	if (g_AudioSpectrum32Right[0][0] > m) m = g_AudioSpectrum32Right[0][0];
	if (g_AudioSpectrum32Right[0][1] > m) m = g_AudioSpectrum32Right[0][1];
	if (g_AudioSpectrum32Right[0][2] > m) m = g_AudioSpectrum32Right[0][2];
	if (g_AudioSpectrum32Right[0][3] > m) m = g_AudioSpectrum32Right[0][3];
	if (g_AudioSpectrum32Right[1][0] > m) m = g_AudioSpectrum32Right[1][0];
	if (g_AudioSpectrum32Right[1][1] > m) m = g_AudioSpectrum32Right[1][1];
	if (g_AudioSpectrum32Right[1][2] > m) m = g_AudioSpectrum32Right[1][2];
	if (g_AudioSpectrum32Right[1][3] > m) m = g_AudioSpectrum32Right[1][3];
	if (g_AudioSpectrum32Right[2][0] > m) m = g_AudioSpectrum32Right[2][0];
	if (g_AudioSpectrum32Right[2][1] > m) m = g_AudioSpectrum32Right[2][1];
	if (g_AudioSpectrum32Right[2][2] > m) m = g_AudioSpectrum32Right[2][2];
	if (g_AudioSpectrum32Right[2][3] > m) m = g_AudioSpectrum32Right[2][3];
	if (g_AudioSpectrum32Right[3][0] > m) m = g_AudioSpectrum32Right[3][0];
	if (g_AudioSpectrum32Right[3][1] > m) m = g_AudioSpectrum32Right[3][1];
	if (g_AudioSpectrum32Right[3][2] > m) m = g_AudioSpectrum32Right[3][2];
	if (g_AudioSpectrum32Right[3][3] > m) m = g_AudioSpectrum32Right[3][3];
	if (g_AudioSpectrum32Right[4][0] > m) m = g_AudioSpectrum32Right[4][0];
	if (g_AudioSpectrum32Right[4][1] > m) m = g_AudioSpectrum32Right[4][1];
	if (g_AudioSpectrum32Right[4][2] > m) m = g_AudioSpectrum32Right[4][2];
	if (g_AudioSpectrum32Right[4][3] > m) m = g_AudioSpectrum32Right[4][3];
	if (g_AudioSpectrum32Right[5][0] > m) m = g_AudioSpectrum32Right[5][0];
	if (g_AudioSpectrum32Right[5][1] > m) m = g_AudioSpectrum32Right[5][1];
	if (g_AudioSpectrum32Right[5][2] > m) m = g_AudioSpectrum32Right[5][2];
	if (g_AudioSpectrum32Right[5][3] > m) m = g_AudioSpectrum32Right[5][3];
	if (g_AudioSpectrum32Right[6][0] > m) m = g_AudioSpectrum32Right[6][0];
	if (g_AudioSpectrum32Right[6][1] > m) m = g_AudioSpectrum32Right[6][1];
	if (g_AudioSpectrum32Right[6][2] > m) m = g_AudioSpectrum32Right[6][2];
	if (g_AudioSpectrum32Right[6][3] > m) m = g_AudioSpectrum32Right[6][3];
	if (g_AudioSpectrum32Right[7][0] > m) m = g_AudioSpectrum32Right[7][0];
	if (g_AudioSpectrum32Right[7][1] > m) m = g_AudioSpectrum32Right[7][1];
	if (g_AudioSpectrum32Right[7][2] > m) m = g_AudioSpectrum32Right[7][2];
	if (g_AudioSpectrum32Right[7][3] > m) m = g_AudioSpectrum32Right[7][3];
	return m;
}

float peak64l() {
	float m = 0.0;
	if (g_AudioSpectrum64Left[0][0] > m) m = g_AudioSpectrum64Left[0][0];
	if (g_AudioSpectrum64Left[0][1] > m) m = g_AudioSpectrum64Left[0][1];
	if (g_AudioSpectrum64Left[0][2] > m) m = g_AudioSpectrum64Left[0][2];
	if (g_AudioSpectrum64Left[0][3] > m) m = g_AudioSpectrum64Left[0][3];
	if (g_AudioSpectrum64Left[1][0] > m) m = g_AudioSpectrum64Left[1][0];
	if (g_AudioSpectrum64Left[1][1] > m) m = g_AudioSpectrum64Left[1][1];
	if (g_AudioSpectrum64Left[1][2] > m) m = g_AudioSpectrum64Left[1][2];
	if (g_AudioSpectrum64Left[1][3] > m) m = g_AudioSpectrum64Left[1][3];
	if (g_AudioSpectrum64Left[2][0] > m) m = g_AudioSpectrum64Left[2][0];
	if (g_AudioSpectrum64Left[2][1] > m) m = g_AudioSpectrum64Left[2][1];
	if (g_AudioSpectrum64Left[2][2] > m) m = g_AudioSpectrum64Left[2][2];
	if (g_AudioSpectrum64Left[2][3] > m) m = g_AudioSpectrum64Left[2][3];
	if (g_AudioSpectrum64Left[3][0] > m) m = g_AudioSpectrum64Left[3][0];
	if (g_AudioSpectrum64Left[3][1] > m) m = g_AudioSpectrum64Left[3][1];
	if (g_AudioSpectrum64Left[3][2] > m) m = g_AudioSpectrum64Left[3][2];
	if (g_AudioSpectrum64Left[3][3] > m) m = g_AudioSpectrum64Left[3][3];
	if (g_AudioSpectrum64Left[4][0] > m) m = g_AudioSpectrum64Left[4][0];
	if (g_AudioSpectrum64Left[4][1] > m) m = g_AudioSpectrum64Left[4][1];
	if (g_AudioSpectrum64Left[4][2] > m) m = g_AudioSpectrum64Left[4][2];
	if (g_AudioSpectrum64Left[4][3] > m) m = g_AudioSpectrum64Left[4][3];
	if (g_AudioSpectrum64Left[5][0] > m) m = g_AudioSpectrum64Left[5][0];
	if (g_AudioSpectrum64Left[5][1] > m) m = g_AudioSpectrum64Left[5][1];
	if (g_AudioSpectrum64Left[5][2] > m) m = g_AudioSpectrum64Left[5][2];
	if (g_AudioSpectrum64Left[5][3] > m) m = g_AudioSpectrum64Left[5][3];
	if (g_AudioSpectrum64Left[6][0] > m) m = g_AudioSpectrum64Left[6][0];
	if (g_AudioSpectrum64Left[6][1] > m) m = g_AudioSpectrum64Left[6][1];
	if (g_AudioSpectrum64Left[6][2] > m) m = g_AudioSpectrum64Left[6][2];
	if (g_AudioSpectrum64Left[6][3] > m) m = g_AudioSpectrum64Left[6][3];
	if (g_AudioSpectrum64Left[7][0] > m) m = g_AudioSpectrum64Left[7][0];
	if (g_AudioSpectrum64Left[7][1] > m) m = g_AudioSpectrum64Left[7][1];
	if (g_AudioSpectrum64Left[7][2] > m) m = g_AudioSpectrum64Left[7][2];
	if (g_AudioSpectrum64Left[7][3] > m) m = g_AudioSpectrum64Left[7][3];
	if (g_AudioSpectrum64Left[8][0] > m) m = g_AudioSpectrum64Left[8][0];
	if (g_AudioSpectrum64Left[8][1] > m) m = g_AudioSpectrum64Left[8][1];
	if (g_AudioSpectrum64Left[8][2] > m) m = g_AudioSpectrum64Left[8][2];
	if (g_AudioSpectrum64Left[8][3] > m) m = g_AudioSpectrum64Left[8][3];
	if (g_AudioSpectrum64Left[9][0] > m) m = g_AudioSpectrum64Left[9][0];
	if (g_AudioSpectrum64Left[9][1] > m) m = g_AudioSpectrum64Left[9][1];
	if (g_AudioSpectrum64Left[9][2] > m) m = g_AudioSpectrum64Left[9][2];
	if (g_AudioSpectrum64Left[9][3] > m) m = g_AudioSpectrum64Left[9][3];
	if (g_AudioSpectrum64Left[10][0] > m) m = g_AudioSpectrum64Left[10][0];
	if (g_AudioSpectrum64Left[10][1] > m) m = g_AudioSpectrum64Left[10][1];
	if (g_AudioSpectrum64Left[10][2] > m) m = g_AudioSpectrum64Left[10][2];
	if (g_AudioSpectrum64Left[10][3] > m) m = g_AudioSpectrum64Left[10][3];
	if (g_AudioSpectrum64Left[11][0] > m) m = g_AudioSpectrum64Left[11][0];
	if (g_AudioSpectrum64Left[11][1] > m) m = g_AudioSpectrum64Left[11][1];
	if (g_AudioSpectrum64Left[11][2] > m) m = g_AudioSpectrum64Left[11][2];
	if (g_AudioSpectrum64Left[11][3] > m) m = g_AudioSpectrum64Left[11][3];
	if (g_AudioSpectrum64Left[12][0] > m) m = g_AudioSpectrum64Left[12][0];
	if (g_AudioSpectrum64Left[12][1] > m) m = g_AudioSpectrum64Left[12][1];
	if (g_AudioSpectrum64Left[12][2] > m) m = g_AudioSpectrum64Left[12][2];
	if (g_AudioSpectrum64Left[12][3] > m) m = g_AudioSpectrum64Left[12][3];
	if (g_AudioSpectrum64Left[13][0] > m) m = g_AudioSpectrum64Left[13][0];
	if (g_AudioSpectrum64Left[13][1] > m) m = g_AudioSpectrum64Left[13][1];
	if (g_AudioSpectrum64Left[13][2] > m) m = g_AudioSpectrum64Left[13][2];
	if (g_AudioSpectrum64Left[13][3] > m) m = g_AudioSpectrum64Left[13][3];
	if (g_AudioSpectrum64Left[14][0] > m) m = g_AudioSpectrum64Left[14][0];
	if (g_AudioSpectrum64Left[14][1] > m) m = g_AudioSpectrum64Left[14][1];
	if (g_AudioSpectrum64Left[14][2] > m) m = g_AudioSpectrum64Left[14][2];
	if (g_AudioSpectrum64Left[14][3] > m) m = g_AudioSpectrum64Left[14][3];
	if (g_AudioSpectrum64Left[15][0] > m) m = g_AudioSpectrum64Left[15][0];
	if (g_AudioSpectrum64Left[15][1] > m) m = g_AudioSpectrum64Left[15][1];
	if (g_AudioSpectrum64Left[15][2] > m) m = g_AudioSpectrum64Left[15][2];
	if (g_AudioSpectrum64Left[15][3] > m) m = g_AudioSpectrum64Left[15][3];
	return m;
}

float peak64r() {
	float m = 0.0;
	if (g_AudioSpectrum64Right[0][0] > m) m = g_AudioSpectrum64Right[0][0];
	if (g_AudioSpectrum64Right[0][1] > m) m = g_AudioSpectrum64Right[0][1];
	if (g_AudioSpectrum64Right[0][2] > m) m = g_AudioSpectrum64Right[0][2];
	if (g_AudioSpectrum64Right[0][3] > m) m = g_AudioSpectrum64Right[0][3];
	if (g_AudioSpectrum64Right[1][0] > m) m = g_AudioSpectrum64Right[1][0];
	if (g_AudioSpectrum64Right[1][1] > m) m = g_AudioSpectrum64Right[1][1];
	if (g_AudioSpectrum64Right[1][2] > m) m = g_AudioSpectrum64Right[1][2];
	if (g_AudioSpectrum64Right[1][3] > m) m = g_AudioSpectrum64Right[1][3];
	if (g_AudioSpectrum64Right[2][0] > m) m = g_AudioSpectrum64Right[2][0];
	if (g_AudioSpectrum64Right[2][1] > m) m = g_AudioSpectrum64Right[2][1];
	if (g_AudioSpectrum64Right[2][2] > m) m = g_AudioSpectrum64Right[2][2];
	if (g_AudioSpectrum64Right[2][3] > m) m = g_AudioSpectrum64Right[2][3];
	if (g_AudioSpectrum64Right[3][0] > m) m = g_AudioSpectrum64Right[3][0];
	if (g_AudioSpectrum64Right[3][1] > m) m = g_AudioSpectrum64Right[3][1];
	if (g_AudioSpectrum64Right[3][2] > m) m = g_AudioSpectrum64Right[3][2];
	if (g_AudioSpectrum64Right[3][3] > m) m = g_AudioSpectrum64Right[3][3];
	if (g_AudioSpectrum64Right[4][0] > m) m = g_AudioSpectrum64Right[4][0];
	if (g_AudioSpectrum64Right[4][1] > m) m = g_AudioSpectrum64Right[4][1];
	if (g_AudioSpectrum64Right[4][2] > m) m = g_AudioSpectrum64Right[4][2];
	if (g_AudioSpectrum64Right[4][3] > m) m = g_AudioSpectrum64Right[4][3];
	if (g_AudioSpectrum64Right[5][0] > m) m = g_AudioSpectrum64Right[5][0];
	if (g_AudioSpectrum64Right[5][1] > m) m = g_AudioSpectrum64Right[5][1];
	if (g_AudioSpectrum64Right[5][2] > m) m = g_AudioSpectrum64Right[5][2];
	if (g_AudioSpectrum64Right[5][3] > m) m = g_AudioSpectrum64Right[5][3];
	if (g_AudioSpectrum64Right[6][0] > m) m = g_AudioSpectrum64Right[6][0];
	if (g_AudioSpectrum64Right[6][1] > m) m = g_AudioSpectrum64Right[6][1];
	if (g_AudioSpectrum64Right[6][2] > m) m = g_AudioSpectrum64Right[6][2];
	if (g_AudioSpectrum64Right[6][3] > m) m = g_AudioSpectrum64Right[6][3];
	if (g_AudioSpectrum64Right[7][0] > m) m = g_AudioSpectrum64Right[7][0];
	if (g_AudioSpectrum64Right[7][1] > m) m = g_AudioSpectrum64Right[7][1];
	if (g_AudioSpectrum64Right[7][2] > m) m = g_AudioSpectrum64Right[7][2];
	if (g_AudioSpectrum64Right[7][3] > m) m = g_AudioSpectrum64Right[7][3];
	if (g_AudioSpectrum64Right[8][0] > m) m = g_AudioSpectrum64Right[8][0];
	if (g_AudioSpectrum64Right[8][1] > m) m = g_AudioSpectrum64Right[8][1];
	if (g_AudioSpectrum64Right[8][2] > m) m = g_AudioSpectrum64Right[8][2];
	if (g_AudioSpectrum64Right[8][3] > m) m = g_AudioSpectrum64Right[8][3];
	if (g_AudioSpectrum64Right[9][0] > m) m = g_AudioSpectrum64Right[9][0];
	if (g_AudioSpectrum64Right[9][1] > m) m = g_AudioSpectrum64Right[9][1];
	if (g_AudioSpectrum64Right[9][2] > m) m = g_AudioSpectrum64Right[9][2];
	if (g_AudioSpectrum64Right[9][3] > m) m = g_AudioSpectrum64Right[9][3];
	if (g_AudioSpectrum64Right[10][0] > m) m = g_AudioSpectrum64Right[10][0];
	if (g_AudioSpectrum64Right[10][1] > m) m = g_AudioSpectrum64Right[10][1];
	if (g_AudioSpectrum64Right[10][2] > m) m = g_AudioSpectrum64Right[10][2];
	if (g_AudioSpectrum64Right[10][3] > m) m = g_AudioSpectrum64Right[10][3];
	if (g_AudioSpectrum64Right[11][0] > m) m = g_AudioSpectrum64Right[11][0];
	if (g_AudioSpectrum64Right[11][1] > m) m = g_AudioSpectrum64Right[11][1];
	if (g_AudioSpectrum64Right[11][2] > m) m = g_AudioSpectrum64Right[11][2];
	if (g_AudioSpectrum64Right[11][3] > m) m = g_AudioSpectrum64Right[11][3];
	if (g_AudioSpectrum64Right[12][0] > m) m = g_AudioSpectrum64Right[12][0];
	if (g_AudioSpectrum64Right[12][1] > m) m = g_AudioSpectrum64Right[12][1];
	if (g_AudioSpectrum64Right[12][2] > m) m = g_AudioSpectrum64Right[12][2];
	if (g_AudioSpectrum64Right[12][3] > m) m = g_AudioSpectrum64Right[12][3];
	if (g_AudioSpectrum64Right[13][0] > m) m = g_AudioSpectrum64Right[13][0];
	if (g_AudioSpectrum64Right[13][1] > m) m = g_AudioSpectrum64Right[13][1];
	if (g_AudioSpectrum64Right[13][2] > m) m = g_AudioSpectrum64Right[13][2];
	if (g_AudioSpectrum64Right[13][3] > m) m = g_AudioSpectrum64Right[13][3];
	if (g_AudioSpectrum64Right[14][0] > m) m = g_AudioSpectrum64Right[14][0];
	if (g_AudioSpectrum64Right[14][1] > m) m = g_AudioSpectrum64Right[14][1];
	if (g_AudioSpectrum64Right[14][2] > m) m = g_AudioSpectrum64Right[14][2];
	if (g_AudioSpectrum64Right[14][3] > m) m = g_AudioSpectrum64Right[14][3];
	if (g_AudioSpectrum64Right[15][0] > m) m = g_AudioSpectrum64Right[15][0];
	if (g_AudioSpectrum64Right[15][1] > m) m = g_AudioSpectrum64Right[15][1];
	if (g_AudioSpectrum64Right[15][2] > m) m = g_AudioSpectrum64Right[15][2];
	if (g_AudioSpectrum64Right[15][3] > m) m = g_AudioSpectrum64Right[15][3];
	return m;
}

float shaderBit(float b) {
	if (b < 16.0) return bitOf(floor(clamp((g_PointerPosition.x + 4.0) * 4096.0 + 0.5, 0.0, 65535.0)), b - 0.0);
	if (b < 32.0) return bitOf(floor(clamp((g_PointerPosition.y + 4.0) * 4096.0 + 0.5, 0.0, 65535.0)), b - 16.0);
	if (b < 56.0) return bitOf(floor(clamp((g_EyePosition.x + 32768.0) * 256.0 + 0.5, 0.0, 16777215.0)), b - 32.0);
	if (b < 80.0) return bitOf(floor(clamp((g_EyePosition.y + 32768.0) * 256.0 + 0.5, 0.0, 16777215.0)), b - 56.0);
	if (b < 104.0) return bitOf(floor(clamp((g_EyePosition.z + 32768.0) * 256.0 + 0.5, 0.0, 16777215.0)), b - 80.0);
	if (b < 112.0) return bitOf(floor(clamp((peak16l() + 0.0) * 255.0 + 0.5, 0.0, 255.0)), b - 104.0);
	if (b < 120.0) return bitOf(floor(clamp((peak16r() + 0.0) * 255.0 + 0.5, 0.0, 255.0)), b - 112.0);
	if (b < 128.0) return bitOf(floor(clamp((peak32l() + 0.0) * 255.0 + 0.5, 0.0, 255.0)), b - 120.0);
	if (b < 136.0) return bitOf(floor(clamp((peak32r() + 0.0) * 255.0 + 0.5, 0.0, 255.0)), b - 128.0);
	if (b < 144.0) return bitOf(floor(clamp((peak64l() + 0.0) * 255.0 + 0.5, 0.0, 255.0)), b - 136.0);
	if (b < 152.0) return bitOf(floor(clamp((peak64r() + 0.0) * 255.0 + 0.5, 0.0, 255.0)), b - 144.0);
	return 0.0;
}

float scriptBit(float b) {
	if (b < 12.0) return bitOf((u_press < 0.0 ? 0.0 : u_press), b - 0.0);
	if (b < 28.0) return bitOf((u_date_day < 0.0 ? 0.0 : u_date_day), b - 12.0);
	if (b < 52.0) return bitOf((u_date_centis < 0.0 ? 0.0 : u_date_centis), b - 28.0);
	if (b < 63.0) return bitOf((u_timezone < 0.0 ? 0.0 : u_timezone), b - 52.0);
	if (b < 87.0) return bitOf((u_camerax < 0.0 ? 0.0 : u_camerax), b - 63.0);
	if (b < 111.0) return bitOf((u_cameray < 0.0 ? 0.0 : u_cameray), b - 87.0);
	if (b < 135.0) return bitOf((u_cameraz < 0.0 ? 0.0 : u_cameraz), b - 111.0);
	return 0.0;
}

float band(float i) {
	if (i < 0.5) return g_AudioSpectrum64Left[0][0];
	if (i < 1.5) return g_AudioSpectrum64Left[0][1];
	if (i < 2.5) return g_AudioSpectrum64Left[0][2];
	if (i < 3.5) return g_AudioSpectrum64Left[0][3];
	if (i < 4.5) return g_AudioSpectrum64Left[1][0];
	if (i < 5.5) return g_AudioSpectrum64Left[1][1];
	if (i < 6.5) return g_AudioSpectrum64Left[1][2];
	if (i < 7.5) return g_AudioSpectrum64Left[1][3];
	if (i < 8.5) return g_AudioSpectrum64Left[2][0];
	if (i < 9.5) return g_AudioSpectrum64Left[2][1];
	if (i < 10.5) return g_AudioSpectrum64Left[2][2];
	if (i < 11.5) return g_AudioSpectrum64Left[2][3];
	if (i < 12.5) return g_AudioSpectrum64Left[3][0];
	if (i < 13.5) return g_AudioSpectrum64Left[3][1];
	if (i < 14.5) return g_AudioSpectrum64Left[3][2];
	if (i < 15.5) return g_AudioSpectrum64Left[3][3];
	if (i < 16.5) return g_AudioSpectrum64Left[4][0];
	if (i < 17.5) return g_AudioSpectrum64Left[4][1];
	if (i < 18.5) return g_AudioSpectrum64Left[4][2];
	if (i < 19.5) return g_AudioSpectrum64Left[4][3];
	if (i < 20.5) return g_AudioSpectrum64Left[5][0];
	if (i < 21.5) return g_AudioSpectrum64Left[5][1];
	if (i < 22.5) return g_AudioSpectrum64Left[5][2];
	if (i < 23.5) return g_AudioSpectrum64Left[5][3];
	if (i < 24.5) return g_AudioSpectrum64Left[6][0];
	if (i < 25.5) return g_AudioSpectrum64Left[6][1];
	if (i < 26.5) return g_AudioSpectrum64Left[6][2];
	if (i < 27.5) return g_AudioSpectrum64Left[6][3];
	if (i < 28.5) return g_AudioSpectrum64Left[7][0];
	if (i < 29.5) return g_AudioSpectrum64Left[7][1];
	if (i < 30.5) return g_AudioSpectrum64Left[7][2];
	if (i < 31.5) return g_AudioSpectrum64Left[7][3];
	if (i < 32.5) return g_AudioSpectrum64Left[8][0];
	if (i < 33.5) return g_AudioSpectrum64Left[8][1];
	if (i < 34.5) return g_AudioSpectrum64Left[8][2];
	if (i < 35.5) return g_AudioSpectrum64Left[8][3];
	if (i < 36.5) return g_AudioSpectrum64Left[9][0];
	if (i < 37.5) return g_AudioSpectrum64Left[9][1];
	if (i < 38.5) return g_AudioSpectrum64Left[9][2];
	if (i < 39.5) return g_AudioSpectrum64Left[9][3];
	if (i < 40.5) return g_AudioSpectrum64Left[10][0];
	if (i < 41.5) return g_AudioSpectrum64Left[10][1];
	if (i < 42.5) return g_AudioSpectrum64Left[10][2];
	if (i < 43.5) return g_AudioSpectrum64Left[10][3];
	if (i < 44.5) return g_AudioSpectrum64Left[11][0];
	if (i < 45.5) return g_AudioSpectrum64Left[11][1];
	if (i < 46.5) return g_AudioSpectrum64Left[11][2];
	if (i < 47.5) return g_AudioSpectrum64Left[11][3];
	if (i < 48.5) return g_AudioSpectrum64Left[12][0];
	if (i < 49.5) return g_AudioSpectrum64Left[12][1];
	if (i < 50.5) return g_AudioSpectrum64Left[12][2];
	if (i < 51.5) return g_AudioSpectrum64Left[12][3];
	if (i < 52.5) return g_AudioSpectrum64Left[13][0];
	if (i < 53.5) return g_AudioSpectrum64Left[13][1];
	if (i < 54.5) return g_AudioSpectrum64Left[13][2];
	if (i < 55.5) return g_AudioSpectrum64Left[13][3];
	if (i < 56.5) return g_AudioSpectrum64Left[14][0];
	if (i < 57.5) return g_AudioSpectrum64Left[14][1];
	if (i < 58.5) return g_AudioSpectrum64Left[14][2];
	if (i < 59.5) return g_AudioSpectrum64Left[14][3];
	if (i < 60.5) return g_AudioSpectrum64Left[15][0];
	if (i < 61.5) return g_AudioSpectrum64Left[15][1];
	if (i < 62.5) return g_AudioSpectrum64Left[15][2];
	if (i < 63.5) return g_AudioSpectrum64Left[15][3];
	if (i < 64.5) return g_AudioSpectrum64Right[0][0];
	if (i < 65.5) return g_AudioSpectrum64Right[0][1];
	if (i < 66.5) return g_AudioSpectrum64Right[0][2];
	if (i < 67.5) return g_AudioSpectrum64Right[0][3];
	if (i < 68.5) return g_AudioSpectrum64Right[1][0];
	if (i < 69.5) return g_AudioSpectrum64Right[1][1];
	if (i < 70.5) return g_AudioSpectrum64Right[1][2];
	if (i < 71.5) return g_AudioSpectrum64Right[1][3];
	if (i < 72.5) return g_AudioSpectrum64Right[2][0];
	if (i < 73.5) return g_AudioSpectrum64Right[2][1];
	if (i < 74.5) return g_AudioSpectrum64Right[2][2];
	if (i < 75.5) return g_AudioSpectrum64Right[2][3];
	if (i < 76.5) return g_AudioSpectrum64Right[3][0];
	if (i < 77.5) return g_AudioSpectrum64Right[3][1];
	if (i < 78.5) return g_AudioSpectrum64Right[3][2];
	if (i < 79.5) return g_AudioSpectrum64Right[3][3];
	if (i < 80.5) return g_AudioSpectrum64Right[4][0];
	if (i < 81.5) return g_AudioSpectrum64Right[4][1];
	if (i < 82.5) return g_AudioSpectrum64Right[4][2];
	if (i < 83.5) return g_AudioSpectrum64Right[4][3];
	if (i < 84.5) return g_AudioSpectrum64Right[5][0];
	if (i < 85.5) return g_AudioSpectrum64Right[5][1];
	if (i < 86.5) return g_AudioSpectrum64Right[5][2];
	if (i < 87.5) return g_AudioSpectrum64Right[5][3];
	if (i < 88.5) return g_AudioSpectrum64Right[6][0];
	if (i < 89.5) return g_AudioSpectrum64Right[6][1];
	if (i < 90.5) return g_AudioSpectrum64Right[6][2];
	if (i < 91.5) return g_AudioSpectrum64Right[6][3];
	if (i < 92.5) return g_AudioSpectrum64Right[7][0];
	if (i < 93.5) return g_AudioSpectrum64Right[7][1];
	if (i < 94.5) return g_AudioSpectrum64Right[7][2];
	if (i < 95.5) return g_AudioSpectrum64Right[7][3];
	if (i < 96.5) return g_AudioSpectrum64Right[8][0];
	if (i < 97.5) return g_AudioSpectrum64Right[8][1];
	if (i < 98.5) return g_AudioSpectrum64Right[8][2];
	if (i < 99.5) return g_AudioSpectrum64Right[8][3];
	if (i < 100.5) return g_AudioSpectrum64Right[9][0];
	if (i < 101.5) return g_AudioSpectrum64Right[9][1];
	if (i < 102.5) return g_AudioSpectrum64Right[9][2];
	if (i < 103.5) return g_AudioSpectrum64Right[9][3];
	if (i < 104.5) return g_AudioSpectrum64Right[10][0];
	if (i < 105.5) return g_AudioSpectrum64Right[10][1];
	if (i < 106.5) return g_AudioSpectrum64Right[10][2];
	if (i < 107.5) return g_AudioSpectrum64Right[10][3];
	if (i < 108.5) return g_AudioSpectrum64Right[11][0];
	if (i < 109.5) return g_AudioSpectrum64Right[11][1];
	if (i < 110.5) return g_AudioSpectrum64Right[11][2];
	if (i < 111.5) return g_AudioSpectrum64Right[11][3];
	if (i < 112.5) return g_AudioSpectrum64Right[12][0];
	if (i < 113.5) return g_AudioSpectrum64Right[12][1];
	if (i < 114.5) return g_AudioSpectrum64Right[12][2];
	if (i < 115.5) return g_AudioSpectrum64Right[12][3];
	if (i < 116.5) return g_AudioSpectrum64Right[13][0];
	if (i < 117.5) return g_AudioSpectrum64Right[13][1];
	if (i < 118.5) return g_AudioSpectrum64Right[13][2];
	if (i < 119.5) return g_AudioSpectrum64Right[13][3];
	if (i < 120.5) return g_AudioSpectrum64Right[14][0];
	if (i < 121.5) return g_AudioSpectrum64Right[14][1];
	if (i < 122.5) return g_AudioSpectrum64Right[14][2];
	if (i < 123.5) return g_AudioSpectrum64Right[14][3];
	if (i < 124.5) return g_AudioSpectrum64Right[15][0];
	if (i < 125.5) return g_AudioSpectrum64Right[15][1];
	if (i < 126.5) return g_AudioSpectrum64Right[15][2];
	if (i < 127.5) return g_AudioSpectrum64Right[15][3];
	return 0.0;
}

float bandBit(float b) {
	if (b >= 1024.0) return 0.0;
	return bitOf(floor(clamp((band(floor(b / 8.0)) + 0.0) * 255.0 + 0.5, 0.0, 255.0)), mod(b, 8.0));
}

void main() {
	vec2 uv = v_TexCoord.xy;
	vec4 scene = texSample2D(g_Texture0, uv);
	float cell = floor(uv.x * 80.0) - 1.0;
	float row = floor(uv.y * 45.0);
	float across = step(0.0, cell) * step(cell, 13.5);
	float shader = across * (step(abs(row - 1.0), 0.1) + step(abs(row - 43.0), 0.1));
	float script = across * (step(abs(row - 3.0), 0.1) + step(abs(row - 41.0), 0.1));
	float frame = script > 0.5 ? u_ScriptFrame : floor(g_Time * 30.0 + 0.5);
	float started = step(-0.5, frame);
	float bit = mod(floor(max(frame, 0.0) / pow(2.0, max(cell - 1.0, 0.0))), 2.0);
	float value = cell < 0.5 ? started : (cell > 12.5 ? 1.0 - started : bit * started);
	vec4 colour = mix(scene, vec4(value, value, value, 1.0), saturate(shader + script));
	{
		float j = 39.0 - row;
		if (row < 22.0) j = row - 5.0;
		if (j > -0.5 && j < 0.5 && cell > -0.5) {
			float count = min(64.0, 51.0 - j * 64.0);
			float started = 1.0;
			if (cell < 0.5) colour = vec4(started, started, started, 1.0);
			else if (cell < count + 0.5) {
				float b = (j * 64.0 + cell - 1.0) * 3.0;
				colour = vec4(vec3(shaderBit(b), shaderBit(b + 1.0), shaderBit(b + 2.0)) * started, 1.0);
			} else if (cell < count + 1.5) colour = vec4(1.0 - started, 1.0 - started, 1.0 - started, 1.0);
		}
	}
	{
		float j = 37.0 - row;
		if (row < 22.0) j = row - 7.0;
		if (j > -0.5 && j < 0.5 && cell > -0.5) {
			float count = min(64.0, 45.0 - j * 64.0);
			float started = step(-0.5, u_press);
			if (cell < 0.5) colour = vec4(started, started, started, 1.0);
			else if (cell < count + 0.5) {
				float b = (j * 64.0 + cell - 1.0) * 3.0;
				colour = vec4(vec3(scriptBit(b), scriptBit(b + 1.0), scriptBit(b + 2.0)) * started, 1.0);
			} else if (cell < count + 1.5) colour = vec4(1.0 - started, 1.0 - started, 1.0 - started, 1.0);
		}
	}
	{
		float j = 35.0 - row;
		if (row < 22.0) j = row - 9.0;
		if (j > -0.5 && j < 5.5 && cell > -0.5) {
			float count = min(64.0, 342.0 - j * 64.0);
			float started = 1.0;
			if (cell < 0.5) colour = vec4(started, started, started, 1.0);
			else if (cell < count + 0.5) {
				float b = (j * 64.0 + cell - 1.0) * 3.0;
				colour = vec4(vec3(bandBit(b), bandBit(b + 1.0), bandBit(b + 2.0)) * started, 1.0);
			} else if (cell < count + 1.5) colour = vec4(1.0 - started, 1.0 - started, 1.0 - started, 1.0);
		}
	}
	gl_FragColor = colour;
}
