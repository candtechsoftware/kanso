#pragma once

#include "arena.h"
#include "types.h"
#include "util.h"

// Simple array types following raddebugger pattern
typedef struct U32Array U32Array;
struct U32Array
{
    u64  count;
    u32 *v;
};

typedef struct U64Array U64Array;
struct U64Array
{
    u64  count;  
    u64 *v;
};

typedef struct F32Array F32Array;
struct F32Array
{
    u64  count;
    f32 *v;
};

// Generic dynamic array for when type safety isn't needed
typedef struct Array Array;
struct Array
{
    void *data;
    u32   size;
    u32   cap;
    u32   element_size;
    b32   resizable;
};

// Array helper functions for the simple types
internal U32Array u32_array_make(Arena *arena, u64 count);
internal U64Array u64_array_make(Arena *arena, u64 count);
internal F32Array f32_array_make(Arena *arena, u64 count);
