#pragma once
#include "types.h"
#include "arena.h"
#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>

typedef void (*thread_task_fn)(void *args);

typedef struct Thread_Task Thread_Task;
struct Thread_Task
{
    thread_task_fn *fn;
    void           *args;
};

typedef struct Thread_Pool Thread_Pool;
struct Thread_Pool
{
    Arena          *arena;
    pthread_t      *threads;
    s32             n_threads;
    Thread_Task    *tasks;
    s32             cap;
    s32             head;
    s32             tail;
    s32             count;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    b32             stop;
};

Thread_Pool *thread_pool_create(s32 n_threads, s32 cap);
int          thread_pool_submit(Thread_Pool *pool, Thread_Task *fn, void *args);
void         thread_pool_destroy(Thread_Pool *pool);
