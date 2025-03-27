#version 450 core

layout (location = 0) in vec3 v0;
layout (location = 1) in vec3 v1;
layout (location = 2) in vec3 v2;
layout (location = 3) in vec3 normal;
layout (location = 4) in vec2 uv;
layout (location = 5) in vec4 color;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec3 v_normal;
layout (location = 2) out vec3 v_world_pos;
layout (location = 3) out vec4 v_light_frag_pos;

layout (set = 1, binding = 0) uniform uniform_block {
	mat4 view;
	mat4 projection;
	mat4 light_view;
	mat4 light_projection;
};

void main()
{
	vec3 vertex_arr[3] = vec3[](v0, v1, v2);
	vec3 vertex = vertex_arr[gl_VertexIndex];
	vec4 pos = projection * view * vec4(vertex, 1.0);
	gl_Position = pos;
	v_world_pos = vertex;
	v_color = color;
	v_normal = normal;
	v_light_frag_pos = light_projection * light_view * vec4(vertex, 1.0);
}
