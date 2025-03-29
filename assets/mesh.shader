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

#if m_fragment
layout (location = 0) out vec4 out_color;

layout (set = 2, binding = 0) uniform sampler2D shadow_map;

layout (set = 3, binding = 0) uniform uniform_block {
	vec3 cam_pos;
};
#endif


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
	bool textured = bool(v_flags & (1 << 4));
	// vec3 normal = normalize(
	// 	cross(
	// 		dFdx(v_world_pos),
	// 		dFdy(v_world_pos)
	// 	)
	// ) * -1;
	vec3 normal = normalize(v_normal);
	vec3 light_dir = normalize(vec3(1, 0, -0.1));
	float m = dot(-light_dir, normal);
	m = max(m, 0.0) * 0.5 + 0.5;
	vec3 temp0 = v_light_frag_pos.xyz / v_light_frag_pos.w;
	temp0.xy = temp0.xy * 0.5 + 0.5;

	float closest_depth = texture(shadow_map, vec2(temp0.x, 1 - temp0.y)).r;
	float curr_depth = temp0.z;
	float bias = 0.003;
	float shadow = (curr_depth > closest_depth + bias) ? 1.0 : 0.0;
	float inv_shadow = 1.0 - shadow;

	vec3 light_color = vec3(1, 1, 1);
	float ambient_strength = 0.3;
	float specular_strength = 0.5;

	vec3 ambient = light_color * ambient_strength;
	vec3 diffuse = light_color * m * inv_shadow;
	vec3 specular = vec3(0);
	{
		vec3 view_dir = normalize(cam_pos - v_world_pos);
		vec3 reflect_dir = reflect(light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		specular = light_color * spec * specular_strength * inv_shadow;
	}

	vec3 color = v_color.rgb;

	// @Note(tkap, 28/03/2025): Ignore lights
	if(!bool(v_flags & (1 << 1))) {
		color *= (ambient + diffuse);
		color += specular;
	}

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		fog start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	vec3 fog_color = vec3(0.2, 0.2, 0.3);
	float cam_dist = distance(cam_pos, v_world_pos);
	float fog_dt = smoothstep(200, 350, cam_dist);

	// @Note(tkap, 28/03/2025): Ignore fog
	if(!bool(v_flags & (1 << 2))) {
		color = mix(color, fog_color, fog_dt);
	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		fog end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	if(textured) {
		color.rgb = vec3(texture(shadow_map, v_uv).r);
	}

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		out of bounds start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	#if 1
	if(!bool(v_flags & (1 << 1))) {
		if(temp0.x < 0 || temp0.x > 1) {
			color = vec3(0, 0, 1);
		}
		else if(temp0.y < 0 || temp0.y > 1) {
			color = vec3(0, 0, 1);
		}
		else if(temp0.z < 0 || temp0.z > 1) {
			color = vec3(0, 0, 1);
		}
	}
	#endif
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		out of bounds end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	out_color = vec4(color, v_color.a);
}
#endif
