#pragma once

#include <string.h>
#include "arena.h"

typedef struct String String;
struct String
{
    u8* data;
    u32 size;
};

typedef struct String32 String32;
struct String32
{
    u32* data;
    u64  size;
};

typedef struct Unicode_Decode Unicode_Decode;
struct Unicode_Decode
{
    u32 inc;
    u32 codepoint;
};

static inline String
cstr_to_string(const char* data, u32 size)
{
    String result;
    result.data = (u8*)data;
    result.size = size;
    return result;
}

#define to_string(str) cstr_to_string(str, sizeof(str) - 1)
#define str_lit(str) cstr_to_string(str, sizeof(str) - 1)

static inline b32
str_match(String a, String b)
{
    if (a.size != b.size)
        return 0;
    for (u32 i = 0; i < a.size; i++)
    {
        if (a.data[i] != b.data[i])
            return 0;
    }
    return 1;
}

static inline Unicode_Decode
utf8_decode(u8* str, u64 max)
{
    Unicode_Decode result = {1, 0xFFFD}; // Default to replacement character

    if (max < 1)
        return result;

    u8 byte1 = str[0];

    // 1-byte sequence (ASCII)
    if ((byte1 & 0x80) == 0)
    {
        result.codepoint = byte1;
        result.inc       = 1;
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
        result.inc       = 2;
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
        result.inc       = 3;
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

        result.codepoint = ((byte1 & 0x07) << 18) | ((byte2 & 0x3F) << 12) | ((byte3 & 0x3F) << 6) |
                           (byte4 & 0x3F);
        result.inc = 4;
    }

    return result;
}

static inline String32
string32_from_string(Arena* arena, String in)
{
    String32 res = {0};
    if (in.size)
    {
        u64  cap  = in.size;
        u32* str  = (u32*)arena_push(arena, sizeof(u32) * (cap + 1), sizeof(u32));
        u8*  ptr  = in.data;
        u8*  opl  = ptr + in.size;
        u64  size = 0;
        Unicode_Decode consume;
        for (; ptr < opl; ptr += consume.inc)
        {
            consume   = utf8_decode(ptr, opl - ptr);
            str[size] = consume.codepoint;
            size     += 1;
        }

        str[size] = 0;
        arena_pop(arena, (cap - size) * 4);
        res.data = str;
        res.size = size;
    }

    return res;
}

static inline String
str8(u8 *data, u32 size)
{
    String result;
    result.data = data;
    result.size = size;
    return result;
}

static inline String
str8_push_copy(Arena *arena, String src)
{
    String result;
    result.size = src.size;
    result.data = push_array(arena, u8, src.size);
    MemoryCopy(result.data, src.data, src.size);
    return result;
}

// Create string from C string
static inline String
string8_from_cstr(const char* cstr)
{
    String result;
    result.data = (u8*)cstr;
    result.size = 0;
    if (cstr)
    {
        while (cstr[result.size])
        {
            result.size++;
        }
    }
    return result;
}

// Aliases for compatibility
#define push_string_copy str8_push_copy
#define string_match str_match
