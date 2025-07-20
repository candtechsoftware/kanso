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
inline to_string(const char* data, u32 size)
{
    return {data, size};
}

String
inline to_string(char* data, u32 size)
{
    return {data, size};
}

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
