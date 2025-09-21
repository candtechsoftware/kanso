#pragma once

#include "arena.h"
#include "types.h"
#include "util.h"

typedef struct Dyn_Array Dyn_Array;
struct Dyn_Array {
    void *items;
    u64   count;
    u64   cap;
};

static inline void
dyn_array_make_with_capicity(Arena *arena, Dyn_Array *arr, u64 elem_size,
                             u64 align, u64 cap) {
    if (arr->cap >= cap)
        return;
    u64 new_cap = arr->cap ? arr->cap * 2 : 16;
    if (new_cap < cap)
        new_cap = cap;

    void *new_items = arena_push(arena, elem_size * new_cap, align);
    if (arr->items) {
        MemoryCopy(new_items, arr->items, elem_size * arr->count);
    }

    arr->items = new_items;
    arr->cap = new_cap;
}

static inline void *dyn_array_push_t(Arena *arena, Dyn_Array *arr, u64 elem_size, u64 align) {
    if (arr->count >= arr->cap) {
        u64 new_cap = arr->cap ? arr->cap * 2 : 16;
        dyn_array_make_with_capicity(arena, arr, elem_size, align, new_cap);
    }

    u8   *base = (u8 *)arr->items;
    void *slot = base + arr->count * elem_size;
    arr->count++;
    return slot;
}

static inline void *dyn_array_pop_t(Dyn_Array *arr, u64 elem_size) {
    if (arr->count == 0)
        return NULL;
    arr->count--;
    u8 *base = (u8 *)arr->items;
    return base + arr->count * elem_size;
}

static inline void *dyn_array_get_t(Dyn_Array *arr, u64 elem_size, u64 index) {
    if (index >= arr->count)
        return NULL;
    u8 *base = (u8 *)arr->items;
    return base + index * elem_size;
}

#define dyn_array_push(arena, arr, T) \
    ((T *)dyn_array_push_t((arena), (arr), sizeof(T), _Alignof(T)))

#define dyn_array_pop(arr, T) \
    ((T *)dyn_array_pop_t((arr), sizeof(T)))

#define dyn_array_get(arr, T, idx) \
    ((T *)dyn_array_get_t((arr), sizeof(T), (idx)))
