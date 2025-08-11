#pragma once

#include "types.h"
#include <math.h>

// Platform-specific SIMD headers
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h> // For SSE intrinsics
#elif defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h> // For ARM NEON intrinsics
#endif

template <typename T>
union Vec2
{
    struct
    {
        T x, y;
    };

    struct
    {
        T u, v;
    };
};

template <typename T>
struct Vec3
{
    T x;
    T y;
    T z;
};

template <typename T>
union Vec4
{
    struct
    {
        T x, y, z, w;
    };
    struct
    {
        T r, g, b, a;
    };
};

template <typename T>
struct Rng1
{
    union
    {
        struct
        {
            T min;
            T max;
        };
        T v[2];
    };
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
struct Quaternion
{
    T x, y, z, w;
};

// Type aliases for common use
template <typename T>
using Mat4 = Mat4x4<T>;

template <typename T>
using Mat3 = Mat3x3<T>;

template <typename T>
using Quat = Quaternion<T>;

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
mat4x4_mul_simd(const Mat4x4<f32> &a, const Mat4x4<f32> &b)
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
            float32x4_t col  = {b.m[0][j], b.m[1][j], b.m[2][j], b.m[3][j]};
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
            __m128 col  = _mm_set_ps(b.m[3][j], b.m[2][j], b.m[1][j], b.m[0][j]);
            __m128 prod = _mm_mul_ps(row, col);

            // Horizontal add using SSE2 instructions
            __m128 shuf = _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 sums = _mm_add_ps(prod, shuf);
            shuf           = _mm_movehl_ps(shuf, sums);
            sums           = _mm_add_ss(sums, shuf);
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
mat3x3_mul_simd(const Mat3x3<f32> &a, const Mat3x3<f32> &b)
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
operator*(const Mat4x4<T> &a, const Mat4x4<T> &b)
{
#if 1
    return mat4x4_mul_simd(a, b);
#else
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
#endif
}

template <typename T>
inline Mat3x3<T>
operator*(const Mat3x3<T> &a, const Mat3x3<T> &b)
{
#if 1
    return mat3x3_mul_simd(a, b);
#else

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
#endif
}

template <typename T>
inline Mat3x3<T>
mat3x3_identity()
{
    Mat3x3<T> result{};
    result.m[0][0] = result.m[1][1] = result.m[2][2] = 1;
    return result;
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
    T c              = cos(angle);
    T s              = sin(angle);

    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;

    return result;
}

template<typename T>
inline Mat4x4<T>
mat4x4_rotate_x(T angle)
{
    Mat4x4<T> result = mat4x4_identity<T>();
    T c              = cos(angle);
    T s              = sin(angle);

    result.m[1][1] = c;
    result.m[1][2] = -s;
    result.m[2][1] = s;
    result.m[2][2] = c;

    return result;
}

template<typename T>
inline Mat4x4<T>
mat4x4_rotate_z(T angle)
{
    Mat4x4<T> result = mat4x4_identity<T>();
    T c              = cos(angle);
    T s              = sin(angle);

    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;

    return result;
}

#define Pi32 3.14159265359f
#define Tau32 6.28318530718f

#define Sin(x) sinf(x)
#define Cos(x) cosf(x)
#define Tan(x) tanf(x)
#define Sqrt(x) sqrtf(x)

template<typename T>
inline Vec3<T> operator+(Vec3<T> a, Vec3<T> b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

template<typename T>
inline Vec3<T> operator-(Vec3<T> a, Vec3<T> b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

template<typename T>
inline Vec3<T> operator*(Vec3<T> v, T s)
{
    return {v.x * s, v.y * s, v.z * s};
}

template<typename T>
inline T dot(Vec3<T> a, Vec3<T> b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template<typename T>
inline Vec3<T> cross(Vec3<T> a, Vec3<T> b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

template<typename T>
inline T length(Vec3<T> v)
{
    return Sqrt(dot(v, v));
}

template<typename T>
inline Vec3<T> normalize(Vec3<T> v)
{
    T len = length(v);
    if(len > 0) {
        T inv_len = 1.0f / len;
        return v * inv_len;
    }
    return v;
}

template<typename T>
inline Mat4x4<T> mat4x4_translate(Vec3<T> v)
{
    Mat4x4<T> result = mat4x4_identity<T>();
    result.m[3][0] = v.x;
    result.m[3][1] = v.y;
    result.m[3][2] = v.z;
    return result;
}

template<typename T>
inline Mat4x4<T> mat4x4_scale(Vec3<T> v)
{
    Mat4x4<T> result = {};
    result.m[0][0] = v.x;
    result.m[1][1] = v.y;
    result.m[2][2] = v.z;
    result.m[3][3] = 1;
    return result;
}

template<typename T>
inline Vec4<T> operator*(Mat4x4<T> m, Vec4<T> v)
{
    Vec4<T> result;
    result.x = m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0] * v.w;
    result.y = m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1] * v.w;
    result.z = m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2] * v.w;
    result.w = m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3] * v.w;
    return result;
}

template<typename T>
inline Mat4x4<T> mat4x4_inverse(Mat4x4<T> m)
{
    Mat4x4<T> result;

    T inv[16];
    T det;
    T invdet;

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

    if(det == 0) return mat4x4_identity<T>();

    invdet = 1.0f / det;

    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            result.m[i][j] = inv[i * 4 + j] * invdet;
        }
    }

    return result;
}

// Additional math utility functions
#ifndef PI
#define PI 3.14159265358979323846f
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif

template <typename T>
inline T clamp(T value, T min, T max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

template <typename T>
inline T lerp(T a, T b, T t)
{
    return a + (b - a) * t;
}

// Vector functions
template <typename T>
inline Vec3<T> vec3_add(Vec3<T> a, Vec3<T> b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

template <typename T>
inline Vec3<T> vec3_sub(Vec3<T> a, Vec3<T> b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

template <typename T>
inline Vec3<T> vec3_mul_scalar(Vec3<T> v, T s)
{
    return {v.x * s, v.y * s, v.z * s};
}

template <typename T>
inline T vec3_dot(Vec3<T> a, Vec3<T> b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
inline Vec3<T> vec3_cross(Vec3<T> a, Vec3<T> b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

template <typename T>
inline T vec3_length_sq(Vec3<T> v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

template <typename T>
inline T vec3_length(Vec3<T> v)
{
    return sqrt(vec3_length_sq(v));
}

template <typename T>
inline Vec3<T> vec3_normalize(Vec3<T> v)
{
    T len = vec3_length(v);
    if (len > 0) {
        return vec3_mul_scalar(v, T(1) / len);
    }
    return v;
}

template <typename T>
inline Vec3<T> vec3_lerp(Vec3<T> a, Vec3<T> b, T t)
{
    return {
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t),
        lerp(a.z, b.z, t)
    };
}

template <typename T>
inline Vec3<T> vec3_min(Vec3<T> a, Vec3<T> b)
{
    return {
        a.x < b.x ? a.x : b.x,
        a.y < b.y ? a.y : b.y,
        a.z < b.z ? a.z : b.z
    };
}

template <typename T>
inline Vec3<T> vec3_max(Vec3<T> a, Vec3<T> b)
{
    return {
        a.x > b.x ? a.x : b.x,
        a.y > b.y ? a.y : b.y,
        a.z > b.z ? a.z : b.z
    };
}

// Vec4 functions
template <typename T>
inline Vec4<T> vec4_normalize(Vec4<T> v)
{
    T len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    if (len > 0) {
        T inv_len = T(1) / len;
        return {v.x * inv_len, v.y * inv_len, v.z * inv_len, v.w * inv_len};
    }
    return v;
}

// Matrix function aliases
template <typename T>
inline Mat4<T> mat4_identity()
{
    return mat4x4_identity<T>();
}

template <typename T>
inline Mat4<T> mat4_translate(const Mat4<T>& m, Vec3<T> v)
{
    Mat4<T> result = m;
    result.m[3][0] = m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0];
    result.m[3][1] = m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1];
    result.m[3][2] = m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2];
    result.m[3][3] = m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3];
    return result;
}

template <typename T>
inline Mat4<T> mat4_rotate_y(const Mat4<T>& m, T angle)
{
    T c = cos(angle);
    T s = sin(angle);
    
    Mat4<T> rotation = mat4_identity<T>();
    rotation.m[0][0] = c;
    rotation.m[0][2] = s;
    rotation.m[2][0] = -s;
    rotation.m[2][2] = c;
    
    return m * rotation;
}

template <typename T>
inline Mat4<T> mat4_rotate_z(const Mat4<T>& m, T angle)
{
    T c = cos(angle);
    T s = sin(angle);
    
    Mat4<T> rotation = mat4_identity<T>();
    rotation.m[0][0] = c;
    rotation.m[0][1] = -s;
    rotation.m[1][0] = s;
    rotation.m[1][1] = c;
    
    return m * rotation;
}

template <typename T>
inline Mat4<T> mat4_mul(const Mat4<T>& a, const Mat4<T>& b)
{
    return a * b;
}

template <typename T>
inline Vec4<T> mat4_mul_vec4(const Mat4<T>& m, Vec4<T> v)
{
    return {
        m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0] * v.w,
        m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1] * v.w,
        m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2] * v.w,
        m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3] * v.w
    };
}

template <typename T>
inline Vec3<T> mat4_mul_vec3(const Mat4<T>& m, Vec3<T> v)
{
    Vec4<T> v4 = {v.x, v.y, v.z, T(1)};
    Vec4<T> result = mat4_mul_vec4(m, v4);
    return {result.x, result.y, result.z};
}

template <typename T>
inline Mat4<T> mat4_inverse(const Mat4<T>& m)
{
    return mat4x4_inverse(m);
}

// Quaternion functions
template <typename T>
inline Quat<T> quat_identity()
{
    return {T(0), T(0), T(0), T(1)};
}

template <typename T>
inline Quat<T> quat_from_axis_angle(Vec3<T> axis, T angle)
{
    T half_angle = angle * T(0.5);
    T s = sin(half_angle);
    return {
        axis.x * s,
        axis.y * s,
        axis.z * s,
        static_cast<T>(cos(half_angle))
    };
}

template <typename T>
inline Quat<T> quat_mul(Quat<T> a, Quat<T> b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

template <typename T>
inline Vec3<T> quat_rotate_vec3(Quat<T> q, Vec3<T> v)
{
    Vec3<T> qvec = {q.x, q.y, q.z};
    Vec3<T> uv = vec3_cross(qvec, v);
    Vec3<T> uuv = vec3_cross(qvec, uv);
    
    return vec3_add(vec3_add(v, vec3_mul_scalar(uv, T(2) * q.w)), vec3_mul_scalar(uuv, T(2)));
}

// Look-at matrix
template <typename T>
inline Mat4<T> mat4_look_at(Vec3<T> eye, Vec3<T> center, Vec3<T> up)
{
    Vec3<T> f = vec3_normalize(vec3_sub(center, eye));
    Vec3<T> s = vec3_normalize(vec3_cross(f, up));
    Vec3<T> u = vec3_cross(s, f);
    
    Mat4<T> result = mat4_identity<T>();
    result.m[0][0] = s.x;
    result.m[1][0] = s.y;
    result.m[2][0] = s.z;
    result.m[0][1] = u.x;
    result.m[1][1] = u.y;
    result.m[2][1] = u.z;
    result.m[0][2] = -f.x;
    result.m[1][2] = -f.y;
    result.m[2][2] = -f.z;
    result.m[3][0] = -vec3_dot(s, eye);
    result.m[3][1] = -vec3_dot(u, eye);
    result.m[3][2] = vec3_dot(f, eye);
    
    return result;
}

// Perspective projection
template <typename T>
inline Mat4<T> mat4_perspective(T fov_y, T aspect, T near_plane, T far_plane)
{
    return mat4x4_perspective(fov_y, aspect, near_plane, far_plane);
}

// Orthographic projection
template <typename T>
inline Mat4<T> mat4_orthographic(T left, T right, T bottom, T top, T near_plane, T far_plane)
{
    Mat4<T> result = {};
    result.m[0][0] = T(2) / (right - left);
    result.m[1][1] = T(2) / (top - bottom);
    result.m[2][2] = T(-2) / (far_plane - near_plane);
    result.m[3][0] = -(right + left) / (right - left);
    result.m[3][1] = -(top + bottom) / (top - bottom);
    result.m[3][2] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.m[3][3] = T(1);
    return result;
}

template <typename T>
inline Quat<T> quat_from_mat4(const Mat4<T>& m)
{
    Quat<T> q;
    T trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
    
    if (trace > 0) {
        T s = T(0.5) / sqrt(trace + T(1));
        q.w = T(0.25) / s;
        q.x = (m.m[2][1] - m.m[1][2]) * s;
        q.y = (m.m[0][2] - m.m[2][0]) * s;
        q.z = (m.m[1][0] - m.m[0][1]) * s;
    } else {
        if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
            T s = T(2) * sqrt(T(1) + m.m[0][0] - m.m[1][1] - m.m[2][2]);
            q.w = (m.m[2][1] - m.m[1][2]) / s;
            q.x = T(0.25) * s;
            q.y = (m.m[0][1] + m.m[1][0]) / s;
            q.z = (m.m[0][2] + m.m[2][0]) / s;
        } else if (m.m[1][1] > m.m[2][2]) {
            T s = T(2) * sqrt(T(1) + m.m[1][1] - m.m[0][0] - m.m[2][2]);
            q.w = (m.m[0][2] - m.m[2][0]) / s;
            q.x = (m.m[0][1] + m.m[1][0]) / s;
            q.y = T(0.25) * s;
            q.z = (m.m[1][2] + m.m[2][1]) / s;
        } else {
            T s = T(2) * sqrt(T(1) + m.m[2][2] - m.m[0][0] - m.m[1][1]);
            q.w = (m.m[1][0] - m.m[0][1]) / s;
            q.x = (m.m[0][2] + m.m[2][0]) / s;
            q.y = (m.m[1][2] + m.m[2][1]) / s;
            q.z = T(0.25) * s;
        }
    }
    return q;
}

template <typename T>
inline Quat<T> quat_slerp(Quat<T> a, Quat<T> b, T t)
{
    T dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    
    if (dot < 0) {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    
    if (dot > T(0.9995)) {
        return {
            lerp(a.x, b.x, t),
            lerp(a.y, b.y, t),
            lerp(a.z, b.z, t),
            lerp(a.w, b.w, t)
        };
    }
    
    T theta = acos(dot);
    T sin_theta = sin(theta);
    T wa = sin((T(1) - t) * theta) / sin_theta;
    T wb = sin(t * theta) / sin_theta;
    
    return {
        wa * a.x + wb * b.x,
        wa * a.y + wb * b.y,
        wa * a.z + wb * b.z,
        wa * a.w + wb * b.w
    };
}

