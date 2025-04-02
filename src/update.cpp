
func void update()
{
	g_player.prev_pos = g_player.pos;

	s_v3 cam_front = get_cam_front(g_game.cam);
	{
		float passed = g_game.update_time - g_player.want_to_shoot_timestamp;
		if(passed <= 0.0f) {
			s_sphere sphere = zero;
			sphere.pos = g_player.pos + cam_front * 3;
			sphere.vel = cam_front * 0.75f;
			sphere.spawn_timestamp = g_game.update_time;
			g_sphere_arr.add(sphere);
		}
	}

	{
		s_v3 gravity = v3(0, 0, -0.006);
		g_player.vel += gravity;

		if(g_game.update_time - g_player.want_to_jump_timestamp < 0.1f) {
			g_player.vel.z = 0.5f;
		}

		s_v3 movement = g_player.vel + g_game.player_wanted_dir * g_game.player_wanted_speed;
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
		float passed = g_game.update_time - sphere->spawn_timestamp;
		if(passed > 10) {
			g_sphere_arr[i] = g_sphere_arr[g_sphere_arr.count - 1];
			g_sphere_arr.count -= 1;
			i -= 1;
		}
	}

	g_game.update_time += (float)c_update_delay;
}