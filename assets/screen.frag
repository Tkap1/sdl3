#version 450 core

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec2 v_uv;

layout (location = 0) out vec4 out_color;

layout (set = 2, binding = 0) uniform sampler2D in_texture;

void main()
{
	vec4 color = texture(in_texture, v_uv);
	out_color = color * v_color;
}