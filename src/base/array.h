#pragma once

#include "base.h"
#include "arena.h"

template <typename T, u32 N>
struct Array
{
    T data[N];
    u32 size;
};

template <typename T, u32 N>
inline T*
array_get(Array<T, N>* array, u32 index)
{
    if (index >= array->size)
    {
        return nullptr;
    }
    return &array->data[index];
}

template <typename T, u32 N>
inline const T*
array_get_const(const Array<T, N>* array, u32 index)
{
    if (index >= array->size)
    {
        return nullptr;
    }
    return &array->data[index];
}

template <typename T, u32 N>
inline void
array_push(Array<T, N>* array, T value)
{
    if (array->size < N)
    {
        array->data[array->size++] = value;
    }
}

template <typename T, u32 N>
inline T*
array_push_new(Array<T, N>* array)
{
    if (array->size < N)
    {
        return &array->data[array->size++];
    }
    return nullptr;
}

template <typename T, u32 N>
inline void
array_clear(Array<T, N>* array)
{
    array->size = 0;
}

template <typename T, u32 N>
inline T*
array_begin(Array<T, N>* array)
{
    return array->data;
}

template <typename T, u32 N>
inline T*
array_end(Array<T, N>* array)
{
    return array->data + array->size;
}

template <typename T>
struct Dynamic_Array
{
    T* data;
    u64 size;
    u64 capacity;
};

template <typename T>
inline Dynamic_Array<T>
dynamic_array_make()
{
    return {0, 0, 0};
}

template <typename T>
inline void
dynamic_array_reserve(Arena* arena, Dynamic_Array<T>* array, u64 new_capacity)
{
    if (new_capacity > array->capacity)
    {
        T* new_data = push_array(arena, T, new_capacity);
        
        if (array->data)
        {
            for (u64 i = 0; i < array->size; i++)
            {
                new_data[i] = array->data[i];
            }
        }
        
        array->data = new_data;
        array->capacity = new_capacity;
    }
}

template <typename T>
inline void
dynamic_array_push(Arena* arena, Dynamic_Array<T>* array, T value)
{
    if (array->size >= array->capacity)
    {
        u64 new_capacity = array->capacity ? array->capacity * 2 : 8;
        dynamic_array_reserve(arena, array, new_capacity);
    }
    
    array->data[array->size++] = value;
}

template <typename T>
inline T*
dynamic_array_push_new(Arena* arena, Dynamic_Array<T>* array)
{
    if (array->size >= array->capacity)
    {
        u64 new_capacity = array->capacity ? array->capacity * 2 : 8;
        dynamic_array_reserve(arena, array, new_capacity);
    }
    
    return &array->data[array->size++];
}

template <typename T>
inline void
dynamic_array_clear(Dynamic_Array<T>* array)
{
    array->size = 0;
}

template <typename T>
inline T*
dynamic_array_begin(Dynamic_Array<T>* array)
{
    return array->data;
}

template <typename T>
inline T*
dynamic_array_end(Dynamic_Array<T>* array)
{
    return array->data + array->size;
}

