#version 450 core

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 vertex_color;
layout (location = 3) in vec2 uv;
layout (location = 4) in vec4 instance_color;
layout (location = 5) in int flags;
layout (location = 6) in mat4 model;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec3 v_normal;
layout (location = 2) out vec3 v_world_pos;
layout (location = 3) out vec4 v_light_frag_pos;

layout (set = 1, binding = 0) uniform uniform_block {
	mat4 view;
	mat4 projection;
	mat4 light_view;
	mat4 light_projection;
	int depth_only;
};

void main()
{
	vec4 pos = projection * view * model * vec4(vertex, 1.0);

	// if we are on depth-only pass and we don't want to cast a shadow, "discard" this vertex
	if(depth_only > 0 && bool(flags & 1)) {
		gl_Position = vec4(0, 0, 100, 0);
	}
	else {
		gl_Position = pos;
	}
	v_world_pos = (model * vec4(vertex, 1)).xyz;
	v_color = vertex_color * instance_color;
	v_normal = normal;
	v_light_frag_pos = light_projection * light_view * model * vec4(vertex, 1.0);
}
