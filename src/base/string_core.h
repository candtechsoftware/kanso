#pragma once

#include <string.h>
#include "arena.h"
#include "profile.h"
#include "util.h"
#if !defined(XXH_IMPLEMENTATION)
#    define XXH_IMPLEMENTATION
#    define XXH_STATIC_LINKING_ONLY
#    include "../../third_party/xxhash/xxhash.h"
#endif

typedef struct String String;
struct String {
    u8 *data;
    u32 size;
};

static inline String
str_zero() {
    String s = {0};
    return s;
}

typedef struct String_Node String_Node;
struct String_Node {
    String_Node *next;
    String       string;
};

typedef struct String_List String_List;
struct String_List {
    String_Node *first;
    String_Node *last;
    u64          node_count;
    u64          total_size;
};

typedef struct String_Join String_Join;
struct String_Join {
    String pre;
    String sep;
    String post;
};

typedef struct String32 String32;
struct String32 {
    u32 *data;
    u64  size;
};

typedef struct Unicode_Decode Unicode_Decode;
struct Unicode_Decode {
    u32 inc;
    u32 codepoint;
};

static inline String
cstr_to_string(const char *data, u32 size) {
    String result;
    result.data = (u8 *)data;
    result.size = size;
    return result;
}

#define to_string(str) cstr_to_string(str, sizeof(str) - 1)
#define str_lit(str)   cstr_to_string(str, sizeof(str) - 1)

static inline b32
str_match(String a, String b) {
    PROF_FUNCTION;
    if (a.size != b.size) {
        prof_end();
        return 0;
    }
    for (u32 i = 0; i < a.size; i++) {
        if (a.data[i] != b.data[i]) {
            Prof_End();
            return 0;
        }
    }
    Prof_End();
    return 1;
}

static inline Unicode_Decode
utf8_decode(u8 *str, u64 max) {
    Unicode_Decode result = {1, 0xFFFD}; // Default to replacement character

    if (max < 1)
        return result;

    u8 byte1 = str[0];

    // 1-byte sequence (ASCII)
    if ((byte1 & 0x80) == 0) {
        result.codepoint = byte1;
        result.inc = 1;
    }
    // 2-byte sequence
    else if ((byte1 & 0xE0) == 0xC0) {
        if (max < 2)
            return result;
        u8 byte2 = str[1];
        if ((byte2 & 0xC0) != 0x80)
            return result;

        result.codepoint = ((byte1 & 0x1F) << 6) | (byte2 & 0x3F);
        result.inc = 2;
    }
    // 3-byte sequence
    else if ((byte1 & 0xF0) == 0xE0) {
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
    else if ((byte1 & 0xF8) == 0xF0) {
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
string32_from_string(Arena *arena, String in) {
    String32 res = {0};
    if (in.size) {
        u64            cap = in.size;
        u32           *str = (u32 *)arena_push(arena, sizeof(u32) * (cap + 1), sizeof(u32));
        u8            *ptr = in.data;
        u8            *opl = ptr + in.size;
        u64            size = 0;
        Unicode_Decode consume;
        for (; ptr < opl; ptr += consume.inc) {
            consume = utf8_decode(ptr, opl - ptr);
            str[size] = consume.codepoint;
            size += 1;
        }

        str[size] = 0;
        arena_pop(arena, (cap - size) * 4);
        res.data = str;
        res.size = size;
    }

    return res;
}

static inline String
str(u8 *data, u32 size) {
    String result;
    result.data = data;
    result.size = size;
    return result;
}

static inline String
str_push_copy(Arena *arena, String src) {
    String result;
    result.size = src.size;
    result.data = push_array(arena, u8, src.size);
    MemoryCopy(result.data, src.data, src.size);
    return result;
}

// Create string from C string
static inline String
string_from_cstr(const char *cstr) {
    String result;
    result.data = (u8 *)cstr;
    result.size = 0;
    if (cstr) {
        while (cstr[result.size]) {
            result.size++;
        }
    }
    return result;
}

// Aliases for compatibility
#define push_string_copy str_push_copy
#define string_match     str_match

internal String_Node *
string_list_push_node_set_string(String_List *list, String_Node *node, String string) {
    SLLQueuePush(list->first, list->last, node);
    list->node_count += 1;
    list->total_size += string.size;
    node->string = string;
    return (node);
}
internal String_Node *
string_list_push(Arena *arena, String_List *list, String string) {
    String_Node *node = push_array_no_zero(arena, String_Node, 1);
    string_list_push_node_set_string(list, node, string);
    return (node);
}

internal u64
u64_hash_from_seed_str(u64 seed, String string) {
    u64 result = XXH3_64bits_withSeed(string.data, string.size, seed);
    return result;
}

internal u64
u64_hash_from_str(String string) {
    u64 result = u64_hash_from_seed_str(5381, string);
    return result;
}

internal String
string_copy(Arena *arena, String s) {
    String string;
    string.size = s.size;
    string.data = push_array_zero(arena, u8, string.size + 1);
    MemoryCopy(string.data, s.data, s.size);
    string.data[s.size] = 0;
    return string;
}

internal String
str_list_join(Arena *arena, String_List *list, String_Join *optional_params) {
    String_Join join = {0};
    if (optional_params != 0) {
        MemoryCopyStruct(&join, optional_params);
    }
    u64 sep_count = 0;
    if (list->node_count > 0) {
        sep_count = list->node_count - 1;
    }
    String res;
    res.size = join.pre.size + join.post.size + sep_count * join.sep.size + list->total_size;
    u8 *ptr = res.data = push_array_no_zero(arena, u8, res.size + 1);

    MemoryCopy(ptr, join.pre.data, join.pre.size);
    ptr += join.pre.size;
    for (String_Node *node = list->first; node != 0; node = node->next) {
        MemoryCopy(ptr, node->string.data, node->string.size);
        ptr += node->string.size;
        if (node->next != 0) {
            MemoryCopy(ptr, join.sep.data, join.sep.size);
            ptr += join.sep.size;
        }
    }
    MemoryCopy(ptr, join.post.data, join.post.size);
    ptr += join.post.size;

    *ptr = 0;
    return res;
}
