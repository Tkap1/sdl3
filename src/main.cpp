
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
#include "intrin.h"
#include "main.h"

global SDL_GPUTexture* scene_depth_texture;
global SDL_GPUTexture* shadow_texture;
global SDL_GPUSampler* shadow_texture_sampler;
global b8 g_do_wireframe = false;
global constexpr int c_tiles_x = 512;
global constexpr int c_tiles_y = 512;
global constexpr int c_vertex_count = c_tiles_x * c_tiles_y * 6;
global constexpr float c_tile_size = 1.0f;
global float g_time = 0;
global constexpr float c_pi = 3.1415926535f;
global constexpr int c_window_width = 1920;
global constexpr int c_window_height = 1080;
global constexpr s_v2 c_window_size = {c_window_width, c_window_height};
global constexpr s_v2 c_half_window_size = {c_window_width * 0.5f, c_window_height * 0.5f};
global s_player g_player;
global float cam_yaw;
global float cam_pitch;
global SDL_GPUDevice* g_device;
global SDL_Window* g_window;
global SDL_GPUTextureFormat g_depth_texture_format = SDL_GPU_TEXTUREFORMAT_INVALID;
global s_speed_buff g_speed_buff;
global float g_last_speed_time = 0;
global constexpr s_v3 c_player_size = v3(0.1f, 0.1f, 6.0f);
global constexpr int c_max_mesh_instances = 1024;
global s_list<s_sphere, c_max_mesh_instances> g_sphere_arr;
global s_mesh g_mesh_arr[e_mesh_count];
global s_mesh_instance_data g_mesh_instance_data[e_mesh_count][c_max_mesh_instances];
global s_list<s_mesh_instance_data, c_max_mesh_instances> g_mesh_instance_data_arr[e_mesh_count];
global s_linear_arena g_frame_arena;
global s_vertex g_terrain_vertex_arr[c_vertex_count];
global shaderc_compiler_t g_shader_compiler;
global SDL_Time g_last_shader_modify_time;
global b8 g_reload_shaders = true;

int main()
{
	g_frame_arena = make_arena_from_malloc(1024 * 1024 * 1024);
	SDL_Init(SDL_INIT_VIDEO);

	g_player.pos.x = 10;
	g_player.pos.y = 10;
	g_player.pos.z = 20;

	s_ply_mesh ply_sphere = parse_ply_mesh("assets/sphere.ply");

	g_device = SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV,
		true,
		null
	);

	g_window = SDL_CreateWindow("3D", c_window_width, c_window_height, 0);
	SDL_ClaimWindowForGPUDevice(g_device, g_window);

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

	b8 left_down = false;
	e_view_state view_state = e_view_state_default;

	s_shader_program mesh_shader = zero;
	s_shader_program depth_only_shader = zero;
	SDL_GPUGraphicsPipeline* mesh_fill_pipeline = null;
	SDL_GPUGraphicsPipeline* mesh_line_pipeline = null;
	SDL_GPUGraphicsPipeline* mesh_depth_only_pipeline = null;

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

	s_v3 cam_pos = g_player.pos;

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
			vertex_arr[0].color = make_color(0, 1, 0);
			vertex_arr[0].uv = v2(0, 0);
			vertex_arr[1].pos = v3(c_size, -c_size, 0.0f);
			vertex_arr[1].normal = v3(0, -1, 0);
			vertex_arr[1].color = make_color(1);
			vertex_arr[1].uv = v2(1, 0);
			vertex_arr[2].pos = v3(c_size, c_size, 0.0f);
			vertex_arr[2].normal = v3(0, -1, 0);
			vertex_arr[2].color = make_color(1, 0, 0);
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


	b8 generate_terrain = true;

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		loop start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	b8 running = true;
	float ticks_before = (float)SDL_GetTicks();
	float delta = 0;
	while(running) {
		float ticks = (float)SDL_GetTicks();
		delta = (ticks - ticks_before) / 1000;
		g_time += delta;
		ticks_before = ticks;
		SDL_Event event = zero;

		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			else if(event.type == SDL_EVENT_KEY_DOWN) {
				if(event.key.key == SDLK_LEFT) {
					g_do_wireframe = !g_do_wireframe;
				}
				else if(event.key.key == SDLK_F) {
					generate_terrain = true;
				}
				else if(event.key.key == SDLK_SPACE) {
					g_player.want_to_jump_timestamp = g_time;
				}
				else if(event.key.key == SDLK_G) {
					if(view_state == e_view_state_shadow_map) {
						view_state = e_view_state_default;
					}
					else {
						view_state = e_view_state_shadow_map;
					}
				}
				else if(event.key.key == SDLK_H) {
					if(view_state == e_view_state_depth) {
						view_state = e_view_state_default;
					}
					else {
						view_state = e_view_state_depth;
					}
				}
			}
			else if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				if(event.button.button == SDL_BUTTON_LEFT) {
					left_down = true;
					g_player.want_to_shoot_timestamp = g_time;
				}
			}
			else if(event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
				if(event.button.button == SDL_BUTTON_LEFT) {
					left_down = false;
				}
			}
			else if(event.type == SDL_EVENT_MOUSE_MOTION) {
				constexpr float sens = 0.002f;
				cam_yaw -= event.motion.xrel * sens;
				cam_pitch -= event.motion.yrel * sens;
				cam_pitch = clamp(cam_pitch, -c_pi * 0.4f, c_pi * 0.4f);
			}
		}

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		hot shader reloading start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		{
			SDL_EnumerateDirectory("./assets", enumerate_directory_callback, null);
			if(g_reload_shaders) {
				s_shader_program new_shader_arr[2] = zero;
				g_reload_shaders = false;
				g_shader_compiler = shaderc_compiler_initialize();

				{
					s_shader_data data = zero;
					data.sampler_count[1] = 1;
					data.uniform_buffer_count[0] = 1;
					data.uniform_buffer_count[1] = 1;
					new_shader_arr[0] = load_shader("assets/mesh.shader", data);
				}
				{
					s_shader_data data = zero;
					data.uniform_buffer_count[0] = 1;
					new_shader_arr[1] = load_shader("assets/depth_only.shader", data);
				}

				if(is_shader_valid(new_shader_arr[0]) && is_shader_valid(new_shader_arr[1])) {
					mesh_shader = new_shader_arr[0];
					depth_only_shader = new_shader_arr[1];

					if(mesh_fill_pipeline) {
						SDL_ReleaseGPUGraphicsPipeline(g_device, mesh_fill_pipeline);
					}
					if(mesh_line_pipeline) {
						SDL_ReleaseGPUGraphicsPipeline(g_device, mesh_line_pipeline);
					}
					if(mesh_depth_only_pipeline) {
						SDL_ReleaseGPUGraphicsPipeline(g_device, mesh_depth_only_pipeline);
					}

					{
						s_list<SDL_GPUVertexElementFormat, 16> vertex_attributes;
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
						s_list<SDL_GPUVertexElementFormat, 16> instance_attributes;
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_INT);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						mesh_fill_pipeline = create_pipeline(mesh_shader, SDL_GPU_FILLMODE_FILL, 1, vertex_attributes, instance_attributes, true);
						mesh_line_pipeline = create_pipeline(mesh_shader, SDL_GPU_FILLMODE_LINE, 1, vertex_attributes, instance_attributes, true);
					}

					{
						s_list<SDL_GPUVertexElementFormat, 16> vertex_attributes;
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						vertex_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
						s_list<SDL_GPUVertexElementFormat, 16> instance_attributes;
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_INT);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						instance_attributes.add(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
						mesh_depth_only_pipeline = create_pipeline(depth_only_shader, SDL_GPU_FILLMODE_FILL, 0, vertex_attributes, instance_attributes, true);
					}
				}

				// Clean up shader resources
				for(int i = 0; i < 2; i += 1) {
					for(int j = 0; j < 2; j += 1) {
						if(new_shader_arr[i].shader_arr[j]) {
							SDL_ReleaseGPUShader(g_device, new_shader_arr[i].shader_arr[j]);
						}
					}
				}

				shaderc_compiler_release(g_shader_compiler);
				g_shader_compiler = null;

			}
		}
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		hot shader reloading end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


		s_v3 cam_front = v3(
			-sinf(cam_yaw) * cosf(cam_pitch),
			cosf(cam_yaw) * cosf(cam_pitch),
			sinf(cam_pitch)
		);
		cam_front = v3_normalized(cam_front);

		s_v3 cam_up = v3(
			sinf(cam_yaw) * sinf(cam_pitch) * cosf(cam_pitch),
			-cosf(cam_yaw) * sinf(cam_pitch) * cosf(cam_pitch),
			cosf(cam_pitch) * cosf(cam_pitch)
		);
		cam_up = v3_normalized(cam_up);

		s_v3 player_wanted_dir = zero;
		float player_wanted_speed = 0.1f;
		{
			b8* key_arr = (b8*)SDL_GetKeyboardState(null);
			s_v3 cam_side = v3_cross(cam_front, v3(0, 0, 1));
			cam_side = v3_normalized(cam_side);
			s_v3 temp_front = cam_front;
			temp_front.z = 0;
			temp_front = v3_normalized(temp_front);
			s_v3 dir = zero;
			if(key_arr[SDL_SCANCODE_D]) {
				dir += cam_side;
			}
			if(key_arr[SDL_SCANCODE_A]) {
				dir += cam_side * -1;
			}
			if(key_arr[SDL_SCANCODE_W]) {
				dir += temp_front;
			}
			if(key_arr[SDL_SCANCODE_S]) {
				dir += temp_front * -1;
			}
			if(key_arr[SDL_SCANCODE_LSHIFT]) {
				player_wanted_speed *= 10;
			}
			player_wanted_dir = v3_normalized(dir);
		}

		if(generate_terrain) {
			generate_terrain = false;

			{
				constexpr int c_biome_count = 6;
				s_v3 biome_color_arr[c_biome_count] = {
					v3(0, 0, 1),
					v3(1, 1, 0),
					v3(0, 1, 0),
					v3(0, 1, 1),
					v3(1, 0, 0),
					v3(1, 0, 1),
				};
				float scale_arr[c_biome_count] = {2, 8, 1, 1, 40, 60};
				fnl_state noise_arr[c_biome_count + 1] = zero;
				for(int i = 0; i < c_biome_count + 1; i += 1) {
					noise_arr[i] = fnlCreateState();
					noise_arr[i].seed = (u32)__rdtsc();
					// noise_arr[i].seed = 1;
					noise_arr[i].noise_type = FNL_NOISE_OPENSIMPLEX2;
				}
				noise_arr[0].frequency = 0.1f;
				noise_arr[1].frequency = 0.05f;
				noise_arr[2].frequency = 0.01f;
				noise_arr[3].frequency = 0.01f;
				noise_arr[4].frequency = 0.05f;
				noise_arr[5].frequency = 0.07f;

				noise_arr[c_biome_count].frequency = 0.005f;

				float weight_arr2[c_biome_count] = {100, 100, 1000, 1000, 100, 1};
				float weight_arr[c_biome_count][2] = zero;
				{
					float total_weight = 0;
					for(int i = 0; i < c_biome_count; i += 1) {
						total_weight += weight_arr2[i];
					}
					float curr = -1;
					for(int i = 0; i < c_biome_count; i += 1) {
						weight_arr[i][0] = curr;
						curr += weight_arr2[i] / total_weight * 2;
						weight_arr[i][1] = curr;
					}
				}

				int count = 0;

				for(int y = 0; y < c_tiles_y; y += 1) {
					float yy = (float)y;
					for(int x = 0; x < c_tiles_x; x += 1) {
						float xx = (float)x;
						float x_arr[6] = zero;
						float y_arr[6] = zero;
						float z_arr[6] = zero;
						float height_arr[6] = zero;
						float height_scale_arr[6] = zero;
						s_v4 color_arr[6] = zero;
						x_arr[0] = xx;
						x_arr[1] = xx + 1;
						x_arr[2] = xx + 1;
						x_arr[3] = xx;
						x_arr[4] = xx + 1;
						x_arr[5] = xx;
						y_arr[0] = yy + 1;
						y_arr[1] = yy + 1;
						y_arr[2] = yy;
						y_arr[3] = yy + 1;
						y_arr[4] = yy;
						y_arr[5] = yy;

						for(int i = 0; i < 6; i += 1) {
							x_arr[i] *= c_tile_size;
							y_arr[i] *= c_tile_size;
						}

						for(int i = 0; i < 6; i += 1) {
							float m = fnlGetNoise2D(&noise_arr[c_biome_count], x_arr[i], y_arr[i]);
							float total_p = 0.0f;
							float p_arr[c_biome_count] = zero;
							for(int j = 0; j < c_biome_count; j += 1) {
								float p = (weight_arr[j][0] + weight_arr[j][1]) / 2;
								float d = 1.0f - fabsf(p - m);
								d = clamp(d, 0.0f, 1.0f);
								d = powf(d, 4);
								p_arr[j] = d;
								total_p += d;
							}
							float height = 0;
							float height_scale = 0;
							s_v4 color = v4(0, 0, 0, 1);
							for(int j = 0; j < c_biome_count; j += 1) {
								height += p_arr[j] / total_p * fnlGetNoise2D(&noise_arr[j], x_arr[i], y_arr[i]);
								height_scale += p_arr[j] / total_p * scale_arr[j];
								color.xyz += biome_color_arr[j] * (p_arr[j] / total_p);
							}
							height_arr[i] = height;
							height_scale_arr[i] = height_scale;
							color_arr[i] = color;
							z_arr[i] = height * height_scale;

							// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		hardcoded terrain start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
							{
								// float h = smoothstep2(30, 60, x_arr[i]) * 40;
								// h += smoothstep2(100, 130, x_arr[i]) * 80;
								// h += smoothstep2(200, 230, x_arr[i]) * 160;
								// h = roundf(sinf(x_arr[i] * 0.1f)) * 10;

								// z_arr[i] = h;
								// height_arr[i] = h;
								// height_scale_arr[i] = 1;
								// color_arr[i] = make_color(
								// 	(x + y) % 2 == 0 ? 0.5f : 0.3f,
								// 	1
								// );
							}
							// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		hardcoded terrain end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
						}

						s_v3 normal_arr[2] = zero;
						normal_arr[0] = get_triangle_normal(make_triangle(
							v3(x_arr[0], y_arr[0], z_arr[0]),
							v3(x_arr[1], y_arr[1], z_arr[1]),
							v3(x_arr[2], y_arr[2], z_arr[2])
						));
						normal_arr[1] = get_triangle_normal(make_triangle(
							v3(x_arr[3], y_arr[3], z_arr[3]),
							v3(x_arr[4], y_arr[4], z_arr[4]),
							v3(x_arr[5], y_arr[5], z_arr[5])
						));

						for(int i = 0; i < 6; i += 1) {
							g_terrain_vertex_arr[count] = {v3(x_arr[i], y_arr[i], height_arr[i] * height_scale_arr[i]), normal_arr[i / 3], color_arr[i], v2(0, 0)};
							count += 1;
						}
					}
				}

				// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		smooth shading start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
				{
					s_v3* sum_normal_arr = (s_v3*)arena_alloc(&g_frame_arena, sizeof(s_v3) * c_vertex_count);
					for(int i = 0; i < c_vertex_count; i += 1) {
						int x = floorfi(g_terrain_vertex_arr[i].pos.x / c_tile_size);
						int y = floorfi(g_terrain_vertex_arr[i].pos.y / c_tile_size);
						sum_normal_arr[x + y * c_tiles_x] += g_terrain_vertex_arr[i].normal;
					}
					for(int i = 0; i < c_vertex_count; i += 1) {
						int x = floorfi(g_terrain_vertex_arr[i].pos.x / c_tile_size);
						int y = floorfi(g_terrain_vertex_arr[i].pos.y / c_tile_size);
						g_terrain_vertex_arr[i].normal = v3_normalized(sum_normal_arr[x + y * c_tiles_x]);
					}
				}
				// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		smooth shading end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
			}

			s_mesh* mesh = &g_mesh_arr[e_mesh_terrain];
			upload_to_gpu_buffer(g_terrain_vertex_arr, sizeof(s_vertex) * c_vertex_count, mesh->vertex_buffer, mesh->vertex_transfer_buffer);
		}

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		update start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		{

			{
				float passed = g_time - g_player.want_to_shoot_timestamp;
				if(passed <= 0.0f) {
					s_sphere sphere = zero;
					sphere.pos = g_player.pos + cam_front * 3;
					sphere.vel = cam_front * 0.75f;
					sphere.spawn_timestamp = g_time;
					g_sphere_arr.add(sphere);
				}
			}

			{
				float passed = g_time - g_last_speed_time;
				if(passed > 0.25f && !g_speed_buff.active) {
					g_speed_buff = zero;
					g_speed_buff.active = true;
					g_speed_buff.start_yaw = cam_yaw;
				}
			}

			{
				s_v3 gravity = v3(0, 0, -0.006);
				g_player.vel += gravity;

				if(g_time - g_player.want_to_jump_timestamp < 0.1f) {
					g_player.vel.z = 0.5f;
				}

				s_v3 movement = g_player.vel + player_wanted_dir * player_wanted_speed;
				s_v3 new_vel = g_player.vel;
				b8 z_modified = false;
				for(int i = 0; i < 3; i += 1) {
					constexpr int c_steps = 100;
					s_v3 temp_movement = zero;
					if(i == 0) { temp_movement.x = movement.x; }
					else if(i == 1) { temp_movement.y = movement.y; }
					else if(i == 2) { temp_movement.z = movement.z; }
					s_v3 small_movement = temp_movement / c_steps;
					for(int j = 0; j < c_steps; j += 1) {
						g_player.pos += small_movement;
						s_collision_data collision_data = check_collision(g_player.pos, make_box(g_player.pos, c_player_size));
						if(collision_data.triangle_arr.count > 0) {
							s_v3 normal = get_triangle_normal(collision_data.triangle_arr[0]);
							g_player.pos -= small_movement;
							if(i == 0) {
								float dot = v3_dot(v3(0, 0, 1), normal);
								dot = clamp(dot, 0.0f, 1.0f);
								b8 is_slope = dot > 0.2f;
								if(is_slope) {
									s_v3 n = v3(normal.x * -1, normal.y * -1, normal.z);
									s_v3 reflect = n;
									new_vel = reflect * (1 - dot);
									z_modified = true;
								}
							}
							if(i == 2) {
								if(!z_modified) {
									new_vel.z = 0;
								}
							}
							break;
						}
					}
				}
				g_player.vel = new_vel;
				g_player.vel.x *= 0.8f;
				g_player.vel.y *= 0.8f;
			}

			for(int i = 0; i < g_sphere_arr.count; i += 1) {
				s_sphere* sphere = &g_sphere_arr[i];
				sphere->vel.z -= 0.005f;
				constexpr int c_steps = 25;
				s_v3 small_movement = sphere->vel / c_steps;
				s_v3 total_collision_normal = zero;
				for(int j = 0; j < c_steps; j += 1) {
					sphere->pos += small_movement;
					s_collision_data collision_data = check_collision(sphere->pos, make_box(sphere->pos, v3(0.1f)));
					b8 did_we_collide = false;
					for(int collision_i = 0; collision_i < collision_data.triangle_arr.count; collision_i += 1) {
						did_we_collide = true;
						total_collision_normal += get_triangle_normal(collision_data.triangle_arr[collision_i]);
					}
					if(did_we_collide) {
						sphere->pos -= small_movement;
						s_v3 normal = v3_normalized(total_collision_normal);
						sphere->vel = v3_reflect(sphere->vel, normal) * 0.9f;
						break;
					}
				}
				float passed = g_time - sphere->spawn_timestamp;
				if(passed > 10) {
					g_sphere_arr[i] = g_sphere_arr[g_sphere_arr.count - 1];
					g_sphere_arr.count -= 1;
					i -= 1;
				}
			}

		}
		cam_pos = g_player.pos;
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		update end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		draw start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(g_device);
		if(cmdbuf == null) {
				SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
				return -1;
		}

		SDL_GPUTexture* swapchain_texture;
		if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, g_window, &swapchain_texture, null, null)) {
			SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
			return -1;
		}

		if(swapchain_texture != null) {

			SDL_GPUDepthStencilTargetInfo base_depth_stencil_target_info = zero;
			// base_depth_stencil_target_info.texture = shadow_texture;
			base_depth_stencil_target_info.cycle = true;
			base_depth_stencil_target_info.clear_depth = 1;
			// base_depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
			base_depth_stencil_target_info.store_op = SDL_GPU_STOREOP_STORE;
			base_depth_stencil_target_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
			base_depth_stencil_target_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

			SDL_GPUColorTargetInfo base_color_target_info = zero;
			// base_color_target_info.texture = swapchain_texture;
			base_color_target_info.clear_color = {0.2f, 0.2f, 0.3f, 1.0f};
			// base_color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
			base_color_target_info.store_op = SDL_GPU_STOREOP_STORE;

			s_v3 sun_pos = v3(-1, c_tiles_y * c_tile_size / 2, 10);
			s_v3 sun_dir = v3_normalized(v3(1, 0, -0.1f));

			s_m4 light_view = look_at(sun_pos, sun_pos + sun_dir, v3(0, 0, 1));
			s_m4 light_projection = make_orthographic(
				-c_tiles_x * 0.6f * c_tile_size, c_tiles_x * 0.6f * c_tile_size, -100, 100, -c_tiles_y * 1.1f * c_tile_size, c_tiles_y * 1.1f * c_tile_size
			);

			draw_mesh_world(e_mesh_terrain, m4_identity(), make_color(1), 0);
			for(int i = 0; i < g_sphere_arr.count; i += 1) {
				s_sphere sphere = g_sphere_arr[i];
				s_m4 model = m4_translate(sphere.pos);
				model = m4_multiply(model, m4_scale(v3(1.5f)));
				draw_mesh_world(e_mesh_sphere, model, make_color(0.5f, 1.0f, 0.5f), 0);
			}

			// {
			// 	s_m4 model = m4_translate(v3(c_window_size.x * 0.5f, c_window_size.y * 0.5f, 0));
			// 	model = m4_multiply(model, m4_rotate(g_time, v3(0, 1, 0)));
			// 	model = m4_multiply(model, m4_scale(v3(64, 64, 1)));
			// 	draw_mesh_screen(e_mesh_quad, model, make_color(1), 0);
			// }

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		scene to depth start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			{
				SDL_GPUDepthStencilTargetInfo depth_stencil_target_info = base_depth_stencil_target_info;
				depth_stencil_target_info.texture = shadow_texture;
				depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;

				SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmdbuf, null, 0, &depth_stencil_target_info);
				SDL_BindGPUGraphicsPipeline(render_pass, mesh_depth_only_pipeline);

				for(int mesh_i = 0; mesh_i < e_mesh_count; mesh_i += 1) {
					int instance_count = g_mesh_instance_data_arr[mesh_i].count;
					if(instance_count <= 0) { continue; }
					s_mesh* mesh = &g_mesh_arr[mesh_i];
					s_mesh_instance_data* instance_data = g_mesh_instance_data_arr[mesh_i].data;

					{
						upload_to_gpu_buffer(instance_data, sizeof(s_mesh_instance_data) * instance_count, mesh->instance_buffer, mesh->instance_transfer_buffer);
						SDL_GPUBufferBinding binding_arr[2] = zero;
						binding_arr[0].buffer = mesh->vertex_buffer;
						binding_arr[1].buffer = mesh->instance_buffer;
						SDL_BindGPUVertexBuffers(render_pass, 0, binding_arr, 2);
					}
					{
						s_vertex_uniform_data1 data = zero;
						data.world_view = light_view;
						data.world_projection = light_projection;
						data.light_view = light_view;
						data.light_projection = light_projection;
						data.depth_only = 1;
						SDL_PushGPUVertexUniformData(cmdbuf, 0, &data, sizeof(data));
					}
					SDL_DrawGPUPrimitives(render_pass, mesh->vertex_count, instance_count, 0, 0);
				}

				SDL_EndGPURenderPass(render_pass);
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		scene to depth end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			if(view_state == e_view_state_depth) {
				for(int mesh_i = 0; mesh_i < e_mesh_count; mesh_i += 1) {
					g_mesh_instance_data_arr[mesh_i].count = 0;
				}
				draw_screen(c_half_window_size, c_window_size, make_color(1));
			}

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		mesh start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			{
				SDL_GPUColorTargetInfo color_target_info = base_color_target_info;
				color_target_info.texture = swapchain_texture;
				color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;

				SDL_GPUDepthStencilTargetInfo depth_stencil_target_info = base_depth_stencil_target_info;
				depth_stencil_target_info.texture = scene_depth_texture;
				depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;

				SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, &depth_stencil_target_info);
				SDL_BindGPUGraphicsPipeline(render_pass, g_do_wireframe ? mesh_line_pipeline : mesh_fill_pipeline);

				for(int mesh_i = 0; mesh_i < e_mesh_count; mesh_i += 1) {
					int instance_count = g_mesh_instance_data_arr[mesh_i].count;
					if(instance_count <= 0) { continue; }
					s_mesh* mesh = &g_mesh_arr[mesh_i];
					s_mesh_instance_data* instance_data = g_mesh_instance_data_arr[mesh_i].data;

					{
						upload_to_gpu_buffer(instance_data, sizeof(s_mesh_instance_data) * instance_count, mesh->instance_buffer, mesh->instance_transfer_buffer);
						SDL_GPUBufferBinding binding_arr[2] = zero;
						binding_arr[0].buffer = mesh->vertex_buffer;
						binding_arr[1].buffer = mesh->instance_buffer;
						SDL_BindGPUVertexBuffers(render_pass, 0, binding_arr, 2);
					}

					{
						s_vertex_uniform_data1 data = zero;
						data.screen_view = m4_identity();
						data.screen_projection = make_orthographic(0, c_window_size.x, c_window_size.y, 0, -100, 100);
						if(view_state == e_view_state_shadow_map) {
							data.world_view = light_view;
							data.world_projection = light_projection;
						}
						else {
							data.world_view = look_at(cam_pos, cam_pos + cam_front, cam_up);
							data.world_projection = make_perspective(90, c_window_width / (float)c_window_height, 0.01f, 500.0f);
						}
						data.light_view = light_view;
						data.light_projection = light_projection;
						SDL_PushGPUVertexUniformData(cmdbuf, 0, &data, sizeof(data));
					}

					{
						s_fragment_uniform_data data = zero;
						data.cam_pos = cam_pos;
						SDL_PushGPUFragmentUniformData(cmdbuf, 0, &data, sizeof(data));
					}

					{
						SDL_GPUTextureSamplerBinding binding_arr[1] = zero;
						binding_arr[0].texture = shadow_texture;
						binding_arr[0].sampler = shadow_texture_sampler;
						SDL_BindGPUFragmentSamplers(render_pass, 0, binding_arr, 1);
					}

					SDL_DrawGPUPrimitives(render_pass, mesh->vertex_count, instance_count, 0, 0);
				}
				SDL_EndGPURenderPass(render_pass);
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		mesh end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			for(int mesh_i = 0; mesh_i < e_mesh_count; mesh_i += 1) {
				g_mesh_instance_data_arr[mesh_i].count = 0;
			}
			SDL_SubmitGPUCommandBuffer(cmdbuf);
		}

		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		draw end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

func float clamp(float curr, float min_val, float max_val)
{
	float result = curr;
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
t* s_list<t, n>::add(t new_element)
{
	assert(count < n);
	t* result = &this->data[this->count];
	this->data[this->count] = new_element;
	this->count += 1;
	return result;
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

func void draw_rect_screen(s_v2 pos, s_v2 size, s_v4 color)
{
	s_m4 model = m4_translate(v3(pos, 0));
	model = m4_multiply(model, m4_scale(v3(size, 1)));
	draw_mesh_screen(e_mesh_quad, model, color, 0);
}