#pragma once

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;
typedef int32_t  b32;

// Forward declaration
typedef union u128 u128;

// 128-bit type
union u128 {
    u8  bytes[16];
    u16 words[8];
    u32 dwords[4];
    u64 u64[2];
#if defined(__SIZEOF_INT128__)
    __uint128_t value;
#endif
};
