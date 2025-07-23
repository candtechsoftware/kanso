#include "arena.h"
#include "../os/os.h"
#include "util.h"
#include "profiler.h"

u64 arena_default_reserve_size = MB(64);
u64 arena_default_commit_size = KB(64);

Arena*
arena_alloc_(Arena_Params* params)
{
    ZoneScopedN("ArenaAlloc");
    u64 reserve_size = params->reserve_size;
    u64 commit_size = params->commit_size;

    // TODO(Alex): need to implement getting the large page size
    reserve_size = AlignPow2(reserve_size, os_get_sys_info().page_size);
    commit_size = AlignPow2(commit_size, os_get_sys_info().page_size);

    void* base = os_reserve(reserve_size);
    os_commit(base, commit_size);

    Arena* arena = (Arena*)base;
    arena->current = arena;
    arena->cmt_size = params->commit_size;
    arena->res_size = params->reserve_size;
    arena->base_pos = 0;
    arena->pos = ARENA_HEADER_SIZE;
    arena->cmt = commit_size;
    arena->res = reserve_size;
    arena->allocation_site_file = params->allocation_site_file;
    arena->allocation_site_line = params->allocation_site_line;

    return arena;
}

void
arena_release(Arena* arena)
{
    ZoneScopedN("ArenaRelease");
    for (Arena *n = arena->current, *prev = 0; n != 0; n = prev)
    {
        prev = n->prev;
        os_release(n, n->res);
    }
}

void*
arena_push(Arena* arena, u64 size, u64 align)
{
    ZoneScopedN("ArenaPush");
    
    Arena* curr = arena->current;
    u64 pos_pre = AlignPow2(curr->pos, align);
    u64 pos_pst = pos_pre + size;

    // Check if we have enough space in current block
    void* result = 0;

    // If current block doesn't have enough space, we would need chaining
    // For now, we'll work within the current block
    if (curr->res >= pos_pst)
    {
        // Commit new pages if needed
        if (curr->cmt < pos_pst)
        {
            u64 cmt_pst_aligned = pos_pst + curr->cmt_size - 1;
            cmt_pst_aligned -= cmt_pst_aligned % curr->cmt_size;
            u64 cmt_pst_clamped = ClampTop(cmt_pst_aligned, curr->res);
            u64 cmt_size = cmt_pst_clamped - curr->cmt;
            u8* cmt_ptr = (u8*)curr + curr->cmt;
            os_commit(cmt_ptr, cmt_size);
            curr->cmt = cmt_pst_clamped;
        }

        // Allocate if we have committed memory
        if (curr->cmt >= pos_pst)
        {
            result = (u8*)curr + pos_pre;
            curr->pos = pos_pst;
            AsanUnpoisonMemoryRegion(result, size);
        }
    }

    return result;
}
u64
arena_pos(Arena* arena)
{
    Arena* current = arena->current;
    u64 pos = current->base_pos + current->pos;
    return pos;
}

void
arena_pop_to(Arena* arena, u64 pos)
{
    ZoneScopedN("ArenaPopTo");
    u64 big_pos = ClampBot((u64)ARENA_HEADER_SIZE, pos);
    Arena* current = arena->current;

    // For now, we only support popping within the current block
    // Full implementation would handle chained blocks
    u64 new_pos = big_pos - current->base_pos;
    if (new_pos <= current->pos)
    {
        u64 freed_size = current->pos - new_pos;
        void* freed_ptr = (u8*)current + new_pos;
        AsanPoisonMemoryRegion(freed_ptr, freed_size);
        current->pos = new_pos;
    }
}

void
arena_clear(Arena* arena)
{
    // Fast path for clearing arena - just reset position without profiling overhead
    Arena* current = arena->current;
    current->pos = ARENA_HEADER_SIZE;
    
    // Only poison memory in debug builds with ASAN
#ifdef __has_feature
#if __has_feature(address_sanitizer)
    u64 freed_size = current->pos - ARENA_HEADER_SIZE;
    void* freed_ptr = (u8*)current + ARENA_HEADER_SIZE;
    AsanPoisonMemoryRegion(freed_ptr, freed_size);
#endif
#endif
}

void
arena_pop(Arena* arena, u64 amt)
{
    u64 pos_old = arena_pos(arena);
    u64 pos_new = pos_old;
    if (amt < pos_old)
    {
        pos_new = pos_old - amt;
    }
    arena_pop_to(arena, pos_new);
}

Scratch
scratch_begin(Arena* arena)
{
    u64 pos = arena_pos(arena);
    Scratch scratch = {arena, pos};
    return scratch;
}

void
scratch_end(Scratch* scratch)
{
    arena_pop_to(scratch->arena, scratch->pos);
}
