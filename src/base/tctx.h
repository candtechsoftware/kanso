#pragma once

#include "types.h"
#include "util.h"
#include "arena.h"
#include "string_core.h"

typedef struct TCTX TCTX;
struct TCTX
{
    Arena *arenas[2];

    u8  thread_name[32];
    u64 thread_name_size;

    char *file_name;
    u64   line_number;
};

internal void  tctx_init_and_equip(TCTX *tctx);
internal void  tctx_release(void);
internal TCTX *tctx_get_equipped(void);

internal Arena *tctx_get_scratch(Arena **conflicts, u64 count);

internal void   tctx_set_thread_name(String name);
internal String tctx_get_thread_name(void);

internal void tctx_write_srcloc(char *file_name, u64 line_number);
internal void tctx_read_srcloc(char **file_name, u64 *line_number);

#define tctx_write_this_srcloc()             tctx_write_srcloc(__FILE__, __LINE__)
#define tctx_scratch_begin(conflicts, count) scratch_begin(tctx_get_scratch((conflicts), (count)))
#define tctx_scratch_end(scratch)            scratch_end(&(scratch))

// Scratch macros use the existing arena scratch functions
// Just providing convenient access through thread context
