
// to get slope dir
// RayMarch: rotate normal, project player velocity onto that vector, then stretch it back to the speed (if this is about wall sliding)

// what if we just add the triangle normal to the player's velocity

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "SDL3/SDL.h"
#include "shaderc/shaderc.h"

#define FNL_IMPL
#include "FastNoiseLite.h"

#include "tk_types.h"

#ifdef _WIN32
#include "intrin.h"
#else
#include "immintrin.h"
#endif

#include "main.h"

global constexpr int c_tiles_x = 512;
global constexpr int c_tiles_y = 512;
global constexpr int c_vertex_count = c_tiles_x * c_tiles_y * 6;
global constexpr float c_tile_size = 1.0f;
global constexpr float c_pi = 3.1415926535f;
global constexpr int c_window_width = 1920;
global constexpr int c_window_height = 1080;
global constexpr s_v2 c_window_size = {c_window_width, c_window_height};
global constexpr s_v2 c_half_window_size = {c_window_width * 0.5f, c_window_height * 0.5f};
global constexpr float c_margin = 8;
global constexpr float c_padding = 8;
global constexpr int c_updates_per_second = 165;
global constexpr f64 c_update_delay = 1.0 / c_updates_per_second;
global constexpr s_v3 c_player_size = v3(0.1f, 0.1f, 6.0f);
global constexpr int c_max_mesh_instances = 1024;

global SDL_GPUTexture* scene_depth_texture;
global SDL_GPUTexture* shadow_texture;
global SDL_GPUSampler* shadow_texture_sampler;
global b8 g_do_wireframe = false;
global s_player g_player;
global SDL_GPUDevice* g_device;
global SDL_Window* g_window;
global SDL_GPUTextureFormat g_depth_texture_format = SDL_GPU_TEXTUREFORMAT_INVALID;
global s_list<s_sphere, c_max_mesh_instances> g_sphere_arr;
global s_mesh g_mesh_arr[e_mesh_count];
global s_mesh_instance_data g_mesh_instance_data[e_mesh_count][c_max_mesh_instances];
global s_list<s_mesh_instance_data, c_max_mesh_instances> g_mesh_instance_data_arr[e_mesh_count];
global s_linear_arena g_frame_arena;
global s_vertex g_terrain_vertex_arr[c_vertex_count];
global shaderc_compiler_t g_shader_compiler;
global SDL_Time g_last_shader_modify_time;
global b8 g_reload_shaders = true;
global s_ui g_ui = {.hot_index = -1};
global s_v2 g_mouse;
global s_v2 g_mouse_delta;
global b8 g_left_down = false;
global b8 g_left_down_this_frame = false;
global SDL_AudioStream* g_audio_stream;
global s_list<s_playing_sound, 64> g_sound_to_play_arr;
global s_sound g_sound_pop;
global s_sound g_sound_cakez;
global f64 g_accumulator = 0;
global s_game g_game;
global SDL_GPUCommandBuffer* g_cmdbuf;

#include "input.cpp"
#include "update.cpp"
#include "render.cpp"


int main()
{
	g_frame_arena = make_arena_from_malloc(1024 * 1024 * 1024);
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	g_player.pos.x = 10;
	g_player.pos.y = 10;
	g_player.pos.z = 20;
	g_player.prev_pos = g_player.pos;

	s_ply_mesh ply_sphere = parse_ply_mesh("assets/sphere.ply");

	g_device = SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV,
		true,
		null
	);

	g_window = SDL_CreateWindow("3D", c_window_width, c_window_height, 0);
	SDL_ClaimWindowForGPUDevice(g_device, g_window);

	SDL_AudioStream* audio = null;
	{
		SDL_AudioSpec src_spec = zero;
		src_spec.format = SDL_AUDIO_S16LE;
		src_spec.channels = 2;
		src_spec.freq = 44100;
		audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &src_spec, audio_callback, null);
		assert(audio);
		SDL_ResumeAudioStreamDevice(audio);

		g_sound_pop = load_sound("assets/pop.wav");
		g_sound_cakez = load_sound("assets/cakez.wav");
	}

	// prefered depth texture format in order of preference
	SDL_GPUTextureFormat prefered_depth_formats[] = {
		SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
		SDL_GPU_TEXTUREFORMAT_D24_UNORM,
		SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
		SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
	};

	for(int i = 0; i < array_count(prefered_depth_formats); i += 1) {
		SDL_GPUTextureFormat format = prefered_depth_formats[i];
		b8 is_supported = SDL_GPUTextureSupportsFormat(g_device, (SDL_GPUTextureFormat) format, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
		// printf("texture format index: %i, value: %i is supported = %s\n", i, format, is_supported ? "true" : "false");
		if(is_supported) {
			g_depth_texture_format = format;
			break;
		}
	}

	// your device does not support any required depth texture format
	assert(g_depth_texture_format != SDL_GPU_TEXTUREFORMAT_INVALID);

	{
		SDL_GPUTextureCreateInfo info = zero;
		info.type = SDL_GPU_TEXTURETYPE_2D;
		info.width = c_window_width;
		info.height = c_window_height;
		info.layer_count_or_depth = 1;
		info.num_levels = 1;
		info.sample_count = SDL_GPU_SAMPLECOUNT_1;
		info.format = g_depth_texture_format;
		info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
		scene_depth_texture = SDL_CreateGPUTexture(g_device, &info);
	}

	{
		SDL_GPUTextureCreateInfo info = zero;
		info.type = SDL_GPU_TEXTURETYPE_2D;
		info.width = 2048;
		info.height = 2048;
		info.layer_count_or_depth = 1;
		info.num_levels = 1;
		info.sample_count = SDL_GPU_SAMPLECOUNT_1;
		info.format = g_depth_texture_format;
		info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
		shadow_texture = SDL_CreateGPUTexture(g_device, &info);
	}

	{
		SDL_GPUSamplerCreateInfo info = zero;
		info.min_filter = SDL_GPU_FILTER_NEAREST;
		info.mag_filter = SDL_GPU_FILTER_NEAREST;
		// info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		// info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		// info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		shadow_texture_sampler = SDL_CreateGPUSampler(g_device, &info);
	}

	SDL_SetWindowRelativeMouseMode(g_window, true);

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		make meshes start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	{
		make_game_mesh_from_ply_mesh(&g_mesh_arr[e_mesh_sphere], &ply_sphere);

		{
			s_mesh* mesh = &g_mesh_arr[e_mesh_quad];
			mesh->vertex_count = 6;
			setup_common_mesh_stuff(mesh);
			setup_mesh_vertex_buffers(mesh, sizeof(s_vertex) * 6);
			s_vertex vertex_arr[6] = zero;
			constexpr float c_size = 0.5f;
			vertex_arr[0].pos = v3(-c_size, -c_size, 0.0f);
			vertex_arr[0].normal = v3(0, -1, 0);
			vertex_arr[0].color = make_color(1);
			vertex_arr[0].uv = v2(0, 0);
			vertex_arr[1].pos = v3(c_size, -c_size, 0.0f);
			vertex_arr[1].normal = v3(0, -1, 0);
			vertex_arr[1].color = make_color(1);
			vertex_arr[1].uv = v2(1, 0);
			vertex_arr[2].pos = v3(c_size, c_size, 0.0f);
			vertex_arr[2].normal = v3(0, -1, 0);
			vertex_arr[2].color = make_color(1);
			vertex_arr[2].uv = v2(1, 1);
			vertex_arr[3].pos = v3(-c_size, -c_size, 0.0f);
			vertex_arr[3].normal = v3(0, -1, 0);
			vertex_arr[3].color = make_color(1);
			vertex_arr[3].uv = v2(0, 0);
			vertex_arr[4].pos = v3(c_size, c_size, 0.0f);
			vertex_arr[4].normal = v3(0, -1, 0);
			vertex_arr[4].color = make_color(1);
			vertex_arr[4].uv = v2(1, 1);
			vertex_arr[5].pos = v3(-c_size, c_size, 0.0f);
			vertex_arr[5].normal = v3(0, -1, 0);
			vertex_arr[5].color = make_color(1);
			vertex_arr[5].uv = v2(0, 1);
			upload_to_gpu_buffer(vertex_arr, sizeof(s_vertex) * mesh->vertex_count, mesh->vertex_buffer, mesh->vertex_transfer_buffer);
		}

		{
			s_mesh* mesh = &g_mesh_arr[e_mesh_terrain];
			setup_common_mesh_stuff(mesh);
			setup_mesh_vertex_buffers(mesh, sizeof(s_vertex) * c_vertex_count);
			mesh->vertex_count = c_vertex_count;
		}
	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		make meshes end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		loop start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	float ticks_before = (float)SDL_GetTicks();
	while(!g_game.quit) {
		float ticks = (float)SDL_GetTicks();
		float delta = (ticks - ticks_before) / 1000;
		ticks_before = ticks;

		g_left_down_this_frame = false;
		g_mouse_delta = zero;

		handle_input();

		g_accumulator += delta;
		while(g_accumulator >= c_update_delay) {
			g_accumulator -= c_update_delay;
			update();
		}

		float interp_dt = (float)(g_accumulator / c_update_delay);
		render(interp_dt);
		g_game.render_time += delta;

		arena_reset(&g_frame_arena);
	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		loop end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	return 0;
}

func s_shader_program load_shader(char* path, s_shader_data shader_data)
{
	s_shader_program result = zero;
	SDL_GPUShaderStage stage_arr[2] = {SDL_GPU_SHADERSTAGE_VERTEX, SDL_GPU_SHADERSTAGE_FRAGMENT};
	shaderc_shader_kind stage2_arr[2] = {shaderc_vertex_shader, shaderc_fragment_shader};
	char* entry_point_arr[2] = {"vertex_main", "fragment_main"};
	char* shader_src = (char*)read_file(path);
	assert(shader_src);
	for(int i = 0; i < 2; i += 1) {

		shaderc_compile_options_t shader_options = shaderc_compile_options_initialize();
		if(i == 0) {
			shaderc_compile_options_add_macro_definition(
				shader_options, "m_vertex", 8, "1", 1
			);
			shaderc_compile_options_add_macro_definition(
				shader_options, "vertex_main", 11, "main", 4
			);
			shaderc_compile_options_add_macro_definition(
				shader_options, "shared_var", 10, "out", 3
			);
		}
		else {
			shaderc_compile_options_add_macro_definition(
				shader_options, "m_fragment", 10, "1", 1
			);
			shaderc_compile_options_add_macro_definition(
				shader_options, "fragment_main", 13, "main", 4
			);
			shaderc_compile_options_add_macro_definition(
				shader_options, "shared_var", 10, "in", 2
			);
		}
		shaderc_compilation_result_t compile_result = shaderc_compile_into_spv(g_shader_compiler, shader_src, strlen(shader_src), stage2_arr[i], path, entry_point_arr[i], shader_options);

		int num_warnings = (int)shaderc_result_get_num_warnings(compile_result);
		int num_errors = (int)shaderc_result_get_num_errors(compile_result);
		b8 make_shader = true;
		if(num_warnings > 0) {
			const char* str = shaderc_result_get_error_message(compile_result);
			printf("SHADER WARNING: %s\n", str);
			make_shader = false;
		}
		if(num_errors > 0) {
			const char* str = shaderc_result_get_error_message(compile_result);
			printf("SHADER ERROR: %s\n", str);
			make_shader = false;
		}

		if(make_shader) {
			SDL_GPUShaderCreateInfo shaderInfo = zero;
			shaderInfo.code = (u8*)shaderc_result_get_bytes(compile_result);
			shaderInfo.code_size = shaderc_result_get_length(compile_result);
			// shaderInfo.entrypoint = entry_point_arr[i];
			shaderInfo.entrypoint = "main";
			shaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
			shaderInfo.stage = stage_arr[i];
			shaderInfo.num_samplers = shader_data.sampler_count[i];
			shaderInfo.num_uniform_buffers = shader_data.uniform_buffer_count[i];
			shaderInfo.num_storage_buffers = shader_data.storage_buffer_count[i];
			shaderInfo.num_storage_textures = shader_data.storage_texture_count[i];

			result.shader_arr[i] = SDL_CreateGPUShader(g_device, &shaderInfo);
			if(result.shader_arr[i] == null) {
				SDL_Log("Failed to create shader!");
			}
		}
	}
	return result;
}

func s_m4 m4_identity()
{
	s_m4 result = zero;
	result.all2[0][0] = 1;
	result.all2[1][1] = 1;
	result.all2[2][2] = 1;
	result.all2[3][3] = 1;
	return result;
}

func s_m4 m4_rotate(float angle, s_v3 axis)
{

	s_m4 result = m4_identity();

	axis = v3_normalized(axis);

	float SinTheta = sinf(angle);
	float CosTheta = cosf(angle);
	float CosValue = 1.0f - CosTheta;

	result.all2[0][0] = (axis.x * axis.x * CosValue) + CosTheta;
	result.all2[0][1] = (axis.x * axis.y * CosValue) + (axis.z * SinTheta);
	result.all2[0][2] = (axis.x * axis.z * CosValue) - (axis.y * SinTheta);

	result.all2[1][0] = (axis.y * axis.x * CosValue) - (axis.z * SinTheta);
	result.all2[1][1] = (axis.y * axis.y * CosValue) + CosTheta;
	result.all2[1][2] = (axis.y * axis.z * CosValue) + (axis.x * SinTheta);

	result.all2[2][0] = (axis.z * axis.x * CosValue) + (axis.y * SinTheta);
	result.all2[2][1] = (axis.z * axis.y * CosValue) - (axis.x * SinTheta);
	result.all2[2][2] = (axis.z * axis.z * CosValue) + CosTheta;

	return result;
}

func s_v3 v3_normalized(s_v3 v)
{
	s_v3 result = v;
	float length = v3_length(v);
	if(length != 0) {
		result.x /= length;
		result.y /= length;
		result.z /= length;
	}
	return result;
}

func float v3_length_squared(s_v3 v)
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

func float v3_length(s_v3 v)
{
	return sqrtf(v3_length_squared(v));
}

func s_m4 make_perspective(float FOV, float AspectRatio, float Near, float Far)
{
	s_m4 Result = zero;

	// See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

	float Cotangent = 1.0f / tanf(FOV * (c_pi / 360.0f));

	Result.all2[0][0] = Cotangent / AspectRatio;

	Result.all2[1][1] = Cotangent;

	Result.all2[2][3] = -1.0f;
	Result.all2[2][2] = (Near + Far) / (Near - Far);
	Result.all2[3][2] = (2.0f * Near * Far) / (Near - Far);
	Result.all2[3][3] = 0.0f;

	return (Result);
}

func s_m4 look_at(s_v3 eye, s_v3 target, s_v3 up)
{
	s_m4 world_to_cam = zero;
	s_v3 front = v3_normalized(target - eye);
	s_v3 side = v3_normalized(v3_cross(front, up));
	s_v3 top = v3_normalized(v3_cross(side, front));

	world_to_cam.all[0] = side.x;
	world_to_cam.all[1] = top.x;
	world_to_cam.all[2] = -front.x;
	world_to_cam.all[3] = 0;

	world_to_cam.all[4] = side.y;
	world_to_cam.all[5] = top.y;
	world_to_cam.all[6] = -front.y;
	world_to_cam.all[7] = 0;

	world_to_cam.all[8] = side.z;
	world_to_cam.all[9] = top.z;
	world_to_cam.all[10] = -front.z;
	world_to_cam.all[11] = 0;

	s_v3 x = v3(world_to_cam.all[0], world_to_cam.all[4], world_to_cam.all[8]);
	s_v3 y = v3(world_to_cam.all[1], world_to_cam.all[5], world_to_cam.all[9]);
	s_v3 z = v3(world_to_cam.all[2], world_to_cam.all[6], world_to_cam.all[10]);

	world_to_cam.all[12] = -v3_dot(x, eye);
	world_to_cam.all[13] = -v3_dot(y, eye);
	world_to_cam.all[14] = -v3_dot(z, eye);
	world_to_cam.all[15] = 1.0f;

	return world_to_cam;
}

func constexpr s_v3 operator-(s_v3 a, s_v3 b)
{
	return v3(
		a.x - b.x,
		a.y - b.y,
		a.z - b.z
	);
}

func s_v3 v3_cross(s_v3 a, s_v3 b)
{
	s_v3 Result;

	Result.x = (a.y * b.z) - (a.z * b.y);
	Result.y = (a.z * b.x) - (a.x * b.z);
	Result.z = (a.x * b.y) - (a.y * b.x);

	return (Result);
}

func float v3_dot(s_v3 a, s_v3 b)
{
	float Result = (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
	return (Result);
}

template <typename t>
func t clamp(t curr, t min_val, t max_val)
{
	t result = curr;
	if(curr < min_val) {
		result = min_val;
	}
	if(curr > max_val) {
		result = max_val;
	}
	return result;
}

func float smoothstep(float edge0, float edge1, float x)
{
	// Scale, bias and saturate x to 0..1 range
	x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	// Evaluate polynomial
	return x * x * (3 - 2 * x);
}

func float smoothstep2(float edge0, float edge1, float x)
{
	float result = smoothstep(edge0, edge1, x) * smoothstep(edge1, edge0, x);
	return result;
}

func float lerp(float a, float b, float t)
{
	float result = a + (b - a) * t;
	return result;
}

func s_v3 lerp_v3(s_v3 a, s_v3 b, float t)
{
	s_v3 result;
	result.x = lerp(a.x, b.x, t);
	result.y = lerp(a.y, b.y, t);
	result.z = lerp(a.z, b.z, t);
	return result;
}

func float ilerp(float a, float b, float c)
{
	float result = (c - a) / (b - a);
	return result;
}

func s_v3 get_triangle_normal(s_triangle triangle)
{
	s_v3 t1 = triangle.vertex_arr[1] - triangle.vertex_arr[0];
	s_v3 t2 = triangle.vertex_arr[2] - triangle.vertex_arr[0];
	s_v3 normal = v3_cross(t2, t1);
	return v3_normalized(normal);
}

func int roundfi(float x)
{
	float result = roundf(x);
	return (int)result;
}

func int floorfi(float x)
{
	float result = floorf(x);
	return (int)result;
}

func SDL_GPUGraphicsPipeline* create_pipeline(
	s_shader_program shader, SDL_GPUFillMode fill_mode, int num_color_targets,
	s_list<SDL_GPUVertexElementFormat, 16> vertex_attributes, s_list<SDL_GPUVertexElementFormat, 16> instance_attributes,
	b8 has_depth
)
{
	SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = zero;
	pipeline_create_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_create_info.vertex_shader = shader.shader_arr[0];
	pipeline_create_info.fragment_shader = shader.shader_arr[1];
	pipeline_create_info.target_info.num_color_targets = num_color_targets;
	pipeline_create_info.target_info.has_depth_stencil_target = has_depth;
	pipeline_create_info.depth_stencil_state.enable_depth_test = has_depth;
	pipeline_create_info.depth_stencil_state.enable_depth_write = has_depth;
	pipeline_create_info.target_info.depth_stencil_format = g_depth_texture_format;
	pipeline_create_info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
	pipeline_create_info.depth_stencil_state.write_mask = 0xFF;
	SDL_GPUColorTargetDescription color_target_description = zero;
	color_target_description.format = SDL_GetGPUSwapchainTextureFormat(g_device, g_window);
	color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
	color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;

	// @Note(tkap, 25/03/2025): no idea what this does
	color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

	color_target_description.blend_state.enable_blend = true;
	pipeline_create_info.target_info.color_target_descriptions = num_color_targets > 0 ? &color_target_description : null;
	{
		int count = 0;
		if(vertex_attributes.count > 0) { count += 1; }
		if(instance_attributes.count > 0) { count += 1; }
		pipeline_create_info.vertex_input_state.num_vertex_buffers = count;
	}
	SDL_GPUVertexBufferDescription gpu_vertex_buffer_description_arr[2] = zero;
	gpu_vertex_buffer_description_arr[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	gpu_vertex_buffer_description_arr[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
	gpu_vertex_buffer_description_arr[0].slot = 0;
	if(vertex_attributes.count <= 0) {
		gpu_vertex_buffer_description_arr[1].slot = 0;
	}
	else {
		gpu_vertex_buffer_description_arr[1].slot = 1;
	}

	int first_gpu_vertex_buffer_description = 0;
	if(vertex_attributes.count <= 0) {
		first_gpu_vertex_buffer_description = 1;
	}

	int attribute_count = 0;
	s_list<SDL_GPUVertexElementFormat, 16>* temp[2] = {&vertex_attributes, &instance_attributes};
	SDL_GPUVertexAttribute vertex_attribute_arr[16] = zero;
	int buffer_count = 0;
	for(int i = 0; i < 2; i += 1) {
		int pitch = 0;
		int count = temp[i]->count;
		for(int j = 0; j < count; j += 1) {
			vertex_attribute_arr[attribute_count].format = temp[i]->data[j];
			vertex_attribute_arr[attribute_count].location = attribute_count;
			vertex_attribute_arr[attribute_count].offset = pitch;
			vertex_attribute_arr[attribute_count].buffer_slot = buffer_count;

			switch(temp[i]->data[j]) {
				case SDL_GPU_VERTEXELEMENTFORMAT_INT: {
					pitch += sizeof(int);
				} break;
				case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2: {
					pitch += sizeof(float) * 2;
				} break;
				case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3: {
					pitch += sizeof(float) * 3;
				} break;
				case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4: {
					pitch += sizeof(float) * 4;
				} break;
				invalid_default_case;
			}
			attribute_count += 1;
		}
		if(count > 0) {
			buffer_count += 1;
		}
		gpu_vertex_buffer_description_arr[i].pitch = pitch;
	}

	pipeline_create_info.vertex_input_state.vertex_buffer_descriptions = &gpu_vertex_buffer_description_arr[first_gpu_vertex_buffer_description];
	pipeline_create_info.vertex_input_state.num_vertex_attributes = attribute_count;
	pipeline_create_info.vertex_input_state.vertex_attributes = vertex_attribute_arr;

	pipeline_create_info.rasterizer_state.fill_mode = fill_mode;
	// pipeline_create_info.rasterizer_state.enable_depth_clip = true;
	SDL_GPUGraphicsPipeline* result = SDL_CreateGPUGraphicsPipeline(g_device, &pipeline_create_info);
	if(result == null) {
		SDL_Log("Failed to create fill pipeline!");
		return null;
	}
	return result;
}

func s_m4 make_orthographic(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
	s_m4 Result = zero;

	// xy values are mapped to -1 to 1 range for viewport mapping
	// z values are mapped to 0 to 1 range instead of -1 to 1 for depth writing
	Result.all2[0][0] = 2.0f / (Right - Left);
	Result.all2[1][1] = 2.0f / (Top - Bottom);
	Result.all2[2][2] = 1.0f / (Near - Far);
	Result.all2[3][3] = 1.0f;

	Result.all2[3][0] = (Left + Right) / (Left - Right);
	Result.all2[3][1] = (Bottom + Top) / (Bottom - Top);
	Result.all2[3][2] = 0.5f * (Near + Far) / (Near - Far);

	return (Result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

func void getProjection(float* min, float* max, s_v3* vertices, int count, s_v3 axis) {
		*min = *max = v3_dot(vertices[0], axis);
		for (int i = 1; i < count; i += 1) {
				float proj = v3_dot(vertices[i], axis);
				if (proj < *min) *min = proj;
				if (proj > *max) *max = proj;
		}
}

func b8 overlap(float minA, float maxA, float minB, float maxB)
{
		return minA <= maxB && minB <= maxA;
}

func b8 SATCollision3D(s_shape shapeA, s_shape shapeB)
{
		// Test face normals of shapeA
		for (int i = 0; i < shapeA.vertex_count - 2; i += 3) {
			s_v3 v1 = shapeA.vertices[i + 1] - shapeA.vertices[i];
			s_v3 v2 = shapeA.vertices[i + 2] - shapeA.vertices[i];
			s_v3 axis = v3_cross(v1, v2);

			float minA, maxA, minB, maxB;
			getProjection(&minA, &maxA, shapeA.vertices, shapeA.vertex_count, axis);
			getProjection(&minB, &maxB, shapeB.vertices, shapeB.vertex_count, axis);

			if(!overlap(minA, maxA, minB, maxB)) {
				return false;
			}
		}

		// Test face normals of shapeB
		for (int i = 0; i < shapeB.vertex_count - 2; i += 3) {
				s_v3 v1 = shapeB.vertices[i + 1] - shapeB.vertices[i];
				s_v3 v2 = shapeB.vertices[i + 2] - shapeB.vertices[i];
				s_v3 axis = v3_cross(v1, v2);

				float minA, maxA, minB, maxB;
				getProjection(&minA, &maxA, shapeA.vertices, shapeA.vertex_count, axis);
				getProjection(&minB, &maxB, shapeB.vertices, shapeB.vertex_count, axis);

				if(!overlap(minA, maxA, minB, maxB)) {
					return false;
				}
		}

		// Test edge cross products
		for (int i = 0; i < shapeA.vertex_count; i += 1) {
			for (int j = 0; j < shapeB.vertex_count; j += 1) {
				s_v3 edgeA = shapeA.vertices[(i + 1) % shapeA.vertex_count] - shapeA.vertices[i];
				s_v3 edgeB = shapeB.vertices[(j + 1) % shapeB.vertex_count] - shapeB.vertices[j];
				s_v3 axis = v3_cross(edgeA, edgeB);

				float minA, maxA, minB, maxB;
				getProjection(&minA, &maxA, shapeA.vertices, shapeA.vertex_count, axis);
				getProjection(&minB, &maxB, shapeB.vertices, shapeB.vertex_count, axis);

				if (!overlap(minA, maxA, minB, maxB)) {
						return false;
				}
			}
		}

		return true;
}

func float get_triangle_height_at_xy(s_v3 t1, s_v3 t2, s_v3 t3, s_v2 p)
{
	// Calculate barycentric coordinates
	// Area of the full triangle using cross product
	float det = (t2.x - t1.x) * (t3.y - t1.y) - (t3.x - t1.x) * (t2.y - t1.y);

	// Edge case: degenerate triangle (area is zero)
	if (fabs(det) < 0.0001f) {
			// Handle degenerate case - could return an error code or average z
			return (t1.z + t2.z + t3.z) / 3.0f;
	}

	// Calculate barycentric coordinates
	float lambda1 = ((t3.y - t1.y) * (p.x - t1.x) - (t3.x - t1.x) * (p.y - t1.y)) / det;
	float lambda2 = ((t1.y - t2.y) * (p.x - t1.x) - (t1.x - t2.x) * (p.y - t1.y)) / det;
	float lambda3 = 1.0f - lambda1 - lambda2;

	// Check if point is inside triangle
	if(lambda1 >= 0.0f && lambda2 >= 0.0f && lambda3 >= 0.0f) {
			// Interpolate z value using barycentric coordinates
			return lambda1 * t2.z + lambda2 * t3.z + lambda3 * t1.z;
	}
	else {
			// Point is outside triangle
			// Option 1: Return an error code (e.g., NaN or a special value)
			// Option 2: Project onto triangle anyway (current implementation)
			return lambda1 * t2.z + lambda2 * t3.z + lambda3 * t1.z;
	}
}

template <typename t>
func t max(t a, t b)
{
	t result = a >= b ? a : b;
	return result;
}

func float min(float a, float b)
{
	float result = a <= b ? a : b;
	return result;
}

func float at_most(float a, float b)
{
	float result = b >= a ? a : b;
	return result;
}

func float sign(float x)
{
	float result = 1;
	if(x < 0) {
		result = -1;
	}
	return result;
}

func s_v3 v3_set_mag(s_v3 v, float mag)
{
	s_v3 result = v3_normalized(v);
	result = result * mag;
	return result;
}

func s_v4 make_color(float r)
{
	s_v4 result;
	result.x = r;
	result.y = r;
	result.z = r;
	result.w = 1;
	return result;
}

func s_v4 make_color(float r, float a)
{
	s_v4 result;
	result.x = r;
	result.y = r;
	result.z = r;
	result.w = a;
	return result;
}

func s_v4 make_color(float r, float g, float b)
{
	s_v4 result;
	result.x = r;
	result.y = g;
	result.z = b;
	result.w = 1;
	return result;
}

func void upload_to_gpu_buffer(void* data, int data_size, SDL_GPUBuffer* vertex_buffer, SDL_GPUTransferBuffer* transfer_buffer)
{
	void* buffer = SDL_MapGPUTransferBuffer(g_device, transfer_buffer, false);
	// nocheckin extra cringe copy. we probably want begin upload and end upload so we can write directly to this instead of writing to our own thing
	// and then doing this memcpy
	memcpy(buffer, data, data_size);
	SDL_UnmapGPUTransferBuffer(g_device, transfer_buffer);
	SDL_GPUCommandBuffer* upload_cmd_buff = SDL_AcquireGPUCommandBuffer(g_device);
	SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_cmd_buff);

	SDL_GPUTransferBufferLocation location = zero;
	location.transfer_buffer = transfer_buffer;

	SDL_GPUBufferRegion region = zero;
	region.buffer = vertex_buffer;
	region.size = data_size;

	SDL_UploadToGPUBuffer(copy_pass, &location, &region, false);
	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(upload_cmd_buff);
	// SDL_ReleaseGPUTransferBuffer(g_device, transferBuffer);
}

func s_m4 m4_scale(s_v3 v)
{
	s_m4 result = {
		v.x, 0, 0, 0,
		0, v.y, 0, 0,
		0, 0, v.z, 0,
		0, 0, 0, 1,
	};
	return result;
}

func s_m4 m4_translate(s_v3 v)
{
	s_m4 result = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		v.x, v.y, v.z, 1,
	};
	return result;
}

func s_m4 m4_multiply(s_m4 a, s_m4 b)
{
	s_m4 result = zero;
	for(int i = 0; i < 4; i += 1) {
		for(int j = 0; j < 4; j += 1) {
			for(int k = 0; k < 4; k += 1) {
				result.all2[j][i] += a.all2[k][i] * b.all2[j][k];
			}
		}
	}

	return result;
}

func s_v3 v3_reflect(s_v3 a, s_v3 b)
{
	float dot = v3_dot(a, b) * 2;
	s_v3 result;
	result.x = a.x - b.x * dot;
	result.y = a.y - b.y * dot;
	result.z = a.z - b.z * dot;
	return result;
}

func s_collision_data check_collision(s_v3 pos, s_box hitbox)
{
	s_collision_data result = zero;
	s_shape a = zero;
	static_assert(sizeof(a.vertices) >= sizeof(hitbox.vertex_arr));
	memcpy(a.vertices, hitbox.vertex_arr, sizeof(hitbox.vertex_arr));
	// a.vertices[0] = v3(pos.x - c_player_size.x * 0.5f, pos.y + c_player_size.y * 0.5f, pos.z + c_player_size.z * 0.5f);
	// a.vertices[1] = v3(pos.x + c_player_size.x * 0.5f, pos.y + c_player_size.y * 0.5f, pos.z + c_player_size.z * 0.5f);
	// a.vertices[2] = v3(pos.x - c_player_size.x * 0.5f, pos.y - c_player_size.y * 0.5f, pos.z + c_player_size.z * 0.5f);
	// a.vertices[3] = v3(pos.x + c_player_size.x * 0.5f, pos.y - c_player_size.y * 0.5f, pos.z + c_player_size.z * 0.5f);
	// a.vertices[4] = v3(pos.x - c_player_size.x * 0.5f, pos.y + c_player_size.y * 0.5f, pos.z - c_player_size.z * 0.5f);
	// a.vertices[5] = v3(pos.x + c_player_size.x * 0.5f, pos.y + c_player_size.y * 0.5f, pos.z - c_player_size.z * 0.5f);
	// a.vertices[6] = v3(pos.x - c_player_size.x * 0.5f, pos.y - c_player_size.y * 0.5f, pos.z - c_player_size.z * 0.5f);
	// a.vertices[7] = v3(pos.x + c_player_size.x * 0.5f, pos.y - c_player_size.y * 0.5f, pos.z - c_player_size.z * 0.5f);
	a.vertex_count = 8;

	int player_x = floorfi(pos.x / c_tile_size);
	int player_y = floorfi(pos.y / c_tile_size);
	for(int y = -1; y <= 1; y += 1) {
		int yy = y + player_y;
		if(yy < 0 || yy >= c_tiles_y) { continue; }
		for(int x = -1; x < 1; x += 1) {
			int xx = x + player_x;
			if(xx < 0 || xx >= c_tiles_x) { continue; }
			int index = (xx + yy * c_tiles_x) * 6;

			for(int i = 0; i < 2; i += 1) {
				s_shape b = zero;
				b.vertices[0] = g_terrain_vertex_arr[index + 3 * i + 0].pos;
				b.vertices[1] = g_terrain_vertex_arr[index + 3 * i + 1].pos;
				b.vertices[2] = g_terrain_vertex_arr[index + 3 * i + 2].pos;
				b.vertex_count = 3;
				if(SATCollision3D(a, b)) {
					s_triangle triangle = make_triangle(b.vertices[0], b.vertices[1], b.vertices[2]);
					result.triangle_arr.add(triangle);
				}
			}
		}
	}
	return result;
}

template <typename t, int n>
t& s_list<t, n>::operator[](int index)
{
	assert(index >= 0);
	assert(index < this->count);
	return this->data[index];
}

template <typename t, int n>
t s_list<t, n>::pop_last()
{
	assert(this->count > 0);
	this->count -= 1;
	t result = this->data[this->count];
	return result;
}

template <typename t, int n>
t* s_list<t, n>::add(t new_element)
{
	assert(count < n);
	t* result = &this->data[this->count];
	this->data[this->count] = new_element;
	this->count += 1;
	return result;
}

template <typename t, int n>
void s_list<t, n>::remove_and_swap(int index)
{
	assert(index < count);
	this->count -= 1;
	this->data[index] = this->data[this->count];
}

func u8* read_file(char* path)
{
	FILE* file = fopen(path, "rb");
	b8 read_file = false;
	u8* result = null;
	if(file) {
		read_file = true;
	}
	if(read_file) {
		fseek(file, 0, SEEK_END);
		int size = ftell(file);
		fseek(file, 0, SEEK_SET);
		result = arena_alloc(&g_frame_arena, size + 1);
		fread(result, 1, size, file);
		fclose(file);
		result[size] = '\0';
	}
	return result;
}

func s_ply_mesh parse_ply_mesh(char* path)
{
	char* data = (char*)read_file(path);
	char* cursor = data;
	s_ply_mesh result = zero;
	{
		char* temp = strstr(cursor, "element vertex ") + 15;
		result.vertex_count = atoi(temp);
		assert(result.vertex_count <= 1024);
	}
	{
		char* temp = strstr(cursor, "element face ") + 13;
		result.face_count = atoi(temp);
		assert(result.face_count <= 1024);
	}
	cursor = strstr(data, "end_header") + 11;

	{
		s_ply_vertex* temp = (s_ply_vertex*)cursor;
		for(int i = 0; i < result.vertex_count; i += 1) {
			result.vertex_arr[i] = *temp;
			temp += 1;
		}
		cursor = (char*)temp;
	}
	{
		s_ply_face* temp = (s_ply_face*)cursor;
		for(int i = 0; i < result.face_count; i += 1) {
			result.face_arr[i] = *temp;
			assert(result.face_arr[i].index_count == 3);
			temp += 1;
		}
	}
	return result;
}

func s_box make_box(s_v3 pos, s_v3 size)
{
	s_box result = zero;
	result.vertex_arr[0].x = -size.x * 0.5f + pos.x;
	result.vertex_arr[0].y = size.y * 0.5f + pos.y;
	result.vertex_arr[0].z = size.z * 0.5f + pos.z;

	result.vertex_arr[1].x = size.x * 0.5f + pos.x;
	result.vertex_arr[1].y = size.y * 0.5f + pos.y;
	result.vertex_arr[1].z = size.z * 0.5f + pos.z;

	result.vertex_arr[2].x = size.x * 0.5f + pos.x;
	result.vertex_arr[2].y = -size.y * 0.5f + pos.y;
	result.vertex_arr[2].z = size.z * 0.5f + pos.z;

	result.vertex_arr[3].x = -size.x * 0.5f + pos.x;
	result.vertex_arr[3].y = -size.y * 0.5f + pos.y;
	result.vertex_arr[3].z = size.z * 0.5f + pos.z;

	result.vertex_arr[4].x = -size.x * 0.5f + pos.x;
	result.vertex_arr[4].y = size.y * 0.5f + pos.y;
	result.vertex_arr[4].z = -size.z * 0.5f + pos.z;

	result.vertex_arr[5].x = size.x * 0.5f + pos.x;
	result.vertex_arr[5].y = size.y * 0.5f + pos.y;
	result.vertex_arr[5].z = -size.z * 0.5f + pos.z;

	result.vertex_arr[6].x = size.x * 0.5f + pos.x;
	result.vertex_arr[6].y = -size.y * 0.5f + pos.y;
	result.vertex_arr[6].z = -size.z * 0.5f + pos.z;

	result.vertex_arr[7].x = -size.x * 0.5f + pos.x;
	result.vertex_arr[7].y = -size.y * 0.5f + pos.y;
	result.vertex_arr[7].z = -size.z * 0.5f + pos.z;

	return result;
}

func void draw_mesh_world(e_mesh mesh_id, s_m4 model, s_v4 color, int flags)
{
	s_mesh_instance_data data = zero;
	data.model = model;
	data.color = color;
	data.flags = flags;
	g_mesh_instance_data_arr[mesh_id].add(data);
}

func void draw_mesh_screen(e_mesh mesh_id, s_m4 model, s_v4 color, int flags)
{
	s_mesh_instance_data data = zero;
	data.model = model;
	data.color = color;
	data.flags = flags | e_render_flag_ignore_fog | e_render_flag_ignore_light | e_render_flag_dont_cast_shadows | e_render_flag_screen;
	g_mesh_instance_data_arr[mesh_id].add(data);
}

func void setup_common_mesh_stuff(s_mesh* mesh)
{
	{
		SDL_GPUBufferCreateInfo buffer_create_info = zero;
		buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		buffer_create_info.size = sizeof(s_mesh_instance_data) * c_max_mesh_instances;
		mesh->instance_buffer = SDL_CreateGPUBuffer(g_device, &buffer_create_info);
	}

	{
		SDL_GPUTransferBufferCreateInfo transfer_create_info = zero;
		transfer_create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transfer_create_info.size = sizeof(s_mesh_instance_data) * c_max_mesh_instances;
		mesh->instance_transfer_buffer = SDL_CreateGPUTransferBuffer(g_device, &transfer_create_info);
	}
}

func void setup_mesh_vertex_buffers(s_mesh* mesh, int buffer_size)
{
	{
		SDL_GPUBufferCreateInfo buffer_create_info = zero;
		buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		buffer_create_info.size = buffer_size;
		mesh->vertex_buffer = SDL_CreateGPUBuffer(g_device, &buffer_create_info);
	}

	{
		SDL_GPUTransferBufferCreateInfo transfer_create_info = zero;
		transfer_create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transfer_create_info.size = buffer_size;
		mesh->vertex_transfer_buffer = SDL_CreateGPUTransferBuffer(g_device, &transfer_create_info);
	}
}

func s_linear_arena make_arena_from_malloc(int requested_size)
{
	assert(requested_size > 0);
	int size = (requested_size + 7) & ~7;
	s_linear_arena result = zero;
	result.capacity = size;
	result.memory = (u8*)malloc(size);
	return result;
}

func u8* arena_alloc(s_linear_arena* arena, int requested_size)
{
	assert(requested_size > 0);
	int size = (requested_size + 7) & ~7;
	assert(arena->used + size <= arena->capacity);
	u8* result = arena->memory;
	arena->used += size;
	return result;
}

func void arena_reset(s_linear_arena* arena)
{
	arena->used = 0;
}

func void make_game_mesh_from_ply_mesh(s_mesh* mesh, s_ply_mesh* ply_mesh)
{
	setup_common_mesh_stuff(mesh);
	setup_mesh_vertex_buffers(mesh, sizeof(s_vertex) * ply_mesh->face_count * 3);

	s_vertex vertex_arr[4096] = zero;
	for(int i = 0; i < ply_mesh->face_count; i += 1) {
		s_ply_vertex arr[3] = {
			ply_mesh->vertex_arr[ply_mesh->face_arr[i].index_arr[0]],
			ply_mesh->vertex_arr[ply_mesh->face_arr[i].index_arr[1]],
			ply_mesh->vertex_arr[ply_mesh->face_arr[i].index_arr[2]],
		};
		for(int j = 0; j < 3; j += 1) {
			assert(mesh->vertex_count < 4096);
			vertex_arr[mesh->vertex_count] = {
				arr[j].pos, arr[j].normal, make_color(1), arr[j].uv
			};
			mesh->vertex_count += 1;
		}
	}
	upload_to_gpu_buffer(vertex_arr, sizeof(s_vertex) * mesh->vertex_count, mesh->vertex_buffer, mesh->vertex_transfer_buffer);
}

func s_triangle make_triangle(s_v3 v0, s_v3 v1, s_v3 v2)
{
	s_triangle result = zero;
	result.vertex_arr[0] = v0;
	result.vertex_arr[1] = v1;
	result.vertex_arr[2] = v2;
	return result;
}

func void draw_screen(s_v2 pos, s_v2 size, s_v4 color)
{
	s_m4 model = m4_translate(v3(pos, 0));
	model = m4_multiply(model, m4_scale(v3(size, 1)));
	int flags = e_render_flag_ignore_fog | e_render_flag_ignore_light | e_render_flag_dont_cast_shadows | e_render_flag_screen | e_render_flag_textured;
	draw_mesh_screen(e_mesh_quad, model, color, flags);
}

func char* format_text(char* str, ...)
{
	static int index = 0;
	static char buffer[4][1024] = zero;
	va_list args;
	va_start(args, str);
	int written = vsnprintf(buffer[index], 1024, str, args);
	assert(written > 0);
	va_end(args);
	char* result = buffer[index];
	index = (index + 1) % 4;
	return result;
}

func SDL_EnumerationResult enumerate_directory_callback(void *userdata, const char *dirname, const char *fname)
{
	(void)dirname;
	(void)userdata;
	if(strstr(fname, ".shader")) {
		char* str = format_text("assets/%s", fname);
		SDL_PathInfo info = zero;
		SDL_GetPathInfo(str, &info);
		if(info.modify_time > g_last_shader_modify_time) {
			g_last_shader_modify_time = max(g_last_shader_modify_time, info.modify_time);
			g_reload_shaders = true;
		}
	}
	return SDL_ENUM_CONTINUE;
}

func b8 is_shader_valid(s_shader_program program)
{
	b8 result = program.shader_arr[0] && program.shader_arr[1];
	return result;
}

func void draw_quad_screen(s_v2 pos, float z, s_v2 size, s_v4 color)
{
	s_m4 model = m4_translate(v3(pos, -99 + z));
	model = m4_multiply(model, m4_scale(v3(size, 1)));
	draw_mesh_screen(e_mesh_quad, model, color, 0);
}

func void draw_quad_screen_topleft(s_v2 pos, float z, s_v2 size, s_v4 color)
{
	pos += size * 0.5f;
	s_m4 model = m4_translate(v3(pos, -99 + z));
	model = m4_multiply(model, m4_scale(v3(size, 1)));
	draw_mesh_screen(e_mesh_quad, model, color, 0);
}

func int ui_push_widget(s_v2 pos, s_v2 size, int flags)
{
	int my_index = g_ui.widget_arr.count;
	s_ui_widget new_widget = zero;
	new_widget.depth = g_ui.depth;
	new_widget.pos = pos;
	new_widget.parent = -1;
	new_widget.flags = flags;
	for(int i = 0; i < g_ui.widget_arr.count; i += 1) {
		s_ui_widget widget = g_ui.widget_arr[i];
		if(widget.depth == g_ui.depth - 1) {
			new_widget.parent = i;
			break;
		}
	}
	if(g_ui.data_arr[my_index].size.x <= 0) {
		g_ui.data_arr[my_index].size = size;
	}
	g_ui.depth += 1;
	g_ui.widget_arr.add(new_widget);
	g_ui.stack_arr.add(g_ui.widget_arr.count - 1);

	int result = 0;
	if(g_ui.hot_index == my_index) {
		result = g_ui.hot_state;
		if(result & e_ui_state_clicked) {
			play_sound(g_sound_pop, false);
		}
	}
	return result;
}

func void ui_pop_widget()
{
	assert(g_ui.widget_arr.count > 0);
	assert(g_ui.stack_arr.count > 0);

	int popped_index = g_ui.stack_arr.pop_last();
	s_ui_widget* popped = &g_ui.widget_arr[popped_index];

	b8 handle = g_ui.depth == 1;

	if(popped->flags & e_ui_flag_resizable) {
		if(ui_widget(e_ui_flag_clickable | e_ui_flag_dynamic, {.bottom_right = true, .size = v2(24)}) == e_ui_state_pressed) {
			s_v2* size = &g_ui.data_arr[popped_index].size;
			size->x += g_mouse_delta.x;
			size->y += g_mouse_delta.y;
		}
	}

	if(handle) {

		g_ui.hot_state &= ~e_ui_state_clicked;
		if(!g_left_down) {
			g_ui.hot_state &= ~e_ui_state_pressed;
		}

		srand(1); // nocheckin

		popped->out_size = g_ui.data_arr[popped_index].size;
		popped->out_pos = popped->pos;
		ui_process_size(popped_index, g_ui.data_arr[popped_index].size);
		ui_process_pos(popped_index, popped->pos);

		int hovered_depth = -1;
		int hovered_i = -1;
		for(int widget_i = 0; widget_i < g_ui.widget_arr.count; widget_i += 1) {
			s_ui_widget widget = g_ui.widget_arr[widget_i];
			b8 hovered = (widget.flags & e_ui_flag_clickable) && mouse_vs_rect_topleft(widget.out_pos, widget.out_size);
			if(g_ui.hot_index >= 0 && g_ui.hot_state & e_ui_state_pressed && g_ui.hot_index != widget_i) {
				hovered = false;
			}
			if(hovered && widget.depth > hovered_depth) {
				hovered_depth = widget.depth;
				hovered_i = widget_i;
			}
		}

		for(int widget_i = 0; widget_i < g_ui.widget_arr.count; widget_i += 1) {
			s_ui_widget widget = g_ui.widget_arr[widget_i];
			float f = rand() / (float)RAND_MAX;
			s_v4 color = make_color(f);
			s_v2 size = widget.out_size;
			if(hovered_i == widget_i) {
				// color = multiply_rgb(color, 1.33f);
				color = make_color(0, 1, 0);
				// size.x += 200;
				if(g_left_down_this_frame) {
					g_ui.hot_state |= e_ui_state_pressed | e_ui_state_clicked;
					g_ui.hot_index = widget_i;
				}
			}
			draw_quad_screen_topleft(widget.out_pos, (float)widget.depth, size, color);
		}

		if(g_ui.hot_state == 0) {
			g_ui.hot_index = -1;
		}

		g_ui.widget_arr.count = 0;
	}

	g_ui.depth -= 1;
}

func int ui_widget(int flags, s_ui_optional optional)
{
	int my_index = g_ui.widget_arr.count;
	s_ui_widget new_widget = zero;
	new_widget.depth = g_ui.depth;
	new_widget.parent = -1;
	new_widget.flags = flags;
	new_widget.optional = optional;
	for(int i = 0; i < g_ui.widget_arr.count; i += 1) {
		s_ui_widget widget = g_ui.widget_arr[i];
		if(widget.depth == g_ui.depth - 1) {
			new_widget.parent = i;
			break;
		}
	}
	g_ui.widget_arr.add(new_widget);

	// if(g_ui.data_arr[my_index].size.x <= 0) {
	// 	g_ui.data_arr[my_index].size = size;
	// }

	int result = 0;
	if(g_ui.hot_index == my_index) {
		result = g_ui.hot_state;
		if(result & e_ui_state_clicked) {
			play_sound(g_sound_pop, false);
		}
	}
	return result;
}

func b8 rect_vs_rect_topleft(s_v2 pos0, s_v2 size0, s_v2 pos1, s_v2 size1)
{
	b8 result = pos0.x + size0.x > pos1.x && pos0.x < pos1.x + size1.x &&
		pos0.y + size0.y > pos1.y && pos0.y < pos1.y + size1.y;
	return result;
}

func b8 mouse_vs_rect_topleft(s_v2 pos, s_v2 size)
{
	b8 result = rect_vs_rect_topleft(g_mouse, v2(1, 1), pos, size);
	return result;
}

func s_v4 multiply_rgb(s_v4 a, float b)
{
	a.x = at_most(1.0f, a.x * b);
	a.y = at_most(1.0f, a.y * b);
	a.z = at_most(1.0f, a.z * b);
	return a;
}

func void ui_process_size(int curr_i, s_v2 size)
{
	int direct_children = 0;
	s_ui_widget* curr_widget = &g_ui.widget_arr[curr_i];
	if(curr_widget->optional.size.x > 0) {
		curr_widget->out_size = curr_widget->optional.size;
	}
	else {
		curr_widget->out_size = size;
	}
	for(int widget_i = curr_i + 1; widget_i < g_ui.widget_arr.count; widget_i += 1) {
		s_ui_widget widget = g_ui.widget_arr[widget_i];
		if(widget.depth <= curr_widget->depth) { break; }
		if(widget.depth != curr_widget->depth + 1) { continue; }
		direct_children += 1;
	}

	int num_auto_sized_children = 0;
	s_v2 space = v2(
		curr_widget->out_size.x - c_margin * 2,
		curr_widget->out_size.y - c_margin * 2
	);

	for(int widget_i = curr_i + 1; widget_i < g_ui.widget_arr.count; widget_i += 1) {
		s_ui_widget widget = g_ui.widget_arr[widget_i];
		if(widget.depth <= curr_widget->depth) { break; }
		if(widget.depth != curr_widget->depth + 1) { continue; }
		if(widget.optional.size.x > 0) {
			space.y -= widget.optional.size.y;
		}
		else {
			num_auto_sized_children += 1;
		}
	}

	s_v2 children_size = v2(
		space.x,
		(space.y - c_padding * (num_auto_sized_children - 1)) / num_auto_sized_children
	);

	for(int widget_i = curr_i + 1; widget_i < g_ui.widget_arr.count; widget_i += 1) {
		s_ui_widget widget = g_ui.widget_arr[widget_i];
		if(widget.depth <= curr_widget->depth) { break; }
		if(widget.depth != curr_widget->depth + 1) { continue; }
		ui_process_size(widget_i, children_size);
	}
}

func void ui_process_pos(int curr_i, s_v2 pos)
{
	int direct_children = 0;
	s_ui_widget* curr_widget = &g_ui.widget_arr[curr_i];
	if(curr_widget->optional.bottom_right) {
		assert(curr_widget->parent >= 0);
		s_ui_widget parent = g_ui.widget_arr[curr_widget->parent];
		curr_widget->out_pos = parent.out_pos + parent.out_size - curr_widget->out_size;
	}
	else {
		curr_widget->out_pos = pos;
	}

	s_v2 curr_pos = pos + v2(c_margin);

	for(int widget_i = curr_i + 1; widget_i < g_ui.widget_arr.count; widget_i += 1) {
		s_ui_widget widget = g_ui.widget_arr[widget_i];
		if(widget.depth <= curr_widget->depth) { break; }
		if(widget.depth != curr_widget->depth + 1) { continue; }
		direct_children += 1;
	}

	for(int widget_i = curr_i + 1; widget_i < g_ui.widget_arr.count; widget_i += 1) {
		s_ui_widget widget = g_ui.widget_arr[widget_i];
		if(widget.depth <= curr_widget->depth) { break; }
		if(widget.depth != curr_widget->depth + 1) { continue; }
		ui_process_pos(widget_i, curr_pos);
		curr_pos.y += widget.out_size.y + c_padding;
	}

}

func void on_failed_assert(char* condition, char* file, int line)
{
	printf("Failed assert at %s(%i)\n", file, line);
	printf("\t%s\n", condition);
	printf("-----------------------------\n");

	#ifdef _WIN32
	__debugbreak();
	#else
	*(int*)0 = 0;
	#endif
}

func void audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	(void)total_amount;
	(void)userdata;

	int num_samples = additional_amount / sizeof(s16);
	s16* to_submit = (s16*)calloc(1, num_samples * sizeof(s16));
	for(int sample_i = 0; sample_i < num_samples; sample_i += 1) {
		int total = 0;
		for(int sound_i = 0; sound_i < g_sound_to_play_arr.count; sound_i += 1) {
			s_playing_sound* sound = &g_sound_to_play_arr[sound_i];
			total += ((s16*)sound->sound.data)[sound->cursor];
			sound->cursor += 1;
			if(sound->cursor >= sound->sound.sample_count) {
				if(sound->loop) {
					sound->cursor = 0;
				}
				else {
					g_sound_to_play_arr.remove_and_swap(sound_i);
					sound_i -= 1;
				}
			}
		}
		total = clamp(total, -(s32)(c_max_s16), (s32)c_max_s16);
		to_submit[sample_i] = (s16)total;
	}
	SDL_PutAudioStreamData(stream, to_submit, num_samples * sizeof(s16));
	free(to_submit);
}

func void play_sound(s_sound sound, b8 loop)
{
	s_playing_sound playing_sound = zero;
	playing_sound.sound = sound;
	playing_sound.loop = loop;
	g_sound_to_play_arr.add(playing_sound);
}

func s_sound load_sound(char* path)
{
	s_sound result = zero;
	SDL_AudioSpec wav_spec = zero;
	u32 temp_len = 0;
	bool success = SDL_LoadWAV(path, &wav_spec, &result.data, &temp_len);

	assert(success);
	assert(wav_spec.freq == 44100);
	assert(wav_spec.channels == 2);

	result.sample_count = (int)temp_len / sizeof(s16);
	return result;
}

func s_v3 get_cam_front(s_camera cam)
{
	s_v3 result = v3(
		-sinf(cam.yaw) * cosf(cam.pitch),
		cosf(cam.yaw) * cosf(cam.pitch),
		sinf(cam.pitch)
	);
	result = v3_normalized(result);
	return result;
}