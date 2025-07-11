typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef bool b32; 

typedef float f32;
typedef double f64;

#define S64_MAX 0x7FFFFFFFFFFFFFFF
#define S64_MIN (-1 - 0x7FFFFFFFFFFFFFFF)
#define S32_MAX 0x7FFFFFFF
#define S32_MIN (-1 - 0x7FFFFFFF)


template<typename T> 
struct Vec2 {
    T x;
    T y;
};


template<typename T> 
struct Vec4 {
    T x;
    T y;
    T z;
    T w; 
};


template<typename T> 
struct Rng2 {
    Vec2<T> min; 
    Vec2<T> max;
};


template<typename T> 
struct Mat4x4{
    T v[4][4];
};
