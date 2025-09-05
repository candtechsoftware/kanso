#include "thread.h"
#include "arena.h"
#include <pthread.h>

internal void *
worker_thread(void *arg)
{
    Thread_Pool *pool = (Thread_Pool *)arg;

    while (1)
    {
        while (pool->count == 0 && !pool->stop)
        {
            pthread_cond_wait(&pool->cond, &pool->lock);
        }

        if (pool->stop && pool->count == 0)
        {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        Thread_Task task = pool->tasks[pool->head];
        pool->head = (pool->head + 1) % pool->cap;
        pool->count--;

        pthread_mutex_unlock(&pool->lock);

        task.fn(task.args);
    }
    return 0;
}

Thread_Pool *
thread_pool_create(s32 n_threads, s32 cap)
{
    Arena       *arena = arena_alloc();
    Thread_Pool *pool = push_struct(arena, Thread_Pool);
    pool->n_threads = n_threads;
    pool->cap = cap;
    pool->tasks = push_array(arena, Thread_Task, cap);
    pool->threads = push_array(arena, pthread_t, n_threads);

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for (s32 i = 0; i < n_threads; i++)
    {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }

    return pool;
}
int
thread_pool_submit(Thread_Pool *pool, Thread_Task *fn, void *args)
{
    pthread_mutex_lock(&pool->lock);

    if (pool->count == pool->cap)
    {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    pool->tasks[pool->tail].fn = fn;
    pool->tasks[pool->tail].args = args;
    pool->tail = (pool->tail + 1) % pool->cap;
    pool->count++;

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);

    return 0;
}
void
thread_pool_destroy(Thread_Pool *pool)
{
    pthread_mutex_lock(&pool->lock);
    pool->stop = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->lock);

    for (s32 i = 0; i < pool->n_threads; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }
    arena_release(pool->arena);
}
