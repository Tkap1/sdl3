#version 450 core

layout (location = 0) in mat4 model;
layout (location = 4) in vec4 color;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec2 v_world_pos;
layout (location = 2) out vec2 v_local_uv;

layout (set = 1, binding = 0) uniform uniform_block {
	mat4 view;
	mat4 projection;
};

void main()
{
	const float size = 0.5f;
	const vec3 vertex_arr[6] = vec3[](
		vec3(-size, -size, 0.0),
		vec3(size, -size, 0.0),
		vec3(size, size, 0.0),
		vec3(-size, -size, 0.0),
		vec3(size, size, 0.0),
		vec3(-size, size, 0.0)
	);
	const vec2 uv_arr[6] = vec2[](
		vec2(0, 1),
		vec2(1, 1),
		vec2(1, 0),
		vec2(0, 1),
		vec2(1, 0),
		vec2(0, 0)
	);
	vec3 vertex = vertex_arr[gl_VertexIndex];

	vec4 pos = projection * view * model * vec4(vertex, 1.0);
	gl_Position = pos;
	v_world_pos = (model * vec4(vertex, 1)).xy;
	v_color = color;
	v_local_uv = uv_arr[gl_VertexIndex];
}