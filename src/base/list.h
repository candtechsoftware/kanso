#ifndef LIST_H
#define LIST_H

#include "arena.h"

template <typename T>
struct List_Node
{
    List_Node<T>* next;
    T v;
};

template <typename T>
struct List
{
    List_Node<T>* first;
    List_Node<T>* last;
    u64 count;
};

template <typename T>
inline List<T>
list_make()
{
    return {0, 0, 0};
}

template <typename T>
inline void
list_push(Arena* arena, List<T>* list, T value)
{
    List_Node<T>* node = push_array(arena, List_Node<T>, 1);
    node->v = value;
    node->next = 0;

    if (list->last)
    {
        list->last->next = node;
    }
    else
    {
        list->first = node;
    }
    list->last = node;
    list->count += 1;
}

template <typename T>
inline T*
list_push_new(Arena* arena, List<T>* list)
{
    List_Node<T>* node = push_array(arena, List_Node<T>, 1);
    node->next = 0;

    if (list->last)
    {
        list->last->next = node;
    }
    else
    {
        list->first = node;
    }
    list->last = node;
    list->count += 1;

    return &node->v;
}

#endif // LIST_H