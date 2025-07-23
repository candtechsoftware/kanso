#ifndef ARENA_H
#define ARENA_H

#include "types.h"

#define ARENA_HEADER_SIZE 128

struct Arena_Params
{
    u64 reserve_size;
    u64 commit_size;
    const char* allocation_site_file;
    int allocation_site_line;
};

struct Arena
{
    Arena* prev;
    Arena* current;
    u64 cmt_size;
    u64 res_size;
    u64 base_pos;
    u64 pos;
    u64 cmt;
    u64 res;

    const char* allocation_site_file;
    int allocation_site_line;
};

struct Scratch
{
    Arena* arena;
    u64 pos;
};

extern u64 arena_default_reserve_size;
extern u64 arena_default_commit_size;

Arena*
arena_alloc_(Arena_Params* params);

inline Arena*
arena_alloc_default(u64 reserve_size, u64 commit_size, const char* file, int line)
{
    Arena_Params params = {
        .reserve_size = reserve_size,
        .commit_size = commit_size,
        .allocation_site_file = file,
        .allocation_site_line = line};
    return arena_alloc_(&params);
}

#define arena_alloc() \
    arena_alloc_default(arena_default_reserve_size, arena_default_commit_size, __FILE__, __LINE__)

void
arena_release(Arena* arena);

void*
arena_push(Arena* arena, u64 size, u64 align);
u64
arena_pos(Arena* arena);
void
arena_pop_to(Arena* arena, u64 pos);

void
arena_clear(Arena* arena);
void
arena_pop(Arena* arena, u64 amt);

Scratch
scratch_begin(Arena* arena);
void
scratch_end(Scratch* scratch);

// Convenience macros for common push operations
#define push_array(arena, T, count) (T*)arena_push((arena), sizeof(T) * (count), alignof(T))
#define push_array_zero(arena, T, count) (T*)MemoryZero(push_array(arena, T, count), sizeof(T) * (count))
#define push_struct(arena, T) push_array(arena, T, 1)
#define push_struct_zero(arena, T) push_array_zero(arena, T, 1)

#define push_array_no_zero_aligned(a, T, c, align) (T *)arena_push((a), sizeof(T)*(c), (align))
#define push_array_aligned(a, T, c, align) (T *)MemoryZero(push_array_no_zero_aligned(a, T, c, align), sizeof(T)*(c))
#define push_array_no_zero(a, T, c) push_array_no_zero_aligned(a, T, c, ((8) > (alignof(T)) ? (8) : (alignof(T))))

#endif // ARENA_H
