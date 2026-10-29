varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec3 u_color; // {"material":"color","label":"Colour","type":"color","default":"1 0 0"}
uniform float u_amount; // {"material":"amount","label":"Amount","default":0.5,"range":[0,1]}

void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	albedo.rgb = mix(albedo.rgb, u_color, u_amount);
	gl_FragColor = albedo;
}
