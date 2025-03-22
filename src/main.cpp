
#include <stdlib.h>
#include <stdio.h>
#include "SDL3/SDL.h"

#include "tk_types.h"
#include "intrin.h"
#include "main.h"

#define FNL_IMPL
#include "FastNoiseLite.h"

global char* BasePath = "";
global SDL_GPUGraphicsPipeline* fill_pipeline;
global SDL_GPUGraphicsPipeline* line_pipeline;
global SDL_GPUGraphicsPipeline* screen_pipeline;
global SDL_GPUGraphicsPipeline* depth_only_pipeline;
global SDL_GPUTexture* scene_depth_texture;
global SDL_GPUTexture* shadow_texture;
global SDL_GPUSampler* shadow_texture_sampler;
global b8 UseWireframeMode = false;
global SDL_GPUBuffer* VertexBuffer;
global constexpr int c_tiles_x = 512;
global constexpr int c_tiles_y = 512;
global constexpr int c_vertex_count = c_tiles_x * c_tiles_y * 6;
global float g_time = 0;
global constexpr float c_pi = 3.1415926535f;
global constexpr int c_window_width = 1920;
global constexpr int c_window_height = 1080;
global s_player player;
global float cam_yaw;
global float cam_pitch;
global s_vertex* transfer_data;
global SDL_GPUDevice* g_device;
global SDL_Window* g_window;
global SDL_GPUTextureFormat g_depth_texture_format = SDL_GPU_TEXTUREFORMAT_INVALID;

int main()
{
	SDL_Init(SDL_INIT_VIDEO);

	player.pos.x = 10;
	player.pos.y = 10;
	player.pos.z = 20;

	g_device = SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV,
		true,
		null
	);

	g_window = SDL_CreateWindow("3D", c_window_width, c_window_height, 0);
	SDL_ClaimWindowForGPUDevice(g_device, g_window);

	// prefered depth texture format in order of preference
	SDL_GPUTextureFormat prefered_depth_formats[] =
	{
		SDL_GPU_TEXTUREFORMAT_D24_UNORM,
		SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
		SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
		SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
	};

	for (int i = 0; i < array_count(prefered_depth_formats); i += 1)
	{
		SDL_GPUTextureFormat format = prefered_depth_formats[i];
		bool is_supported = SDL_GPUTextureSupportsFormat(g_device, (SDL_GPUTextureFormat) format, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
		printf("texture format index: %i, value: %i is supported = %s\n", i, format, is_supported ? "true" : "false");
		if (is_supported && g_depth_texture_format == SDL_GPU_TEXTUREFORMAT_INVALID)
			g_depth_texture_format = format;
	}

	// your device does not support any required depth texutre format
	assert(g_depth_texture_format != SDL_GPU_TEXTUREFORMAT_INVALID);

	SDL_GPUShader* vertexShader = load_shader("PositionColor.vert", 0, 1, 0, 0);
	if(vertexShader == null) {
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GPUShader* fragmentShader = load_shader("PositionColor.frag", 1, 1, 0, 0);
	if(fragmentShader == null) {
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

	SDL_GPUShader* screen_vertex_shader = load_shader("screen.vert", 0, 1, 0, 0);
	SDL_GPUShader* screen_fragment_shader = load_shader("screen.frag", 1, 0, 0, 0);

	SDL_GPUShader* depth_only_vertex_shader = load_shader("depth_only.vert", 0, 1, 0, 0);
	SDL_GPUShader* depth_only_fragment_shader = load_shader("depth_only.frag", 0, 0, 0, 0);

	{
		SDL_GPUVertexElementFormat arr[3] = zero;
		arr[0] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
		arr[1] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
		arr[2] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
		fill_pipeline = create_pipeline(vertexShader, fragmentShader, SDL_GPU_FILLMODE_FILL, 1, arr, array_count(arr), true);
		line_pipeline = create_pipeline(vertexShader, fragmentShader, SDL_GPU_FILLMODE_LINE, 1, arr, array_count(arr), true);
	}
	screen_pipeline = create_pipeline(screen_vertex_shader, screen_fragment_shader, SDL_GPU_FILLMODE_FILL, 1, null, 0, false);

	{
		SDL_GPUVertexElementFormat arr[3] = zero;
		arr[0] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
		arr[1] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
		arr[2] = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
		depth_only_pipeline = create_pipeline(depth_only_vertex_shader, depth_only_fragment_shader, SDL_GPU_FILLMODE_FILL, 0, arr, array_count(arr), true);
	}

	// Clean up shader resources
	SDL_ReleaseGPUShader(g_device, vertexShader);
	SDL_ReleaseGPUShader(g_device, fragmentShader);
	SDL_ReleaseGPUShader(g_device, screen_vertex_shader);
	SDL_ReleaseGPUShader(g_device, screen_fragment_shader);
	SDL_ReleaseGPUShader(g_device, depth_only_vertex_shader);
	SDL_ReleaseGPUShader(g_device, depth_only_fragment_shader);


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

	// {
	// 	SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(g_device, g_window);
	// 	SDL_GPUTextureCreateInfo info = zero;
	// 	info.type = SDL_GPU_TEXTURETYPE_2D;
	// 	info.width = c_window_width;
	// 	info.height = c_window_height;
	// 	info.layer_count_or_depth = 1;
	// 	info.num_levels = 1;
	// 	info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	// 	info.format = format;
	// 	info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	// 	shadow_texture = SDL_CreateGPUTexture(g_device, &info);
	// }

	e_view_state view_state = e_view_state_default;

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

	SDL_GPUBufferCreateInfo buffer_create_info = zero;
	buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
	buffer_create_info.size = sizeof(s_vertex) * c_vertex_count;
	VertexBuffer = SDL_CreateGPUBuffer(g_device, &buffer_create_info);

	// s_v3 cam_pos = v3(10, -3, 50);
	s_v3 cam_pos = player.pos;

	SDL_SetWindowRelativeMouseMode(g_window, true);

	SDL_GPUTransferBufferCreateInfo transfer_create_info = zero;
	transfer_create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transfer_create_info.size = sizeof(s_vertex) * c_vertex_count;
	SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(g_device, &transfer_create_info);

	SDL_GPUTransferBufferLocation location = zero;
	location.transfer_buffer = transfer_buffer;

	SDL_GPUBufferRegion region = zero;
	region.buffer = VertexBuffer;
	region.size = sizeof(s_vertex) * c_vertex_count;

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
					UseWireframeMode = !UseWireframeMode;
				}
				else if(event.key.key == SDLK_F) {
					generate_terrain = true;
				}
				else if(event.key.key == SDLK_SPACE) {
					player.vel.z = 0.5f;
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
			else if(event.type == SDL_EVENT_MOUSE_MOTION) {
				constexpr float sens = 0.002f;
				cam_yaw -= event.motion.xrel * sens;
				cam_pitch -= event.motion.yrel * sens;
				cam_pitch = clamp(cam_pitch, -c_pi * 0.4f, c_pi * 0.4f);
			}
		}


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
			player.pos += v3_normalized(dir) * delta * 50;
		}

		if(generate_terrain) {
			generate_terrain = false;

			transfer_data = (s_vertex*)SDL_MapGPUTransferBuffer(g_device, transfer_buffer, false);

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
				float scale_arr[c_biome_count] = {2, 8, 1, 1, 20, 30};
				fnl_state noise_arr[c_biome_count + 1] = zero;
				for(int i = 0; i < c_biome_count + 1; i += 1) {
					noise_arr[i] = fnlCreateState();
					// noise_arr[i].seed = (u32)__rdtsc();
					noise_arr[i].seed = 1;
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
						s_v3 color_arr[6] = zero;
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
							s_v3 color = v3(0, 0, 0);
							for(int j = 0; j < c_biome_count; j += 1) {
								height += p_arr[j] / total_p * fnlGetNoise2D(&noise_arr[j], x_arr[i], y_arr[i]);
								height_scale += p_arr[j] / total_p * scale_arr[j];
								color += biome_color_arr[j] * (p_arr[j] / total_p);
							}
							height_arr[i] = height;
							height_scale_arr[i] = height_scale;
							color_arr[i] = color;
							z_arr[i] = height * height_scale;
						}

						s_v3 normal_arr[2] = zero;
						normal_arr[0] = get_triangle_normal(v3(x_arr[0], y_arr[0], z_arr[0]), v3(x_arr[1], y_arr[1], z_arr[1]), v3(x_arr[2], y_arr[2], z_arr[2]));
						normal_arr[1] = get_triangle_normal(v3(x_arr[3], y_arr[3], z_arr[3]), v3(x_arr[4], y_arr[4], z_arr[4]), v3(x_arr[5], y_arr[5], z_arr[5]));

						for(int i = 0; i < 6; i += 1) {
							transfer_data[count++] = {x_arr[i], y_arr[i], height_arr[i] * height_scale_arr[i], normal_arr[i / 3], color_arr[i], 1};
						}
					}
				}

				// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		smooth shading start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
				{
					s_v3* sum_normal_arr = (s_v3*)calloc(1, sizeof(s_v3) * c_vertex_count);
					for(int i = 0; i < c_vertex_count; i += 1) {
						int x = roundfi(transfer_data[i].x);
						int y = roundfi(transfer_data[i].y);
						sum_normal_arr[x + y * c_tiles_x] += transfer_data[i].normal;
					}
					for(int i = 0; i < c_vertex_count; i += 1) {
						int x = roundfi(transfer_data[i].x);
						int y = roundfi(transfer_data[i].y);
						transfer_data[i].normal = v3_normalized(sum_normal_arr[x + y * c_tiles_x]);
					}
					free(sum_normal_arr);
				}
				// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		smooth shading end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
			}

			SDL_UnmapGPUTransferBuffer(g_device, transfer_buffer);

			SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(g_device);
			SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

			SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
			SDL_EndGPUCopyPass(copyPass);
			SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
			// SDL_ReleaseGPUTransferBuffer(g_device, transferBuffer);
		}

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		update start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		{
			int x = (int)player.pos.x;
			int y = (int)player.pos.y;
			float ground = transfer_data[(x + y * c_tiles_x) * 6].z + 5;
			player.vel.z -= delta;
			player.pos.z += player.vel.z;
			if(player.pos.z <= ground) {
				player.pos.z = ground;
				player.vel.z = 0;
			}
		}
		cam_pos = player.pos;
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

			float s = sinf(g_time * 0.2f);
			s_v3 sun_pos = v3(0, 0, 20);
			// the sun direction along the x axis with some down direction
			s_v3 sun_dir = v3_normalized(v3(1, 0, (s * 0.5f + 0.5f) * -0.2f));

			s_m4 light_view = look_at(sun_pos, sun_pos + sun_dir, v3(0, 0, 1));

			// we view the world along the x axis (see sun_dir), the near/far values are relative to the c_files_x
			// we see the worls y axis as our screen x axis, so we set left/right along that axis -c_tiles_y/0
			s_m4 light_projection = make_orthographic(-c_tiles_y, 0, -30, 100, -100 - c_tiles_x, 100 + c_tiles_x);

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		scene to depth start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			{

				SDL_GPUDepthStencilTargetInfo depth_stencil_target_info = zero;
				depth_stencil_target_info.texture = shadow_texture;
				depth_stencil_target_info.cycle = true;
				depth_stencil_target_info.clear_depth = 1;
				depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
				depth_stencil_target_info.store_op = SDL_GPU_STOREOP_STORE;
				depth_stencil_target_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
				depth_stencil_target_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

				SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmdbuf, null, 0, &depth_stencil_target_info);
				SDL_BindGPUGraphicsPipeline(render_pass, depth_only_pipeline);
				{
					SDL_GPUBufferBinding binding = zero;
					binding.buffer = VertexBuffer;
					SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);
				}
				{
					s_vertex_uniform_data0 data = zero;
					data.model = m4_identity();
					data.view = light_view;
					data.projection = light_projection;
					SDL_PushGPUVertexUniformData(cmdbuf, 0, &data, sizeof(data));
				}
				// {
				// 	s_fragment_uniform_data data = zero;
				// 	data.cam_pos = cam_pos;
				// 	SDL_PushGPUFragmentUniformData(cmdbuf, 0, &data, sizeof(data));
				// }
				SDL_DrawGPUPrimitives(render_pass, c_vertex_count, 1, 0, 0);
				SDL_EndGPURenderPass(render_pass);
			}
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		scene to depth end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

			if(view_state == e_view_state_depth) {
				SDL_GPUColorTargetInfo color_target_info = { 0 };
					color_target_info.texture = swapchain_texture;
					color_target_info.clear_color = { 0.2f, 0.2f, 0.3f, 1.0f };
					color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
					color_target_info.store_op = SDL_GPU_STOREOP_STORE;

					SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, null);
					SDL_BindGPUGraphicsPipeline(render_pass, screen_pipeline);
					{
						s_v4 color = v4(1, 1, 1, 1);
						SDL_PushGPUVertexUniformData(cmdbuf, 0, &color, sizeof(color));
					}
					{
						SDL_GPUTextureSamplerBinding binding = zero;
						binding.texture = shadow_texture;
						binding.sampler = shadow_texture_sampler;
						SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
					}
					SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
					SDL_EndGPURenderPass(render_pass);
			}
			else {
				// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		draw scene start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
				{
					SDL_GPUColorTargetInfo color_target_info = { 0 };
					color_target_info.texture = swapchain_texture;
					color_target_info.clear_color = { 0.2f, 0.2f, 0.3f, 1.0f };
					color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
					color_target_info.store_op = SDL_GPU_STOREOP_STORE;

					SDL_GPUDepthStencilTargetInfo depth_stencil_target_info = zero;
					depth_stencil_target_info.texture = scene_depth_texture;
					depth_stencil_target_info.cycle = true;
					depth_stencil_target_info.clear_depth = 1;
					depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
					depth_stencil_target_info.store_op = SDL_GPU_STOREOP_STORE;
					depth_stencil_target_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
					depth_stencil_target_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

					SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, &depth_stencil_target_info);
					SDL_BindGPUGraphicsPipeline(render_pass, fill_pipeline);
					{
						SDL_GPUBufferBinding binding = zero;
						binding.buffer = VertexBuffer;
						SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);
					}
					{
						s_vertex_uniform_data1 data = zero;
						data.model = m4_identity();
						if(view_state == e_view_state_shadow_map) {
							data.view = light_view;
							data.projection = light_projection;
						}
						else {
							data.view = look_at(cam_pos, cam_pos + cam_front, cam_up);
							data.projection = make_perspective(90, c_window_width / (float)c_window_height, 0.01f, 500.0f);
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
						SDL_GPUTextureSamplerBinding binding = zero;
						binding.texture = shadow_texture;
						binding.sampler = shadow_texture_sampler;
						SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
					}
					SDL_DrawGPUPrimitives(render_pass, c_vertex_count, 1, 0, 0);
					SDL_EndGPURenderPass(render_pass);
				}
				// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		draw scene end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
			}

			// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		scene to screen start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
			// {
			// 	SDL_GPUColorTargetInfo color_target_info = { 0 };
			// 	color_target_info.texture = swapchain_texture;
			// 	color_target_info.clear_color = { 0.2f, 0.2f, 0.3f, 1.0f };
			// 	color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
			// 	color_target_info.store_op = SDL_GPU_STOREOP_STORE;

			// 	SDL_GPUDepthStencilTargetInfo depth_stencil_target_info = zero;
			// 	depth_stencil_target_info.texture = scene_depth_texture;
			// 	depth_stencil_target_info.cycle = true;
			// 	depth_stencil_target_info.clear_depth = 1;
			// 	depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
			// 	depth_stencil_target_info.store_op = SDL_GPU_STOREOP_STORE;
			// 	depth_stencil_target_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
			// 	depth_stencil_target_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

			// 	SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, &depth_stencil_target_info);
			// 	SDL_BindGPUGraphicsPipeline(render_pass, screen_pipeline);
			// 	{
			// 		s_v4 color = v4(1, 1, 1, 1);
			// 		SDL_PushGPUVertexUniformData(cmdbuf, 0, &color, sizeof(color));
			// 	}
			// 	{
			// 		SDL_GPUTextureSamplerBinding binding = zero;
			// 		binding.texture = shadow_texture;
			// 		binding.sampler = shadow_texture_sampler;
			// 		SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
			// 	}

			// 	SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
			// 	SDL_EndGPURenderPass(render_pass);
			// }
			// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		scene to screen end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
		}

		SDL_SubmitGPUCommandBuffer(cmdbuf);
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		draw end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		loop end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	return 0;
}

func SDL_GPUShader* load_shader(
	const char* shaderFilename,
	Uint32 samplerCount,
	Uint32 uniformBufferCount,
	Uint32 storageBufferCount,
	Uint32 storageTextureCount
)
{
	// Auto-detect the shader stage from the file name for convenience
	SDL_GPUShaderStage stage;
	if(SDL_strstr(shaderFilename, ".vert")) {
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	}
	else if(SDL_strstr(shaderFilename, ".frag")) {
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	}
	else {
		SDL_Log("Invalid shader stage!");
		return null;
	}

	char fullPath[256];
	SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(g_device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char *entrypoint;

	if(backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%sassets/%s.spv", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	}
	else {
		SDL_Log("%s", "Unrecognized backend shader format!");
		return null;
	}

	size_t codeSize;
	void* code = SDL_LoadFile(fullPath, &codeSize);
	if(code == null) {
		SDL_Log("Failed to load shader from disk! %s", fullPath);
		return null;
	}

	SDL_GPUShaderCreateInfo shaderInfo = zero;
	shaderInfo.code = (u8*)code;
	shaderInfo.code_size = codeSize;
	shaderInfo.entrypoint = entrypoint;
	shaderInfo.format = format;
	shaderInfo.stage = stage;
	shaderInfo.num_samplers = samplerCount;
	shaderInfo.num_uniform_buffers = uniformBufferCount;
	shaderInfo.num_storage_buffers = storageBufferCount;
	shaderInfo.num_storage_textures = storageTextureCount;

	SDL_GPUShader* shader = SDL_CreateGPUShader(g_device, &shaderInfo);
	if(shader == null) {
		SDL_Log("Failed to create shader!");
		SDL_free(code);
		return null;
	}

	SDL_free(code);
	return shader;
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

	s_m4 Result = m4_identity();

	axis = v3_normalized(axis);

	float SinTheta = sinf(angle);
	float CosTheta = cosf(angle);
	float CosValue = 1.0f - CosTheta;

	Result.all2[0][0] = (axis.x * axis.x * CosValue) + CosTheta;
	Result.all2[0][1] = (axis.x * axis.y * CosValue) + (axis.z * SinTheta);
	Result.all2[0][2] = (axis.x * axis.z * CosValue) - (axis.y * SinTheta);

	Result.all2[1][0] = (axis.y * axis.x * CosValue) - (axis.z * SinTheta);
	Result.all2[1][1] = (axis.y * axis.y * CosValue) + CosTheta;
	Result.all2[1][2] = (axis.y * axis.z * CosValue) + (axis.x * SinTheta);

	Result.all2[2][0] = (axis.z * axis.x * CosValue) + (axis.y * SinTheta);
	Result.all2[2][1] = (axis.z * axis.y * CosValue) - (axis.x * SinTheta);
	Result.all2[2][2] = (axis.z * axis.z * CosValue) + CosTheta;

	return (Result);
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

func s_v3 get_triangle_normal(s_v3 v1, s_v3 v2, s_v3 v3)
{
	s_v3 t1 = v2 - v1;
	s_v3 t2 = v3 - v1;
	s_v3 normal = v3_cross(t2, t1);
	return v3_normalized(normal);
}

func int roundfi(float x)
{
	float result = roundf(x);
	return (int)result;
}

func SDL_GPUGraphicsPipeline* create_pipeline(
	SDL_GPUShader* vertex_shader, SDL_GPUShader* fragment_shader, SDL_GPUFillMode fill_mode, int num_color_targets,
	SDL_GPUVertexElementFormat* element_format_arr, int element_format_count,
	b8 has_depth
)
{
	SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = zero;
	pipeline_create_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_create_info.vertex_shader = vertex_shader;
	pipeline_create_info.fragment_shader = fragment_shader;
	pipeline_create_info.target_info.num_color_targets = num_color_targets;
	pipeline_create_info.target_info.has_depth_stencil_target = has_depth;
	pipeline_create_info.depth_stencil_state.enable_depth_test = has_depth;
	pipeline_create_info.depth_stencil_state.enable_depth_write = has_depth;
	pipeline_create_info.target_info.depth_stencil_format = g_depth_texture_format;
	pipeline_create_info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
	pipeline_create_info.depth_stencil_state.write_mask = 0xFF;
	SDL_GPUColorTargetDescription color_target_description = {
		.format = SDL_GetGPUSwapchainTextureFormat(g_device, g_window)
	};
	pipeline_create_info.target_info.color_target_descriptions = num_color_targets > 0 ? &color_target_description : null;
	pipeline_create_info.vertex_input_state.num_vertex_buffers = element_format_count > 0 ? 1 : 0;
	SDL_GPUVertexBufferDescription gpu_vertex_buffer_description = zero;
	gpu_vertex_buffer_description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

	int pitch = 0;
	SDL_GPUVertexAttribute vertex_attribute_arr[16] = zero;
	for(int i = 0; i < element_format_count; i += 1) {
		vertex_attribute_arr[i].format = element_format_arr[i];
		vertex_attribute_arr[i].location = i;
		vertex_attribute_arr[i].offset = pitch;

		switch(element_format_arr[i]) {
			case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3: {
				pitch += sizeof(float) * 3;
			} break;
			case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4: {
				pitch += sizeof(float) * 4;
			} break;
			invalid_default_case;
		}
	}
	gpu_vertex_buffer_description.pitch = pitch;
	pipeline_create_info.vertex_input_state.vertex_buffer_descriptions = &gpu_vertex_buffer_description;
	pipeline_create_info.vertex_input_state.num_vertex_attributes = element_format_count;
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
	Result.all2[3][1] = 0.5f * (Bottom + Top) / (Bottom - Top);
	Result.all2[3][2] = (Near + Far) / (Near - Far);

	return (Result);
}

func s_m4 make_orthographic_ai(float left, float right, float bottom, float top, float near, float far)
{
	s_m4 matrix = zero;

	float rl = right - left;
	float tb = top - bottom;
	float fn = far - near;

	matrix.all[0] = 2.0f / rl;
	matrix.all[5] = 2.0f / tb;
	matrix.all[10] = -2.0f / fn;
	matrix.all[15] = 1.0f;

	matrix.all[12] = -(right + left) / rl;
	matrix.all[13] = -(top + bottom) / tb;
	matrix.all[14] = -(far + near) / fn;

	return matrix;
}
