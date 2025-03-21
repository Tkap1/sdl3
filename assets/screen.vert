#version 450 core

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec2 v_uv;

layout (set = 1, binding = 0) uniform uniform_block {
	vec4 color;
};


void main()
{
	const float size = 1.0f;
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
	vec4 pos = vec4(vertex_arr[gl_VertexIndex], 1.0);
	gl_Position = pos;
	v_uv = uv_arr[gl_VertexIndex];
	v_color = color;
}