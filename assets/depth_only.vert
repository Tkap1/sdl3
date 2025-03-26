#version 450 core

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in mat4 model;

layout (set = 1, binding = 0) uniform uniform_block {
	mat4 view;
	mat4 projection;
};

void main()
{
	vec4 pos = projection * view * model * vec4(vertex, 1.0);
	gl_Position = pos;
}