#ifndef STRING_H
#define STRING_H
#include <cstring>

#include "types.h"

struct String
{
    const char* data;
    u32 size;
};

String
inline cstr_to_string(const char* data, u32 size)
{
    return {data, size};
}

String
inline cstr_to_string(char* data, u32 size)
{
    return {data, size};
}

// Macro to create a String from a string literal with automatic size calculation
#define to_string(str) cstr_to_string(str, sizeof(str) - 1)

bool
inline operator==(const String a, const String b)
{
    if (a.size != b.size)
    {
        return false;
    }
    return std::memcmp(a.data, b.data, a.size) == 0;
}

bool
inline operator!=(const String a, const String b)
{
    return !(a == b);
}
#endif
