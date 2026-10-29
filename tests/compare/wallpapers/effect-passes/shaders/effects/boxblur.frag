varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec4 g_Texture0Resolution;
uniform float u_radius; // {"material":"radius","label":"Radius","default":2,"range":[0,10]}

void main() {
#if VERTICAL
	vec2 offset = vec2(0.0, 1.0 / g_Texture0Resolution.y);
#else
	vec2 offset = vec2(1.0 / g_Texture0Resolution.x, 0.0);
#endif
	// (a loop with a texture sample in it does not compile in the real Wallpaper Engine)
	vec4 sum = texSample2D(g_Texture0, v_TexCoord.xy - offset * 2.0 * u_radius)
		+ texSample2D(g_Texture0, v_TexCoord.xy - offset * u_radius)
		+ texSample2D(g_Texture0, v_TexCoord.xy)
		+ texSample2D(g_Texture0, v_TexCoord.xy + offset * u_radius)
		+ texSample2D(g_Texture0, v_TexCoord.xy + offset * 2.0 * u_radius);
	gl_FragColor = sum / 5.0;
}
