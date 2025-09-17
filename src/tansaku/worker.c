#include "worker.h"
#include "types.h"

internal void worker_push(Arena *arena, Work_Queue_List *wq, void *data, u64 size) {
    Work_Queue_Node *node = push_array_no_zero(arena, Work_Queue_Node, 1);
    node->v = data;
    node->size = size;
    os_mutex_lock(wq->m);
    SLLQueuePush(wq->first, wq->last, node);
    wq->node_count += 1;
    os_mutex_unlock(wq->m);
}
internal void worker_push_batch(Arena *arena, Work_Queue_List *wq, void **data, u64 count, u64 size) {
    os_mutex_lock(wq->m);

    for (u64 i = 0; i < count; i++) {
        void *owned = push_array(arena, void, 1);
        void *item = data[i];
        MemoryCopy(owned, item, size);
        Work_Queue_Node *node = push_array_no_zero(arena, Work_Queue_Node, 1);
        node->v = owned;
        node->size = size;
        SLLQueuePush(wq->first, wq->last, node);
        wq->node_count += 1;
    }

    os_mutex_unlock(wq->m);
}
internal void *pop(Arena *arena, Work_Queue_List *wq) {
    os_mutex_lock(wq->m);
    if (wq->node_count == 0) return nullptr;
    Work_Queue_Node *ret = wq->first; 
    SLLQueuePop(wq->first, wq->last);
    os_mutex_unlock(wq->m);
    return ret;

}

internal u64 pop_batch(Arena *arena, Work_Queue_List *wq, Work_Queue_Node **nodes, u64 count) {
    os_mutex_lock(wq->m);
    u64 ret_count = Min(count, wq->node_count);
    if (ret_count == 0) return 0;

    u64 i = ret_count; 
    while (i > 0) {
        Work_Queue_Node *node = wq->first;
        nodes[ret_count - i] = node; 
        i -= 1; 
    } 

    os_mutex_unlock(wq->m);
    return ret_count; 
} 
