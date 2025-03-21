
#define array_count(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define invalid_default_case default: { assert(false); }
#define assert(condition) if(!(condition)) { printf("FUCK\n"); exit(1); }

union s_m4
{
	float all[16];
	float all2[4][4];
};

struct s_v3
{
	float x;
	float y;
	float z;
};

struct s_v4
{
	float x;
	float y;
	float z;
	float w;
};

struct s_vertex
{
	float x;
	float y;
	float z;
	s_v3 normal;
	s_v3 color;
	float a;
};

struct s_vertex_uniform_data
{
	s_m4 model;
	s_m4 view;
	s_m4 projection;
};

struct s_fragment_uniform_data
{
	s_v3 cam_pos;
};

struct s_player
{
	s_v3 pos;
	s_v3 vel;
};

func SDL_GPUShader* load_shader(
	const char* shaderFilename,
	Uint32 samplerCount,
	Uint32 uniformBufferCount,
	Uint32 storageBufferCount,
	Uint32 storageTextureCount
);
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
func float clamp(float curr, float min_val, float max_val);
func float smoothstep2(float edge0, float edge1, float x);
func s_v3 get_triangle_normal(s_v3 v1, s_v3 v2, s_v3 v3);
func int roundfi(float x);
func SDL_GPUGraphicsPipeline* create_pipeline(
	SDL_GPUShader* vertex_shader, SDL_GPUShader* fragment_shader, SDL_GPUFillMode fill_mode, SDL_GPUVertexElementFormat* element_format_arr, int element_format_count
);

template <typename T>
func constexpr s_v3 v3(T v)
{
	return {(float)v, (float)v, (float)v};
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

func constexpr s_v3 operator+(s_v3 a, s_v3 b)
{
	return v3(
		a.x + b.x,
		a.y + b.y,
		a.z + b.z
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

func void operator+=(s_v3& a, s_v3 b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
}