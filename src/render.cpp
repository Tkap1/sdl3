
func void render(float interp_dt)
{
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
				g_game.mesh_shader = new_shader_arr[0];
				g_game.depth_only_shader = new_shader_arr[1];

				if(g_game.mesh_fill_pipeline) {
					SDL_ReleaseGPUGraphicsPipeline(g_device, g_game.mesh_fill_pipeline);
				}
				if(g_game.mesh_line_pipeline) {
					SDL_ReleaseGPUGraphicsPipeline(g_device, g_game.mesh_line_pipeline);
				}
				if(g_game.mesh_depth_only_pipeline) {
					SDL_ReleaseGPUGraphicsPipeline(g_device, g_game.mesh_depth_only_pipeline);
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
					g_game.mesh_fill_pipeline = create_pipeline(g_game.mesh_shader, SDL_GPU_FILLMODE_FILL, 1, vertex_attributes, instance_attributes, true);
					g_game.mesh_line_pipeline = create_pipeline(g_game.mesh_shader, SDL_GPU_FILLMODE_LINE, 1, vertex_attributes, instance_attributes, true);
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
					g_game.mesh_depth_only_pipeline = create_pipeline(g_game.depth_only_shader, SDL_GPU_FILLMODE_FILL, 0, vertex_attributes, instance_attributes, true);
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

	s_v3 player_pos = lerp_v3(g_player.prev_pos, g_player.pos, interp_dt);
	s_v3 cam_pos = player_pos;

	s_v3 cam_front = get_cam_front(g_game.cam);
	cam_front = v3_normalized(cam_front);

	s_v3 cam_up = v3(
		sinf(g_game.cam.yaw) * sinf(g_game.cam.pitch) * cosf(g_game.cam.pitch),
		-cosf(g_game.cam.yaw) * sinf(g_game.cam.pitch) * cosf(g_game.cam.pitch),
		cosf(g_game.cam.pitch) * cosf(g_game.cam.pitch)
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

	if(!g_game.terrain_generated) {
		g_game.terrain_generated = false;

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

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(g_device);
	if(cmdbuf == null) {
		SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
		assert(false);
	}

	SDL_GPUTexture* swapchain_texture;
	if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, g_window, &swapchain_texture, null, null)) {
		SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
		assert(false);
	}

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		ui start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	#if 1
	{
		if(ui_push_widget(v2(300), v2(300), e_ui_flag_clickable | e_ui_flag_resizable)) {
		}

		ui_push_widget(zero, zero, e_ui_flag_clickable);
			if(ui_widget(e_ui_flag_clickable | e_ui_flag_dynamic, zero)) {
			}
			ui_widget(e_ui_flag_clickable | e_ui_flag_dynamic, zero);
		ui_pop_widget();

		ui_push_widget(zero, zero, e_ui_flag_clickable);
			ui_widget(e_ui_flag_clickable | e_ui_flag_dynamic, zero);
			ui_widget(e_ui_flag_clickable | e_ui_flag_dynamic, zero);
			ui_widget(e_ui_flag_clickable | e_ui_flag_dynamic, zero);
		ui_pop_widget();

		ui_pop_widget();

	}
	#endif
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		ui end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
			SDL_BindGPUGraphicsPipeline(render_pass, g_game.mesh_depth_only_pipeline);

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

		if(g_game.view_state == e_view_state_depth) {
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
			SDL_BindGPUGraphicsPipeline(render_pass, g_do_wireframe ? g_game.mesh_line_pipeline : g_game.mesh_fill_pipeline);

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
					if(g_game.view_state == e_view_state_shadow_map) {
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
}