#version 450 core

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec3 v_normal;
layout (location = 2) out vec3 v_world_pos;

layout (set = 1, binding = 0) uniform uniform_block {
	mat4 model;
	mat4 view;
	mat4 projection;
};

void main()
{
	vec4 pos = projection * view * model * vec4(vertex, 1.0);
	// pos.z = pos.z * 0.5 + 0.5;
	gl_Position = pos;
	v_world_pos = (model * vec4(vertex, 1.0)).xyz;
	v_color = color;
	v_normal = normal;
}