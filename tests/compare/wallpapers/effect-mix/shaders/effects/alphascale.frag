varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform float u_scale; // {"material":"scale","label":"Opacity","default":1,"range":[0,1]}

void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	albedo.a *= u_scale;
	gl_FragColor = albedo;
}
