#version 450 core

layout (location = 0) shared_var vec4 v_color;
layout (location = 1) shared_var vec2 v_uv;

#if m_vertex
layout (set = 1, binding = 0) uniform uniform_block {
	vec4 color;
};
#endif

#if m_fragment
layout (location = 0) out vec4 out_color;
layout (set = 2, binding = 0) uniform sampler2D in_texture;
#endif


#if m_vertex
void vertex_main()
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
#endif

#if m_fragment
void fragment_main()
{
	float r = texture(in_texture, v_uv).r;
	// vec3 color = vec3(r);
	vec3 color;
	if(r <= 0.01) {
		color = vec3(0, 1,0 );
	}
	else {
		color = vec3(r);
	}
	out_color = vec4(color, 1) * v_color;
}
#endif