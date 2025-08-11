#pragma once

#include "arena.h"
#include "types.h"
#include "util.h"

template <typename T>
struct Array
{
    T  *data;
    u32 size;
    u32 cap;
    b32 resizable = false;
};

template <typename T>
inline Array<T>
array_make()
{
    return {0, 0, 0};
}

template <typename T>
inline void
array_reserve(Arena *arena, Array<T> *array, u32 new_capacity)
{
    if (new_capacity > array->cap)
    {
        T *new_data = push_array(arena, T, new_capacity);

        if (array->data)
        {
            for (u32 i = 0; i < array->size; i++)
            {
                new_data[i] = array->data[i];
            }
        }

        array->data = new_data;
        array->cap  = new_capacity;
    }
}

template <typename T>
inline Array<T>
array_create_and_reserve(Arena *arena, u32 cap)
{
    Array<T> arr = array_make<T>();
    array_reserve(arena, &arr, cap);
    return arr;
}

template <typename T>
inline T *
array_get(Array<T> *array, u32 index)
{
    if (index >= array->size)
    {
        ASSERT(index >= array->size, "Trying to get something outside of the array bounds");
        return nullptr;
    }
    return &array->data[index];
}

template <typename T>
inline const T *
array_get_const(const Array<T> *array, u32 index)
{
    if (index >= array->size)
    {
        ASSERT(index >= array->size, "Trying to get something outside of the array bounds");
        return nullptr;
    }
    return &array->data[index];
}

template <typename T>
inline void
array_push(Arena *arena, Array<T> *array, T value)
{
    if (array->size >= array->cap )
    {
        ASSERT(array->resizable, "Array is not resizeable (size={d}, cap={d})\n", array->size, array->cap);
        u32 new_capacity = array->cap ? array->cap * 2 : 8;
        array_reserve(arena, array, new_capacity);
    }

    array->data[array->size++] = value;
}

template <typename T>
inline T *
array_push_new(Arena *arena, Array<T> *array)
{
    if (array->size >= array->cap)
    {
        ASSERT(array->resizable, "Array is not resizeable (size={d}, cap={d})\n", array->size, array->cap);
        u32 new_capacity = array->cap ? array->cap * 2 : 8;
        array_reserve(arena, array, new_capacity);
    }

    return &array->data[array->size++];
}

template <typename T>
inline void
array_clear(Array<T> *array)
{
    array->size = 0;
}

template <typename T>
inline T *
array_begin(Array<T> *array)
{
    return array->data;
}

template <typename T>
inline T *
array_end(Array<T> *array)
{
    return array->data + array->size;
}

