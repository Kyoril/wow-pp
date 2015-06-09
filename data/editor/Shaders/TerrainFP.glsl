#version 120

uniform sampler2D alpha;			// Coverage map
uniform sampler2D splat1;			// Splatting map 1
uniform sampler2D splat2;			// Splatting map 2
uniform sampler2D splat3;			// Splatting map 3
uniform sampler2D splat4;			// Splatting map 4
uniform float Scale;				// Splatting scale

uniform vec3 g_AmbientLight;
uniform vec3 g_DiffuseLight;

varying vec4 out_Pos;
varying vec3 out_Normal;
varying vec4 out_LightPos;
varying vec3 out_EyePos;

vec4 lit(float NdotL, float NdotH, float m) 
{ 
  float ambient = 1.0;
  float diffuse = max(NdotL, 0.0);
  float specular = step(0.0, NdotL) * max(NdotH * m, 0.0);
 
  return vec4(ambient, diffuse, specular, 1.0);
}

void main()	
{
	vec4 coverage = texture2D(alpha, gl_TexCoord[0].st);
	
	vec2 scaledUV = gl_TexCoord[0].st * Scale;
	vec4 diffuse01 = texture2D(splat1, scaledUV);
	vec4 diffuse02 = texture2D(splat2, scaledUV);
	vec4 diffuse03 = texture2D(splat3, scaledUV);
	vec4 diffuse04 = texture2D(splat4, scaledUV);
	
	vec4 finalDiffuse = 
		diffuse01 * (1.0 - (coverage.x + coverage.y + coverage.z)) + 
		diffuse02 * coverage.x + 
		diffuse03 * coverage.y + 
		diffuse04 * coverage.z;
	
	vec3 N = normalize(out_Normal);
	vec3 EyeDir = normalize(out_EyePos - out_Pos.xyz);
	vec3 LightDir = normalize(out_LightPos.xyz - (out_Pos.xyz * out_LightPos.w));
	vec3 HalfAngle = normalize(LightDir + EyeDir);
	
	float NdotL = dot(LightDir, N);
	float NdotH = dot(HalfAngle, N);
	vec4 Lit = lit(NdotL, NdotH, 128.0);
	
	vec3 texColor = finalDiffuse.xyz;
	gl_FragColor = vec4(g_DiffuseLight * Lit.y * texColor + g_AmbientLight * texColor, 1.0);
}
