#version 450 core

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec2 v_uv;

layout (location = 0) out vec4 out_color;

layout (set = 2, binding = 0) uniform sampler2D in_texture;

void main()
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