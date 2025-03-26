#version 450 core

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec2 v_world_pos;
layout (location = 2) in vec2 v_local_uv;

layout (location = 0) out vec4 out_color;

void main()
{
	float d = distance(vec2(0.5), v_local_uv);
	vec4 color = v_color;
	color.a *= smoothstep(0.5, 0.45, d);
	out_color = color;
}