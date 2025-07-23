#ifndef STRING_H
#define STRING_H

#include "arena.h"
#include "types.h"
#include "util.h"

struct String
{
    u8* data;
    u32 size;
};

String inline cstr_to_string(const char* data, u32 size)
{
    return {(u8*)data, size};
}

inline String cstr_to_string(char* data, u32 size)
{
    return {(u8*)data, size};
}

// Macro to create a String from a string literal with automatic size calculation
#define to_string(str) cstr_to_string(str, sizeof(str) - 1)

bool inline
operator==(const String a, const String b)
{
    if (a.size != b.size)
    {
        return false;
    }
    return MemoryCopy(a.data, b.data, a.size) == 0;
}

bool inline
operator!=(const String a, const String b)
{
    return !(a == b);
}

struct String32
{
    u32* data;
    u64 size;
};

struct UnicodeDecode
{
    u32 inc;
    u32 codepoint;
};

static UnicodeDecode
utf8_decode(u8* str, u64 max)
{
    UnicodeDecode result = {1, 0xFFFD}; // Default to replacement character

    if (max < 1)
        return result;

    u8 byte1 = str[0];

    // 1-byte sequence (ASCII)
    if ((byte1 & 0x80) == 0)
    {
        result.codepoint = byte1;
        result.inc = 1;
    }
    // 2-byte sequence
    else if ((byte1 & 0xE0) == 0xC0)
    {
        if (max < 2)
            return result;
        u8 byte2 = str[1];
        if ((byte2 & 0xC0) != 0x80)
            return result;

        result.codepoint = ((byte1 & 0x1F) << 6) | (byte2 & 0x3F);
        result.inc = 2;
    }
    // 3-byte sequence
    else if ((byte1 & 0xF0) == 0xE0)
    {
        if (max < 3)
            return result;
        u8 byte2 = str[1];
        u8 byte3 = str[2];
        if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80)
            return result;

        result.codepoint = ((byte1 & 0x0F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F);
        result.inc = 3;
    }
    // 4-byte sequence
    else if ((byte1 & 0xF8) == 0xF0)
    {
        if (max < 4)
            return result;
        u8 byte2 = str[1];
        u8 byte3 = str[2];
        u8 byte4 = str[3];
        if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || (byte4 & 0xC0) != 0x80)
            return result;

        result.codepoint = ((byte1 & 0x07) << 18) | ((byte2 & 0x3F) << 12) | ((byte3 & 0x3F) << 6) | (byte4 & 0x3F);
        result.inc = 4;
    }

    return result;
}

static String32
string32_from_string(Arena* arena, String in)
{
    String32 result = {0};
    if (in.size)
    {
        u64 cap = in.size;
        u32* str = push_array_no_zero(arena, u32, cap + 1);
        u8* ptr = in.data;
        u8* opl = ptr + in.size;
        u64 size = 0;
        UnicodeDecode consume;
        for (; ptr < opl; ptr += consume.inc)
        {
            consume = utf8_decode(ptr, opl - ptr);
            str[size] = consume.codepoint;
            size += 1;
        }
        str[size] = 0;
        arena_pop(arena, (cap - size) * 4);
        result = String32{str, size};
    }
    return result;
}

inline bool
string_match(String a, String b)
{
    if (a.size != b.size) return false;
    for (u32 i = 0; i < a.size; i++)
    {
        if (a.data[i] != b.data[i]) return false;
    }
    return true;
}

inline String
push_string_copy(Arena* arena, String str)
{
    String result = {0};
    if (str.size > 0)
    {
        result.data = push_array(arena, u8, str.size);
        result.size = str.size;
        MemoryCopy(result.data, str.data, str.size);
    }
    return result;
}

#endif
