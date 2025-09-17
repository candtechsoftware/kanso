#pragma once
#include "base_inc.h"

typedef struct Work_Queue_Node Work_Queue_Node;
struct Work_Queue_Node {
    Work_Queue_Node *next;
    void            *v;
    u64              size;
};

typedef struct Work_Queue_List Work_Queue_List;
struct Work_Queue_List {
    Work_Queue_Node *first;
    Work_Queue_Node *last;
    u64              node_count;
    Mutex            m;
};

internal void  worker_push_batch(Arena *arena, Work_Queue_List *wq, void **data, u64 count, u64 size);
internal void  worker_push(Arena *arena, Work_Queue_List *wq, void *data, u64 size);
internal void *worker_pop(Arena *arena, Work_Queue_List *wq);
internal u64   worker_pop_batch(Arena *arena, Work_Queue_List *wq, void **data, u64 count);
