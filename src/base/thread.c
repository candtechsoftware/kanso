#pragma once

#include "thread.h"
#include "arena.h"
#include "util.h"
#include <pthread.h>
#include <unistd.h>

internal void
thread_pool_worker_main(void *arg) {
    Thread_Pool_Worker *worker = (Thread_Pool_Worker *)arg;
    Thread_Pool        *pool = worker->pool;

    while (pool->is_live) {
        os_semaphore_wait(pool->exec_semaphore);

        if (!pool->is_live)
            break;

        while (pool->task_left > 0) {
            u64 task_index = ins_atomic_u64_dec_eval(&pool->task_left) - 1;

            if (task_index < pool->task_count) {
                pool->task_func(pool->task_arena->arenas[worker->id],
                                worker->id,
                                task_index,
                                pool->task_data);

                (void)ins_atomic_u64_inc_eval(&pool->task_done);
            }
        }

        os_semaphore_signal(pool->main_semaphore);
    }
}

internal Thread_Pool
thread_pool_alloc(Arena *arena, u32 worker_count, u32 max_worker_count, String name) {
    Thread_Pool pool = {0};

    if (worker_count == 0) {
        Sys_Info info = os_get_system_info();
        worker_count = info.num_threads;
    }

    worker_count = Min(worker_count, max_worker_count);

    pool.is_live = 1;
    pool.worker_count = worker_count;
    pool.workers = push_array(arena, Thread_Pool_Worker, worker_count);

    pool.exec_semaphore = os_semaphore_create(0);
    pool.task_semaphore = os_semaphore_create(1);
    pool.main_semaphore = os_semaphore_create(0);

    for (u32 i = 0; i < worker_count; i++) {
        pool.workers[i].id = i;
        pool.workers[i].pool = &pool;
        pool.workers[i].handle = os_thread_create(thread_pool_worker_main, &pool.workers[i]);
    }

    return pool;
}

internal void
thread_pool_release(Thread_Pool *pool) {
    pool->is_live = 0;

    for (u32 i = 0; i < pool->worker_count; i++) {
        os_semaphore_signal(pool->exec_semaphore);
    }

    for (u32 i = 0; i < pool->worker_count; i++) {
        os_thread_join(pool->workers[i].handle);
    }

    os_semaphore_destroy(pool->exec_semaphore);
    os_semaphore_destroy(pool->task_semaphore);
    os_semaphore_destroy(pool->main_semaphore);
}

internal Thread_Pool_Arena *
thread_pool_arena_alloc(Thread_Pool *pool) {
    Arena             *arena = arena_alloc();
    Thread_Pool_Arena *result = push_struct(arena, Thread_Pool_Arena);
    result->count = pool->worker_count;
    result->arenas = push_array(arena, Arena *, pool->worker_count);

    for (u32 i = 0; i < pool->worker_count; i++) {
        result->arenas[i] = arena_alloc();
    }

    return result;
}

internal void
thread_pool_arena_release(Thread_Pool_Arena **arena_ptr) {
    Thread_Pool_Arena *arena = *arena_ptr;
    if (arena) {
        for (u64 i = 0; i < arena->count; i++) {
            arena_release(arena->arenas[i]);
        }
        *arena_ptr = 0;
    }
}

internal Thread_Pool_Scratch
thread_pool_scratch_begin(Thread_Pool_Arena *arena) {
    Thread_Pool_Scratch result = {0};
    result.count = arena->count;
    Arena *temp_arena = arena_alloc();
    result.v = push_array(temp_arena, Scratch, arena->count);

    for (u64 i = 0; i < arena->count; i++) {
        result.v[i] = scratch_begin(arena->arenas[i]);
    }

    return result;
}

internal void
thread_pool_scratch_end(Thread_Pool_Scratch scratch) {
    for (u64 i = 0; i < scratch.count; i++) {
        scratch_end(&scratch.v[i]);
    }
}

internal void
thread_pool_for_parallel(Thread_Pool *pool, Thread_Pool_Arena *arena,
                         u64 task_count, Thread_Pool_Task_Func *task_func, void *task_data) {
    os_semaphore_wait(pool->task_semaphore);

    pool->task_arena = arena;
    pool->task_func = task_func;
    pool->task_data = task_data;
    pool->task_count = task_count;
    pool->task_done = 0;
    pool->task_left = (s64)task_count;

    for (u32 i = 0; i < pool->worker_count; i++) {
        os_semaphore_signal(pool->exec_semaphore);
    }

    for (u32 i = 0; i < pool->worker_count; i++) {
        os_semaphore_wait(pool->main_semaphore);
    }

    os_semaphore_signal(pool->task_semaphore);
}

internal Rng1_u64 *
thread_pool_divide_work(Arena *arena, u64 item_count, u32 worker_count) {
    Rng1_u64 *ranges = push_array(arena, Rng1_u64, worker_count);
    u64       items_per_worker = item_count / worker_count;
    u64       remainder = item_count % worker_count;

    u64 current = 0;
    for (u32 i = 0; i < worker_count; i++) {
        ranges[i].min = current;
        ranges[i].max = current + items_per_worker + (i < remainder ? 1 : 0);
        current = ranges[i].max;
    }

    return ranges;
}
