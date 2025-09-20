#pragma once

#include "types.h"
#include <math.h>

// Platform-specific SIMD headers
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#    include <xmmintrin.h> // For SSE intrinsics
#elif defined(__aarch64__) || defined(__arm64__)
#    include <arm_neon.h> // For ARM NEON intrinsics
#endif

// Vector types for f32
typedef union Vec2_f32 {
    struct
    {
        f32 x, y;
    };
    struct
    {
        f32 u, v;
    };
} Vec2_f32;

typedef union Vec2_f64 {
    struct
    {
        f64  x, y;
    };
    struct
    {
        f64 u, v;
    };
} Vec2_f64;

typedef struct Vec2_s16 {
    s16 x, y;
} Vec2_s16;

typedef struct Vec2_s32 {
    s32 x, y;
} Vec2_s32;

typedef struct Vec2_s64 {
    s64 x, y;
} Vec2_s64;

typedef struct Vec3_f32 {
    f32 x;
    f32 y;
    f32 z;
} Vec3_f32;

typedef union Vec4_f32 {
    struct
    {
        f32 x, y, z, w;
    };
    struct
    {
        f32 r, g, b, a;
    };
} Vec4_f32;

typedef struct Rng1_f32 {
    union {
        struct
        {
            f32 min;
            f32 max;
        };
        f32 v[2];
    };
} Rng1_f32;

typedef struct Rng1_u32 {
    union {
        struct
        {
            u32 min;
            u32 max;
        };
        u32 v[2];
    };
} Rng1_u32;

typedef struct Rng1_u64 {
    union {
        struct
        {
            u64 min;
            u64 max;
        };
        u64 v[2];
    };
} Rng1_u64;

typedef struct Rng2_s16 {
    Vec2_s16 min;
    Vec2_s16 max;
} Rng2_s16;

typedef struct Rng2_f32 {
    Vec2_f32 min;
    Vec2_f32 max;
} Rng2_f32;

typedef struct Mat3x3_f32 {
    f32 m[3][3];
} Mat3x3_f32;

typedef struct Mat4x4_f32 {
    f32 m[4][4];
} Mat4x4_f32;

typedef struct Quaternion_f32 {
    f32 x, y, z, w;
} Quaternion_f32;

// Type aliases for compatibility
typedef Mat4x4_f32     Mat4_f32;
typedef Mat3x3_f32     Mat3_f32;
typedef Quaternion_f32 Quat_f32;

// Generic template replacements - using f32 as default
typedef Vec2_f32 Vec2;

// Convenience macros for creating vectors
#define V2F32(x, y)       ((Vec2_f32){{(x), (y)}})
#define V2S32(x, y)       ((Vec2_s32){(x), (y)})
#define V2S64(x, y)       ((Vec2_s64){(x), (y)})
#define V4F32(x, y, z, w) ((Vec4_f32){{(x), (y), (z), (w)}})
typedef Vec3_f32       Vec3;
typedef Vec4_f32       Vec4;
typedef Mat3x3_f32     Mat3x3;
typedef Mat4x4_f32     Mat4x4;
typedef Quaternion_f32 Quaternion;

// Vec2 functions
static inline Vec2_f32
vec2_f32_add(Vec2_f32 a, Vec2_f32 b) {
    return (Vec2_f32){{a.x + b.x, a.y + b.y}};
}

static inline Vec2_f32
vec2_f32_sub(Vec2_f32 a, Vec2_f32 b) {
    return (Vec2_f32){{a.x - b.x, a.y - b.y}};
}

static inline Vec2_f32
vec2_f32_mul(Vec2_f32 a, Vec2_f32 b) {
    return (Vec2_f32){{a.x * b.x, a.y * b.y}};
}

static inline Vec2_f32
vec2_f32_div(Vec2_f32 a, Vec2_f32 b) {
    return (Vec2_f32){{a.x / b.x, a.y / b.y}};
}

static inline Vec2_f32
vec2_f32_div_scalar(Vec2_f32 a, f32 scalar) {
    return (Vec2_f32){{a.x / scalar, a.y / scalar}};
}

static inline b32
vec2_f32_equal(Vec2_f32 a, Vec2_f32 b) {
    return a.x == b.x && a.y == b.y;
}

static inline Mat4x4_f32
mat4x4_mul_simd(const Mat4x4_f32 *a, const Mat4x4_f32 *b) {
    Mat4x4_f32 result;

#if defined(__APPLE__) && TARGET_CPU_ARM64
    // Optimized ARM NEON implementation for Apple Silicon
    float32x4_t b_row0 = vld1q_f32(&b->m[0][0]);
    float32x4_t b_row1 = vld1q_f32(&b->m[1][0]);
    float32x4_t b_row2 = vld1q_f32(&b->m[2][0]);
    float32x4_t b_row3 = vld1q_f32(&b->m[3][0]);

    for (int i = 0; i < 4; i++) {
        float32x4_t a_row = vld1q_f32(&a->m[i][0]);

        float32x4_t prod0 = vmulq_n_f32(b_row0, vgetq_lane_f32(a_row, 0));
        float32x4_t prod1 = vmulq_n_f32(b_row1, vgetq_lane_f32(a_row, 1));
        float32x4_t prod2 = vmulq_n_f32(b_row2, vgetq_lane_f32(a_row, 2));
        float32x4_t prod3 = vmulq_n_f32(b_row3, vgetq_lane_f32(a_row, 3));

        float32x4_t sum = vaddq_f32(vaddq_f32(prod0, prod1), vaddq_f32(prod2, prod3));
        vst1q_f32(&result.m[i][0], sum);
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    for (int i = 0; i < 4; i++) {
        float32x4_t row = vld1q_f32(&a->m[i][0]);
        for (int j = 0; j < 4; j++) {
            float32x4_t col = {b->m[0][j], b->m[1][j], b->m[2][j], b->m[3][j]};
            float32x4_t prod = vmulq_f32(row, col);
            result.m[i][j] = vaddvq_f32(prod);
        }
    }
#elif defined(__x86_64__) || defined(_M_X64)
    for (int i = 0; i < 4; i++) {
        __m128 row = _mm_loadu_ps(&a->m[i][0]);
        for (int j = 0; j < 4; j++) {
            __m128 col = _mm_set_ps(b->m[3][j], b->m[2][j], b->m[1][j], b->m[0][j]);
            __m128 prod = _mm_mul_ps(row, col);

            __m128 shuf = _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 sums = _mm_add_ps(prod, shuf);
            shuf = _mm_movehl_ps(shuf, sums);
            sums = _mm_add_ss(sums, shuf);
            result.m[i][j] = _mm_cvtss_f32(sums);
        }
    }
#else
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i][j] = 0;
            for (int k = 0; k < 4; k++) {
                result.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
#endif

    return result;
}

static inline Mat3x3_f32
mat3x3_mul_simd(const Mat3x3_f32 *a, const Mat3x3_f32 *b) {
    Mat3x3_f32 result;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.m[i][j] = 0;
            for (int k = 0; k < 3; k++) {
                result.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }

    return result;
}

static inline Mat4x4_f32
mat4x4_mul(Mat4x4_f32 a, Mat4x4_f32 b) {
#if 1
    return mat4x4_mul_simd(&a, &b);
#else
    Mat4x4_f32 result = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return result;
#endif
}

static inline Mat3x3_f32
mat3x3_mul(Mat3x3_f32 a, Mat3x3_f32 b) {
#if 1
    return mat3x3_mul_simd(&a, &b);
#else
    Mat3x3_f32 result = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return result;
#endif
}

static inline Mat3x3_f32
mat3x3_identity(void) {
    Mat3x3_f32 result = {0};
    result.m[0][0] = result.m[1][1] = result.m[2][2] = 1;
    return result;
}

static inline Mat4x4_f32
mat4x4_identity(void) {
    Mat4x4_f32 result = {0};
    result.m[0][0] = result.m[1][1] = result.m[2][2] = result.m[3][3] = 1;
    return result;
}

static inline Mat4x4_f32
mat4x4_perspective(f32 fov_y, f32 aspect, f32 near_plane, f32 far_plane) {
    Mat4x4_f32 result = {0};
    f32        tan_half_fov = tanf(fov_y * 0.5f);

    result.m[0][0] = 1.0f / (aspect * tan_half_fov);
    result.m[1][1] = 1.0f / tan_half_fov;
    result.m[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.m[2][3] = -1;
    result.m[3][2] = -(2 * far_plane * near_plane) / (far_plane - near_plane);

    return result;
}

static inline Mat4x4_f32
mat4x4_translate(f32 x, f32 y, f32 z) {
    Mat4x4_f32 result = mat4x4_identity();
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

static inline Mat4x4_f32
mat4x4_scale(f32 x, f32 y, f32 z) {
    Mat4x4_f32 result = {0};
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    result.m[3][3] = 1;
    return result;
}

static inline Mat4x4_f32
mat4x4_rotate_y(f32 angle) {
    Mat4x4_f32 result = mat4x4_identity();
    f32        c = cosf(angle);
    f32        s = sinf(angle);

    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;

    return result;
}

static inline Mat4x4_f32
mat4x4_rotate_x(f32 angle) {
    Mat4x4_f32 result = mat4x4_identity();
    f32        c = cosf(angle);
    f32        s = sinf(angle);

    result.m[1][1] = c;
    result.m[1][2] = -s;
    result.m[2][1] = s;
    result.m[2][2] = c;

    return result;
}

static inline Mat4x4_f32
mat4x4_rotate_z(f32 angle) {
    Mat4x4_f32 result = mat4x4_identity();
    f32        c = cosf(angle);
    f32        s = sinf(angle);

    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;

    return result;
}

#define Pi32  3.14159265359f
#define Tau32 6.28318530718f

#define Sin(x)  sinf(x)
#define Cos(x)  cosf(x)
#define Tan(x)  tanf(x)
#define Sqrt(x) sqrtf(x)

static inline Vec3_f32
vec3_f32_add(Vec3_f32 a, Vec3_f32 b) {
    return (Vec3_f32){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3_f32
vec3_f32_sub(Vec3_f32 a, Vec3_f32 b) {
    return (Vec3_f32){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3_f32
vec3_f32_mul_scalar(Vec3_f32 v, f32 s) {
    return (Vec3_f32){v.x * s, v.y * s, v.z * s};
}

static inline f32
vec3_f32_dot(Vec3_f32 a, Vec3_f32 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3_f32
vec3_f32_cross(Vec3_f32 a, Vec3_f32 b) {
    return (Vec3_f32){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

static inline f32
vec3_f32_length(Vec3_f32 v) {
    return Sqrt(vec3_f32_dot(v, v));
}

static inline Vec3_f32
vec3_f32_normalize(Vec3_f32 v) {
    f32 len = vec3_f32_length(v);
    if (len > 0) {
        f32 inv_len = 1.0f / len;
        return vec3_f32_mul_scalar(v, inv_len);
    }
    return v;
}

static inline Mat4x4_f32
mat4x4_translate_vec3(Vec3_f32 v) {
    Mat4x4_f32 result = mat4x4_identity();
    result.m[3][0] = v.x;
    result.m[3][1] = v.y;
    result.m[3][2] = v.z;
    return result;
}

static inline Mat4x4_f32
mat4x4_scale_vec3(Vec3_f32 v) {
    Mat4x4_f32 result = {0};
    result.m[0][0] = v.x;
    result.m[1][1] = v.y;
    result.m[2][2] = v.z;
    result.m[3][3] = 1;
    return result;
}

static inline Vec4_f32
mat4x4_mul_vec4(Mat4x4_f32 m, Vec4_f32 v) {
    Vec4_f32 result;
    result.x = m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0] * v.w;
    result.y = m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1] * v.w;
    result.z = m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2] * v.w;
    result.w = m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3] * v.w;
    return result;
}

static inline Mat4x4_f32
mat4x4_inverse(Mat4x4_f32 m) {
    Mat4x4_f32 result;

    f32 inv[16];
    f32 det;
    f32 invdet;

    inv[0] = m.m[1][1] * m.m[2][2] * m.m[3][3] -
             m.m[1][1] * m.m[2][3] * m.m[3][2] -
             m.m[2][1] * m.m[1][2] * m.m[3][3] +
             m.m[2][1] * m.m[1][3] * m.m[3][2] +
             m.m[3][1] * m.m[1][2] * m.m[2][3] -
             m.m[3][1] * m.m[1][3] * m.m[2][2];

    inv[4] = -m.m[1][0] * m.m[2][2] * m.m[3][3] +
             m.m[1][0] * m.m[2][3] * m.m[3][2] +
             m.m[2][0] * m.m[1][2] * m.m[3][3] -
             m.m[2][0] * m.m[1][3] * m.m[3][2] -
             m.m[3][0] * m.m[1][2] * m.m[2][3] +
             m.m[3][0] * m.m[1][3] * m.m[2][2];

    inv[8] = m.m[1][0] * m.m[2][1] * m.m[3][3] -
             m.m[1][0] * m.m[2][3] * m.m[3][1] -
             m.m[2][0] * m.m[1][1] * m.m[3][3] +
             m.m[2][0] * m.m[1][3] * m.m[3][1] +
             m.m[3][0] * m.m[1][1] * m.m[2][3] -
             m.m[3][0] * m.m[1][3] * m.m[2][1];

    inv[12] = -m.m[1][0] * m.m[2][1] * m.m[3][2] +
              m.m[1][0] * m.m[2][2] * m.m[3][1] +
              m.m[2][0] * m.m[1][1] * m.m[3][2] -
              m.m[2][0] * m.m[1][2] * m.m[3][1] -
              m.m[3][0] * m.m[1][1] * m.m[2][2] +
              m.m[3][0] * m.m[1][2] * m.m[2][1];

    det = m.m[0][0] * inv[0] + m.m[0][1] * inv[4] + m.m[0][2] * inv[8] + m.m[0][3] * inv[12];

    if (det == 0)
        return mat4x4_identity();

    invdet = 1.0f / det;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i][j] = inv[i * 4 + j] * invdet;
        }
    }

    return result;
}

#ifndef PI
#    define PI 3.14159265358979323846f
#endif

#ifndef FLT_MAX
#    define FLT_MAX 3.402823466e+38F
#endif

static inline f32
clamp_f32(f32 value, f32 min, f32 max) {
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

static inline f32
lerp_f32(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

static inline f32
vec3_f32_length_sq(Vec3_f32 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline Vec3_f32
vec3_f32_lerp(Vec3_f32 a, Vec3_f32 b, f32 t) {
    return (Vec3_f32){
        lerp_f32(a.x, b.x, t),
        lerp_f32(a.y, b.y, t),
        lerp_f32(a.z, b.z, t)};
}

static inline Vec3_f32
vec3_f32_min(Vec3_f32 a, Vec3_f32 b) {
    return (Vec3_f32){
        a.x < b.x ? a.x : b.x,
        a.y < b.y ? a.y : b.y,
        a.z < b.z ? a.z : b.z};
}

static inline Vec3_f32
vec3_f32_max(Vec3_f32 a, Vec3_f32 b) {
    return (Vec3_f32){
        a.x > b.x ? a.x : b.x,
        a.y > b.y ? a.y : b.y,
        a.z > b.z ? a.z : b.z};
}

static inline Vec4_f32
vec4_f32_normalize(Vec4_f32 v) {
    f32 len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    if (len > 0) {
        f32 inv_len = 1.0f / len;
        return (Vec4_f32){{v.x * inv_len, v.y * inv_len, v.z * inv_len, v.w * inv_len}};
    }
    return v;
}
