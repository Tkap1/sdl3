#version 450 core

#if m_vertex
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 vertex_color;
layout (location = 3) in vec2 uv;
layout (location = 4) in vec4 instance_color;
layout (location = 5) in int flags;
layout (location = 6) in mat4 model;

layout (set = 1, binding = 0) uniform uniform_block {
	mat4 world_view;
	mat4 world_projection;
	mat4 screen_view;
	mat4 screen_projection;
	mat4 light_view;
	mat4 light_projection;
	int depth_only;
};
#endif

layout (location = 0) shared_var vec4 v_color;
layout (location = 1) shared_var vec3 v_normal;
layout (location = 2) shared_var vec3 v_world_pos;
layout (location = 3) shared_var vec4 v_light_frag_pos;
layout (location = 4) shared_var flat int v_flags;
layout (location = 5) shared_var vec2 v_uv;

#if m_vertex
void vertex_main()
{
	vec4 pos;
	if(bool(flags & 1 << 3)) {
		pos = screen_projection * screen_view * model * vec4(vertex, 1.0);
	}
	else {
		pos = world_projection * world_view * model * vec4(vertex, 1.0);
	}

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
	v_flags = flags;
	v_uv = uv;
}
#endif

#if m_fragment
void fragment_main()
{
}
#endif
