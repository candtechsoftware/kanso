#pragma once

#include "types.h"
#include "util.h"
#include "arena.h"
#include "string_core.h"
#include "../os/os.h"

typedef struct Lane_Ctx Lane_Ctx;
struct Lane_Ctx {
    u64     lane_idx;
    u64     lane_count;
    Barrier barrier;
};

typedef struct TCTX TCTX;
struct TCTX {
    Arena *arenas[2];

    u8  thread_name[32];
    u64 thread_name_size;

    Lane_Ctx lane_ctx;

    char *file_name;
    u64   line_number;
};

internal TCTX *tctx_alloc(void);
internal void  tctx_release(TCTX *tctx);
internal void  tctx_select(TCTX *tctx);
internal TCTX *tctx_selected(void);

internal void  tctx_init_and_equip(TCTX *tctx);
internal TCTX *tctx_get_equipped(void);

internal Arena *tctx_get_scratch(Arena **conflicts, u64 count);

internal void   tctx_set_thread_name(String name);
internal String tctx_get_thread_name(void);

internal void tctx_write_srcloc(char *file_name, u64 line_number);
internal void tctx_read_srcloc(char **file_name, u64 *line_number);

#define tctx_write_this_srcloc()             tctx_write_srcloc(__FILE__, __LINE__)
#define tctx_scratch_begin(conflicts, count) scratch_begin(tctx_get_scratch((conflicts), (count)))
#define tctx_scratch_end(scratch)            scratch_end(&(scratch))

internal Lane_Ctx tctx_set_lane_ctx(Lane_Ctx lane_ctx);
internal void     tctx_lane_barrier_wait(void);

#define lane_idx()              (tctx_selected()->lane_ctx.lane_idx)
#define lane_count()            (tctx_selected()->lane_ctx.lane_count)
#define lane_from_task_idx(idx) ((idx) % lane_count())
#define lane_ctx(ctx)           tctx_set_lane_ctx((ctx))
#define lane_sync()             tctx_lane_barrier_wait()
#define lane_range(count)       rng1_u64(lane_idx() * (count) / lane_count(), \
                                   (lane_idx() + 1) * (count) / lane_count())
