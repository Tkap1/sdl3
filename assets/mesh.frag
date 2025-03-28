#version 450 core

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec3 v_world_pos;
layout (location = 3) in vec4 v_light_frag_pos;
layout (location = 4) in flat int v_flags;

layout (location = 0) out vec4 out_color;

layout (set = 2, binding = 0) uniform sampler2D shadow_map;

layout (set = 3, binding = 0) uniform uniform_block {
	vec3 cam_pos;
};

void main()
{
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

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		out of bounds start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	#if 1
	if(temp0.x < 0 || temp0.x > 1) {
		color = vec3(0, 0, 1);
	}
	else if(temp0.y < 0 || temp0.y > 1) {
		color = vec3(0, 0, 1);
	}
	else if(temp0.z < 0 || temp0.z > 1) {
		color = vec3(0, 0, 1);
	}
	#endif
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		out of bounds end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	out_color = vec4(color, v_color.a);
}