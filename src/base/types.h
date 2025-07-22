#ifndef TYPES_H
#define TYPES_H

#include "simd.h"
#include <stdint.h>
#include <type_traits>

// Basic types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef int32_t b32;
template <typename T>
struct Vec2
{
    T x;
    T y;
};

template <typename T>
struct Vec3
{
    T x;
    T y;
    T z;
};

template <typename T>
struct Vec4
{
    T x;
    T y;
    T z;
    T w;
};

template <typename T>
struct Rng2
{
    Vec2<T> min;
    Vec2<T> max;
};

template <typename T>
struct Mat3x3
{
    T m[3][3];
};

template <typename T>
struct Mat4x4
{
    T m[4][4];
};

template <typename T>
inline Vec2<T>
operator+(const Vec2<T> a, const Vec2<T> b)
{
    return Vec2<T>{a.x + b.x, a.y + b.y};
}

template <typename T>
inline Vec2<T>
operator-(const Vec2<T> a, const Vec2<T> b)
{
    return Vec2<T>{a.x - b.x, a.y - b.y};
}

template <typename T>
inline Vec2<T>
operator*(const Vec2<T> a, const Vec2<T> b)
{
    return Vec2<T>{a.x * b.x, a.y * b.y};
}

template <typename T>
inline Vec2<T>
operator/(const Vec2<T> a, const Vec2<T> b)
{
    return Vec2<T>{a.x / b.x, a.y / b.y};
}

template <typename T>
inline Vec2<T>
operator/(const Vec2<T> a, T scalar)
{
    return Vec2<T>{a.x / scalar, a.y / scalar};
}

template <typename T>
inline bool
operator==(const Vec2<T> a, const Vec2<T> b)
{
    return a.x == b.x && a.y == b.y;
}

inline Mat4x4<f32>
mat4x4_mul_simd(const Mat4x4<f32>& a, const Mat4x4<f32>& b)
{
    Mat4x4<f32> result;

#if defined(__APPLE__) && TARGET_CPU_ARM64
    // Optimized ARM NEON implementation for Apple Silicon
    // Transpose b for better memory access pattern
    float32x4_t b_row0 = vld1q_f32(&b.m[0][0]);
    float32x4_t b_row1 = vld1q_f32(&b.m[1][0]);
    float32x4_t b_row2 = vld1q_f32(&b.m[2][0]);
    float32x4_t b_row3 = vld1q_f32(&b.m[3][0]);
    
    // Compute result rows
    for (int i = 0; i < 4; i++)
    {
        float32x4_t a_row = vld1q_f32(&a.m[i][0]);
        
        // Broadcast each element of a_row and multiply with corresponding b row
        float32x4_t prod0 = vmulq_n_f32(b_row0, vgetq_lane_f32(a_row, 0));
        float32x4_t prod1 = vmulq_n_f32(b_row1, vgetq_lane_f32(a_row, 1));
        float32x4_t prod2 = vmulq_n_f32(b_row2, vgetq_lane_f32(a_row, 2));
        float32x4_t prod3 = vmulq_n_f32(b_row3, vgetq_lane_f32(a_row, 3));
        
        // Sum all products
        float32x4_t sum = vaddq_f32(vaddq_f32(prod0, prod1), vaddq_f32(prod2, prod3));
        
        // Store result
        vst1q_f32(&result.m[i][0], sum);
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    // Generic ARM NEON implementation
    for (int i = 0; i < 4; i++)
    {
        float32x4_t row = vld1q_f32(&a.m[i][0]);
        for (int j = 0; j < 4; j++)
        {
            float32x4_t col = {b.m[0][j], b.m[1][j], b.m[2][j], b.m[3][j]};
            float32x4_t prod = vmulq_f32(row, col);
            result.m[i][j] = vaddvq_f32(prod); // ARM64 has horizontal add
        }
    }
#elif defined(__x86_64__) || defined(_M_X64)
    // x86 SSE implementation
    for (int i = 0; i < 4; i++)
    {
        __m128 row = _mm_loadu_ps(&a.m[i][0]);
        for (int j = 0; j < 4; j++)
        {
            __m128 col = _mm_set_ps(b.m[3][j], b.m[2][j], b.m[1][j], b.m[0][j]);
            __m128 prod = _mm_mul_ps(row, col);
            __m128 shuf = _mm_movehdup_ps(prod);
            __m128 sums = _mm_add_ps(prod, shuf);
            shuf = _mm_movehl_ps(shuf, sums);
            sums = _mm_add_ss(sums, shuf);
            result.m[i][j] = _mm_cvtss_f32(sums);
        }
    }
#else
    // Fallback scalar implementation
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            result.m[i][j] = 0;
            for (int k = 0; k < 4; k++)
            {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
#endif

    return result;
}

inline Mat3x3<f32>
mat3x3_mul_simd(const Mat3x3<f32>& a, const Mat3x3<f32>& b)
{
    Mat3x3<f32> result;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            result.m[i][j] = 0;
            for (int k = 0; k < 3; k++)
            {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }

    return result;
}

template <typename T>
inline Mat4x4<T>
operator*(const Mat4x4<T>& a, const Mat4x4<T>& b)
{
    if constexpr (std::is_same_v<T, f32>)
    {
        return mat4x4_mul_simd(a, b);
    }
    else
    {
        Mat4x4<T> result{};
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                for (int k = 0; k < 4; k++)
                {
                    result.m[i][j] += a.m[i][k] * b.m[k][j];
                }
            }
        }
        return result;
    }
}

template <typename T>
inline Mat3x3<T>
operator*(const Mat3x3<T>& a, const Mat3x3<T>& b)
{
    if constexpr (std::is_same_v<T, f32>)
    {
        return mat3x3_mul_simd(a, b);
    }
    else
    {
        Mat3x3<T> result{};
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                for (int k = 0; k < 3; k++)
                {
                    result.m[i][j] += a.m[i][k] * b.m[k][j];
                }
            }
        }
        return result;
    }
}

template <typename T>
inline Mat4x4<T>
mat4x4_identity()
{
    Mat4x4<T> result{};
    result.m[0][0] = result.m[1][1] = result.m[2][2] = result.m[3][3] = 1;
    return result;
}

template <typename T>
inline Mat4x4<T>
mat4x4_perspective(T fov_y, T aspect, T near_plane, T far_plane)
{
    Mat4x4<T> result{};
    T tan_half_fov = tan(fov_y * 0.5f);

    result.m[0][0] = 1 / (aspect * tan_half_fov);
    result.m[1][1] = 1 / tan_half_fov;
    result.m[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.m[2][3] = -1;
    result.m[3][2] = -(2 * far_plane * near_plane) / (far_plane - near_plane);

    return result;
}

template <typename T>
inline Mat4x4<T>
mat4x4_translate(T x, T y, T z)
{
    Mat4x4<T> result = mat4x4_identity<T>();
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

template <typename T>
inline Mat4x4<T>
mat4x4_scale(T x, T y, T z)
{
    Mat4x4<T> result{};
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    result.m[3][3] = 1;
    return result;
}

template <typename T>
inline Mat4x4<T>
mat4x4_rotate_y(T angle)
{
    Mat4x4<T> result = mat4x4_identity<T>();
    T c = cos(angle);
    T s = sin(angle);

    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;

    return result;
}

#endif