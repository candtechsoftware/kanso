#pragma once
#include "tctx.h"

thread_static TCTX *tl_tctx = 0;

internal void
tctx_init_and_equip(TCTX *tctx)
{
    tctx->arenas[0] = arena_alloc();
    tctx->arenas[1] = arena_alloc();
    tl_tctx = tctx;
}

internal void
tctx_release(void)
{
    if (tl_tctx)
    {
        arena_release(tl_tctx->arenas[0]);
        arena_release(tl_tctx->arenas[1]);
        tl_tctx = 0;
    }
}

internal TCTX *
tctx_get_equipped(void)
{
    return tl_tctx;
}

internal Arena *
tctx_get_scratch(Arena **conflicts, u64 count)
{
    Arena *result = 0;
    if (tl_tctx)
    {
        for (u64 arena_idx = 0; arena_idx < ArrayCount(tl_tctx->arenas); arena_idx++)
        {
            Arena *arena = tl_tctx->arenas[arena_idx];
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

internal void
tctx_set_thread_name(String name)
{
    if (tl_tctx)
    {
        u64 size = Min(name.size, sizeof(tl_tctx->thread_name) - 1);
        MemoryCopy(tl_tctx->thread_name, name.data, size);
        tl_tctx->thread_name[size] = 0;
        tl_tctx->thread_name_size = size;
    }
}

internal String
tctx_get_thread_name(void)
{
    String result = {0};
    if (tl_tctx)
    {
        result.data = tl_tctx->thread_name;
        result.size = tl_tctx->thread_name_size;
    }
    return result;
}

internal void
tctx_write_srcloc(char *file_name, u64 line_number)
{
    if (tl_tctx)
    {
        tl_tctx->file_name = file_name;
        tl_tctx->line_number = line_number;
    }
}

internal void
tctx_read_srcloc(char **file_name, u64 *line_number)
{
    if (tl_tctx)
    {
        if (file_name)
            *file_name = tl_tctx->file_name;
        if (line_number)
            *line_number = tl_tctx->line_number;
    }
}
