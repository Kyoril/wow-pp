#version 120

uniform mat4 g_WorldViewprojMatrix;	// Wvp Matrix
uniform mat4 g_TexMatrix;			// Texture matrix

attribute vec4 vertex;				// Vertex coords in object space
attribute vec4 uv0;					// Texture coordinates
attribute vec3 normal;				// Normalized normal

uniform vec3 g_eyePosition;
uniform vec4 g_lightPosition;

varying vec4 out_Pos;
varying vec3 out_Normal;
varying vec4 out_LightPos;
varying vec3 out_EyePos;

void main()
{
	// Calculate new position of this vertex in screen space
	gl_Position = g_WorldViewprojMatrix * vertex;
	gl_TexCoord[0] = g_TexMatrix * uv0;
	
	out_Pos = vertex;
	out_Normal = normal;
	out_LightPos = g_lightPosition;
	out_EyePos = g_eyePosition;
}
