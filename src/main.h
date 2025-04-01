
#define array_count(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define invalid_default_case default: { assert(false); }
#define assert(condition) if(!(condition)) { on_failed_assert(#condition, __FILE__, __LINE__); }

union s_m4
{
	float all[16];
	float all2[4][4];
};

enum e_mesh
{
	e_mesh_sphere,
	e_mesh_terrain,
	e_mesh_quad,
	e_mesh_count,
};

struct s_mesh
{
	int vertex_count;
	SDL_GPUBuffer* vertex_buffer;
	SDL_GPUBuffer* instance_buffer;
	SDL_GPUTransferBuffer* vertex_transfer_buffer;
	SDL_GPUTransferBuffer* instance_transfer_buffer;
};

struct s_shader_data
{
	u32 sampler_count[2];
	u32 uniform_buffer_count[2];
	u32 storage_buffer_count[2];
	u32 storage_texture_count[2];
};

struct s_shader_program
{
	SDL_GPUShader* shader_arr[2];
};

// @Note(tkap, 28/03/2025): If values change, shaders must also change
enum e_render_flag
{
	e_render_flag_dont_cast_shadows = 1 << 0,
	e_render_flag_ignore_light = 1 << 1,
	e_render_flag_ignore_fog = 1 << 2,
	e_render_flag_screen = 1 << 3,
	e_render_flag_textured = 1 << 4,
};

enum e_view_state
{
	e_view_state_default,
	e_view_state_shadow_map,
	e_view_state_depth,
};

enum e_ui_flag
{
	e_ui_flag_draggable = 1 << 0,
	e_ui_flag_clickable = 1 << 1,
	e_ui_flag_dynamic = 1 << 2,
	e_ui_flag_resizable = 1 << 3,
};

enum e_ui_state
{
	e_ui_state_clicked = 1 << 0,
	e_ui_state_pressed = 1 << 1,
};

struct s_v2
{
	float x;
	float y;
};

struct s_v3
{
	union {
		struct {
			float x;
			float y;
		};
		s_v2 xy;
	};
	float z;
};

struct s_v4
{
	union {
		struct {
			float x;
			float y;
			float z;
		};
		s_v3 xyz;
	};
	float w;
};

#pragma pack(push, 1)
struct s_vertex
{
	s_v3 pos;
	s_v3 normal;
	s_v4 color;
	s_v2 uv;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct s_mesh_instance_data
{
	s_v4 color;
	int flags;
	s_m4 model;
};
#pragma pack(pop)


struct s_sphere
{
	float spawn_timestamp;
	s_v3 pos;
	s_v3 vel;
};

template <typename t, int n>
struct s_list
{
	int count = 0;
	t data[n];
	t& operator[](int index);
	t* add(t new_element);
	t pop_last();
	void remove_and_swap(int index);
};

#pragma pack(push, 1)
struct s_ply_vertex
{
	s_v3 pos;
	s_v3 normal;
	s_v2 uv;
};

struct s_ply_face
{
	s8 index_count;
	int index_arr[3];
};
#pragma pack(pop)

struct s_ply_mesh
{
	int vertex_count;
	int face_count;
	s_ply_vertex vertex_arr[1024];
	s_ply_face face_arr[1024];
};

struct s_linear_arena
{
	int capacity;
	int used;
	u8* memory;
};

struct s_vertex_uniform_data1
{
	s_m4 world_view;
	s_m4 world_projection;
	s_m4 screen_view;
	s_m4 screen_projection;
	s_m4 light_view;
	s_m4 light_projection;
	int depth_only;
};


struct s_fragment_uniform_data
{
	s_v3 cam_pos;
};

struct s_player
{
	float want_to_shoot_timestamp;
	float want_to_jump_timestamp;
	b8 on_ground;
	s_v3 pos;
	s_v3 vel;
};

struct s_shape
{
	s_v3 vertices[16];
	int vertex_count;
};

struct s_triangle
{
	s_v3 vertex_arr[3];
};

struct s_collision_data
{
	s_list<s_triangle, 32> triangle_arr;
};

struct s_box
{
	s_v3 vertex_arr[8];
};

struct s_ui_optional
{
	b8 bottom_right;
	s_v2 size;
};

struct s_ui_widget
{
	int flags;
	int parent;
	int depth;
	s_v2 pos;
	s_ui_optional optional;

	s_v2 out_pos;
	s_v2 out_size;
};

struct s_ui_data
{
	s_v2 size;
};

struct s_ui
{
	int hot_index;
	int hot_state;

	int hovered;
	int depth;
	s_list<s_ui_widget, 64> widget_arr;
	s_list<int, 64> stack_arr;
	s_ui_data data_arr[64];
};

struct s_sound
{
	int sample_count;
	u8* data;
};

struct s_playing_sound
{
	b8 loop;
	s_sound sound;
	int cursor;
};

// -----------------------------------------------------------------------------------------------------------------------------
func constexpr s_v2 v2(float x, float y)
{
	s_v2 result = zero;
	result.x = x;
	result.y = y;
	return result;
}

func constexpr s_v2 v2(float x)
{
	s_v2 result = zero;
	result.x = x;
	result.y = x;
	return result;
}

template <typename T>
func constexpr s_v3 v3(T v)
{
	return {(float)v, (float)v, (float)v};
}

func constexpr s_v3 v3(s_v2 xy, float z)
{
	return {xy.x, xy.y, z};
}

template <typename A, typename B, typename C>
func constexpr s_v3 v3(A x, B y, C z)
{
	return {(float)x, (float)y, (float)z};
}

template <typename T>
func constexpr s_v4 v4(T v)
{
	return {(float)v, (float)v, (float)v, (float)v};
}

template <typename A, typename B, typename C, typename D>
func constexpr s_v4 v4(A x, B y, C z, D w)
{
	return {(float)x, (float)y, (float)z, (float)w};
}

func constexpr s_v2 operator+(s_v2 a, s_v2 b)
{
	return v2(
		a.x + b.x,
		a.y + b.y
	);
}

func constexpr s_v2 operator-(s_v2 a, s_v2 b)
{
	return v2(
		a.x - b.x,
		a.y - b.y
	);
}

func constexpr s_v3 operator+(s_v3 a, s_v3 b)
{
	return v3(
		a.x + b.x,
		a.y + b.y,
		a.z + b.z
	);
}

func constexpr s_v2 operator*(s_v2 a, float b)
{
	return v2(
		a.x * b,
		a.y * b
	);
}

func constexpr s_v3 operator*(s_v3 a, float b)
{
	return v3(
		a.x * b,
		a.y * b,
		a.z * b
	);
}

func constexpr s_v3 operator/(s_v3 a, float b)
{
	return v3(
		a.x / b,
		a.y / b,
		a.z / b
	);
}

func void operator+=(s_v2& a, s_v2 b)
{
	a.x += b.x;
	a.y += b.y;
}

func void operator-=(s_v2& a, s_v2 b)
{
	a.x -= b.x;
	a.y -= b.y;
}

func void operator*=(s_v2& a, float b)
{
	a.x *= b;
	a.y *= b;
}

func void operator+=(s_v3& a, s_v3 b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
}

func void operator-=(s_v3& a, s_v3 b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
}
// -----------------------------------------------------------------------------------------------------------------------------

func s_shader_program load_shader(char* path, s_shader_data shader_data);
func s_m4 m4_identity();
func s_m4 m4_rotate(float angle, s_v3 axis);
func s_v3 v3_normalized(s_v3 v);
func float v3_length_squared(s_v3 v);
func float v3_length(s_v3 v);
func s_m4 make_perspective(float FOV, float AspectRatio, float Near, float Far);
func constexpr s_v3 operator-(s_v3 a, s_v3 b);
func s_v3 v3_cross(s_v3 a, s_v3 b);
func float v3_dot(s_v3 a, s_v3 b);
func s_m4 look_at(s_v3 eye, s_v3 target, s_v3 up);
func float smoothstep(float edge0, float edge1, float x);
func float lerp(float a, float b, float t);
func float ilerp(float a, float b, float c);

template <typename t>
func t clamp(t curr, t min_val, t max_val);

func float smoothstep2(float edge0, float edge1, float x);
func s_v3 get_triangle_normal(s_triangle triangle);
func int roundfi(float x);
func SDL_GPUGraphicsPipeline* create_pipeline(
	s_shader_program shader, SDL_GPUFillMode fill_mode, int num_color_targets,
	s_list<SDL_GPUVertexElementFormat, 16> vertex_attributes, s_list<SDL_GPUVertexElementFormat, 16> instance_attributes,
	b8 has_depth
);
func s_m4 make_orthographic(float Left, float Right, float Bottom, float Top, float Near, float Far);
func b8 SATCollision3D(s_shape shapeA, s_shape shapeB);
func float get_triangle_height_at_xy(s_v3 t1, s_v3 t2, s_v3 t3, s_v2 p);

template <typename t>
func t max(t a, t b);

func float at_most(float a, float b);
func float sign(float x);
func s_v3 v3_set_mag(s_v3 v, float mag);
func s_v4 make_color(float r);
func void upload_to_gpu_buffer(void* data, int data_size, SDL_GPUBuffer* vertex_buffer, SDL_GPUTransferBuffer* transfer_buffer);
func s_m4 m4_scale(s_v3 v);
func s_m4 m4_multiply(s_m4 a, s_m4 b);
func s_m4 m4_translate(s_v3 v);
func s_v4 make_color(float r, float g, float b);
func s_v4 make_color(float r, float a);
func float min(float a, float b);
func int floorfi(float x);
func s_v3 v3_reflect(s_v3 a, s_v3 b);
func s_collision_data check_collision(s_v3 pos, s_box hitbox);
func u8* read_file(char* path);
func s_ply_mesh parse_ply_mesh(char* path);
func s_box make_box(s_v3 pos, s_v3 size);
func void draw_mesh_world(e_mesh mesh_id, s_m4 model, s_v4 color, int flags);
func void draw_mesh_screen(e_mesh mesh_id, s_m4 model, s_v4 color, int flags);
func void setup_common_mesh_stuff(s_mesh* mesh);
func void setup_mesh_vertex_buffers(s_mesh* mesh, int buffer_size);
func s_linear_arena make_arena_from_malloc(int requested_size);
func u8* arena_alloc(s_linear_arena* arena, int requested_size);
func void arena_reset(s_linear_arena* arena);
func void make_game_mesh_from_ply_mesh(s_mesh* mesh, s_ply_mesh* ply_mesh);
func s_triangle make_triangle(s_v3 v0, s_v3 v1, s_v3 v2);
func void draw_screen(s_v2 pos, s_v2 size, s_v4 color);
func SDL_EnumerationResult enumerate_directory_callback(void *userdata, const char *dirname, const char *fname);
func b8 is_shader_valid(s_shader_program program);
func void draw_quad_screen(s_v2 pos, float z, s_v2 size, s_v4 color);
func int ui_push_widget(s_v2 pos, s_v2 size, int flags);
func void ui_pop_widget();
func int ui_widget(int flags, s_ui_optional optional);
func b8 rect_vs_rect_topleft(s_v2 pos0, s_v2 size0, s_v2 pos1, s_v2 size1);
func b8 mouse_vs_rect_topleft(s_v2 pos, s_v2 size);
func s_v4 multiply_rgb(s_v4 a, float b);
func void process_ui();
func void ui_process_size(int widget_i, s_v2 size);
func void ui_process_pos(int curr_i, s_v2 pos);
func void on_failed_assert(char* condition, char* file, int line);
func void audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);
func void play_sound(s_sound sound, b8 loop);
func s_sound load_sound(char* path);
