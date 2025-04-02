
func void handle_input()
{
	b8* key_arr = (b8*)SDL_GetKeyboardState(null);

	SDL_Event event = zero;
	while(SDL_PollEvent(&event)) {
		if(event.type == SDL_EVENT_QUIT) {
			g_game.quit = true;
		}
		else if(event.type == SDL_EVENT_KEY_DOWN) {
			if(event.key.key == SDLK_LEFT) {
				g_do_wireframe = !g_do_wireframe;
			}
			else if(event.key.key == SDLK_F) {
				g_game.terrain_generated = true;
			}
			else if(event.key.key == SDLK_SPACE) {
				g_player.want_to_jump_timestamp = g_game.update_time;
			}
			else if(event.key.key == SDLK_G) {
				if(g_game.view_state == e_view_state_shadow_map) {
					g_game.view_state = e_view_state_default;
				}
				else {
					g_game.view_state = e_view_state_shadow_map;
				}
			}
			else if(event.key.key == SDLK_H) {
				if(g_game.view_state == e_view_state_depth) {
					g_game.view_state = e_view_state_default;
				}
				else {
					g_game.view_state = e_view_state_depth;
				}
			}
		}
		else if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			if(event.button.button == SDL_BUTTON_LEFT) {
				g_left_down = true;
				g_left_down_this_frame = true;
				g_player.want_to_shoot_timestamp = g_game.update_time;
			}
		}
		else if(event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			if(event.button.button == SDL_BUTTON_LEFT) {
				g_left_down = false;
			}
		}
		else if(event.type == SDL_EVENT_MOUSE_MOTION) {
			constexpr float sens = 0.002f;
			g_game.cam.yaw -= event.motion.xrel * sens;
			g_game.cam.pitch -= event.motion.yrel * sens;
			g_game.cam.pitch = clamp(g_game.cam.pitch, -c_pi * 0.4f, c_pi * 0.4f);
		}
	}

	{
		s_v2 temp;
		SDL_GetGlobalMouseState(&temp.x, &temp.y);
		g_mouse_delta = temp - g_mouse;
		g_mouse = temp;
	}

	g_game.player_wanted_dir = zero;
	g_game.player_wanted_speed = 0.1f;
	{
		s_v3 cam_front = get_cam_front(g_game.cam);
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
			g_game.player_wanted_speed *= 10;
		}
		g_game.player_wanted_dir = v3_normalized(dir);
	}
}