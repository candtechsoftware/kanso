#pragma once
#pragma once

#include "types.h"
#include "arena.h"

template <typename T>
struct List_Node
{
    List_Node<T> *next;
    List_Node<T> *prev;
    T v;
};

template <typename T>
struct List
{
    List_Node<T> *first;
    List_Node<T> *last;
    u64 count;
};

template <typename T>
inline List<T>
list_make()
{
    List<T> result = {0};
    return result;
}

template <typename T>
inline void
list_push(Arena *arena, List<T> *list, T value)
{
    List_Node<T> *node = push_array(arena, List_Node<T>, 1);
    node->v = value;
    
    SLLQueuePush(list->first, list->last, node);
    list->count += 1;
}

template <typename T>
inline T*
list_push_new(Arena *arena, List<T> *list)
{
    List_Node<T> *node = push_array(arena, List_Node<T>, 1);
    
    SLLQueuePush(list->first, list->last, node);
    list->count += 1;
    
    return &node->v;
}
