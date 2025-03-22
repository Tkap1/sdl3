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
	// temp0.z is already in 0 to 1 range, since that is what depth writing expects
	temp0.xy = temp0.xy * 0.5 + 0.5;

	// depth texture is flipped
	float closest_depth = texture(shadow_map, vec2(temp0.x, 1 - temp0.y)).r;

	float curr_depth = temp0.z;
	float bias = 0.005;
	float shadow = (curr_depth > (closest_depth + bias)) ? 1.0 : 0.0;
	float shadow_strength = 0.5;
	color *= (1.0 - shadow * shadow_strength);

	#if 0
	float depth_distance = (curr_depth - closest_depth) * 0.5 + 0.5;
	// depth_distance = max(0, closest_depth - curr_depth);
	depth_distance = pow(depth_distance, 0.5);
	color = vec3(depth_distance, 0, clamp(temp0.y, 0, 1) * 2);

	color = vec3(closest_depth, 0, 0);
	#endif

	// show out of bounds sampling
	#if 1
	if ((temp0.x < 0) || (temp0.x > 1))
		color = vec3(0, 0, 1);
	else if ((temp0.y < 0) || (temp0.y > 1))
		color = vec3(0, 0, 1);
	else if ((temp0.z < 0) || (temp0.z > 1))
		color.b = 1;
	#endif

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