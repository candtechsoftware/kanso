#pragma once
#include "tctx.h"

thread_static TCTX *tctx_thread_local = 0;

internal TCTX *
tctx_alloc(void)
{
    Arena *arena = arena_alloc();
    TCTX  *tctx = push_struct(arena, TCTX);
    tctx->arenas[0] = arena;
    tctx->arenas[1] = arena_alloc();
    tctx->lane_ctx.lane_count = 1;
    return tctx;
}

internal void
tctx_release(TCTX *tctx)
{
    arena_release(tctx->arenas[1]);
    arena_release(tctx->arenas[0]);
}

internal void
tctx_select(TCTX *tctx)
{
    tctx_thread_local = tctx;
}

internal TCTX *
tctx_selected(void)
{
    return tctx_thread_local;
}

internal void
tctx_init_and_equip(TCTX *tctx)
{
    tctx->arenas[0] = arena_alloc();
    tctx->arenas[1] = arena_alloc();
    tctx->lane_ctx.lane_count = 1;
    tctx_thread_local = tctx;
}

internal TCTX *
tctx_get_equipped(void)
{
    return tctx_thread_local;
}

internal Arena *
tctx_get_scratch(Arena **conflicts, u64 count)
{
    Arena *result = 0;
    TCTX  *tctx = tctx_selected();
    if (tctx)
    {
        for (u64 arena_idx = 0; arena_idx < ArrayCount(tctx->arenas); arena_idx++)
        {
            Arena *arena = tctx->arenas[arena_idx];
            b32    is_conflicting = 0;
            for (u64 conflict_idx = 0; conflict_idx < count; conflict_idx++)
            {
                if (arena == conflicts[conflict_idx])
                {
                    is_conflicting = 1;
                    break;
                }
            }
            if (!is_conflicting)
            {
                result = arena;
                break;
            }
        }
    }
    return result;
}

internal Lane_Ctx
tctx_set_lane_ctx(Lane_Ctx lane_ctx)
{
    TCTX    *tctx = tctx_selected();
    Lane_Ctx restore = {0};
    if (tctx)
    {
        restore = tctx->lane_ctx;
        tctx->lane_ctx = lane_ctx;
    }
    return restore;
}

internal void
tctx_lane_barrier_wait(void)
{
    Prof_Begin("lane_barrier_wait");
    TCTX *tctx = tctx_selected();
    if (tctx)
    {
        os_barrier_wait(tctx->lane_ctx.barrier);
    }
    Prof_End();
}

internal void
tctx_set_thread_name(String name)
{
    TCTX *tctx = tctx_selected();
    if (tctx)
    {
        u64 size = Min(name.size, sizeof(tctx->thread_name) - 1);
        MemoryCopy(tctx->thread_name, name.data, size);
        tctx->thread_name[size] = 0;
        tctx->thread_name_size = size;
    }
}

internal String
tctx_get_thread_name(void)
{
    String result = {0};
    TCTX  *tctx = tctx_selected();
    if (tctx)
    {
        result.data = tctx->thread_name;
        result.size = tctx->thread_name_size;
    }
    return result;
}

internal void
tctx_write_srcloc(char *file_name, u64 line_number)
{
    TCTX *tctx = tctx_selected();
    if (tctx)
    {
        tctx->file_name = file_name;
        tctx->line_number = line_number;
    }
}

internal void
tctx_read_srcloc(char **file_name, u64 *line_number)
{
    TCTX *tctx = tctx_selected();
    if (tctx)
    {
        if (file_name)
            *file_name = tctx->file_name;
        if (line_number)
            *line_number = tctx->line_number;
    }
}