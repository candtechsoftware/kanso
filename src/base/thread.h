#pragma once
#include "types.h"
#include "arena.h"
#include "math_core.h"
#include "../os/os_inc.h"
#include "profile.h"
#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>

#define THREAD_POOL_TASK_FUNC(name) void name(Arena *arena, u64 worker_id, u64 task_id, void *raw_task);
typedef THREAD_POOL_TASK_FUNC(Thread_Pool_Task_Func);

typedef struct Thread_Pool_Arena Thread_Pool_Arena;
struct Thread_Pool_Arena
{
    u64     count;
    Arena **arenas;
};

typedef struct Thread_Pool_Scratch Thread_Pool_Scratch;
struct Thread_Pool_Scratch
{
    u64      count;
    Scratch *v;
};

typedef struct Thread_Pool_Worker Thread_Pool_Worker;
struct Thread_Pool_Worker
{
    u64                 id;
    struct Thread_Pool *pool;
    OS_Handle           handle;
};

typedef struct Thread_Pool Thread_Pool;
struct Thread_Pool
{
    b32       is_live;
    Semaphore exec_semaphore;
    Semaphore task_semaphore;
    Semaphore main_semaphore;

    u32                 worker_count;
    Thread_Pool_Worker *workers;

    Thread_Pool_Arena     *task_arena;
    Thread_Pool_Task_Func *task_func;
    void                  *task_data;
    u64                    task_count;
    u64                    task_done;
    s64                    task_left;
};

internal Thread_Pool         thread_pool_alloc(Arena *arena, u32 worker_count, u32 max_worker_count, String name);
internal void                thread_pool_release(Thread_Pool *pool);
internal Thread_Pool_Arena  *thread_pool_arena_alloc(Thread_Pool *pool);
internal void                thread_pool_arena_release(Thread_Pool_Arena **arena_ptr);
internal Thread_Pool_Scratch thread_pool_scratch_begin(Thread_Pool_Arena *arena);
internal void                thread_pool_scratch_end(Thread_Pool_Scratch scratch);
#define thread_pool_parrallel_prof(pool, arena, task_count, task_data, zone_name) \
    Prof_Begin(zone_name);                                                        \
    thread_pool_parrallel(pool, arena, task_count, task_func, task_data);         \
    Prof_End();
internal void      thread_pool_for_parallel(Thread_Pool *pool, Thread_Pool_Arena *arena, u64 task_count, Thread_Pool_Task_Func *task_func, void *task_data);
internal Rng1_u64 *thread_pool_divide_work(Arena *arena, u64 item_count, u32 worker_count);
