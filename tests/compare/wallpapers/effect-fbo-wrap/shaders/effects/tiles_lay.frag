varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform sampler2D g_Texture1; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	vec3 tile = texSample2D(g_Texture1, v_TexCoord.xy * 3.0).rgb;
	gl_FragColor = vec4(albedo.rgb * 0.25 + tile * 0.75, albedo.a);
}
