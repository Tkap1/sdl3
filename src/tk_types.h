
#pragma once

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef double f64;
typedef float f32;

typedef bool b8;

#define func static
#define zero {}
#define null nullptr
#define global static

global constexpr u64 c_max_u8 = UINT8_MAX;
global constexpr u64 c_max_u16 = UINT16_MAX;
global constexpr u64 c_max_u32 = UINT32_MAX;
global constexpr u64 c_max_u64 = UINT64_MAX;

global constexpr u64 c_max_s8 = INT8_MAX;
global constexpr u64 c_max_s16 = INT16_MAX;
global constexpr u64 c_max_s32 = INT32_MAX;
global constexpr u64 c_max_s64 = INT64_MAX;
