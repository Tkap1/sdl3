#version 450 core

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec3 v_world_pos;
layout (location = 3) in vec4 v_light_frag_pos;

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
	vec3 normal = v_normal;
	vec3 light_dir = normalize(vec3(1, 1, -2));
	float m = dot(-light_dir, normal);
	m = max(m, 0.0) * 0.5 + 0.5;
	// m = max(m, 0.0);
	vec3 color = v_color.rgb * m;
	vec3 temp0 = v_light_frag_pos.xyz / v_light_frag_pos.w;
	temp0 = temp0 * 0.5 + 0.5;
	// temp0.y = 1.0 - temp0.y;
	float closest_depth = texture(shadow_map, temp0.xy).r;
	float curr_depth = temp0.z;
	float shadow = (curr_depth > closest_depth) ? 1.0 : 0.0;
	color *= 1.0 - shadow;

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		fog start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	// vec3 fog_color = vec3(0.2, 0.2, 0.3);
	// float cam_dist = distance(cam_pos, v_world_pos);
	// float fog_dt = smoothstep(100, 250, cam_dist);
	// color = mix(color, fog_color, fog_dt);
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		fog end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// color = vec3(closest_depth);
	// color = vec3(temp0.x, temp0.y, 0.0);
	// vec3 color = normal * 0.5 + 0.5;
	out_color = vec4(color, v_color.a);
}