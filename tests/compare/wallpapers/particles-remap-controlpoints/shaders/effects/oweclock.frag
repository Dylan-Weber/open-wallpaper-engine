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
	gl_FragColor = colour;
}
