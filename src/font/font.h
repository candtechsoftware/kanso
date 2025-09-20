#pragma once

#include "../base/base_inc.h"

typedef union Font_Handle Font_Handle;
union Font_Handle {
    u64   u64s[2];
    u32   u32s[4];
    u16   u16s[8];
    void *ptr;
};

typedef struct Font_Renderer Font_Renderer;
struct Font_Renderer {
    Font_Handle handle;
};

typedef Font_Handle Font_Renderer_Handle;

static inline b32
font_handle_equal(Font_Renderer_Handle a, Font_Renderer_Handle b) {
    return a.u64s[0] == b.u64s[0] && a.u64s[1] == b.u64s[1];
}

static inline Font_Renderer_Handle
font_handle_zero(void) {
    Font_Renderer_Handle res = {0};
    return res;
}

static inline Font_Handle
font_handle_from_ptr(void *ptr) {
    Font_Handle result = {0};
    result.ptr = ptr;
    return result;
}

typedef struct Font_Renderer_Metrics Font_Renderer_Metrics;
struct Font_Renderer_Metrics {
    // TODO(Alex) do we need to record height if we are doing the SDF??
    f32 accent;
    f32 descent;
    f32 line_gap;
};

typedef struct Font_Renderer_Raster_Result Font_Renderer_Raster_Result;
struct Font_Renderer_Raster_Result {
    u8      *atlas_data;
    Vec2_s16 atlas_dim;
    b32      valid;
};

void                  font_init(void);
Font_Renderer_Handle  font_open(String path);
Font_Renderer_Handle  font_open_from_data(String *data);
void                  font_close(Font_Renderer_Handle font);
Font_Renderer_Metrics font_metrics_from_font(Font_Renderer_Handle font);

Font_Renderer_Raster_Result
font_raster(Arena *arena, Font_Renderer_Handle handle, f32 size, String string);

Font_Renderer
font_from_handle(Font_Renderer_Handle handle);

Font_Renderer_Handle
font_to_handle(Font_Renderer font);
