#ifndef FONT_H
#define FONT_H

#include "base/base.h"
#include "base/types.h"
#include <stb_truetype.h>


struct Font {
    stbtt_fontinfo *info;
};
struct Font_Handle
{
    u64 data[2];
};

static inline bool
operator==(Font_Handle a, Font_Handle b)
{
    return a.data[0] == b.data[0] && a.data[1] == b.data[1];
}

static inline Font_Handle
font_handle_zero(void)
{
    Font_Handle res = {0};
    return res;
}

struct Font_Metrics
{
    // TODO(Alex) do we need to record height if we are doing the SDF??
    f32 accent;
    f32 descent;
    f32 line_gap;
};

struct Font_Raster_Result
{
    u8* atlas_data;
    Vec2<s16> atlas_dim;
    bool valid;
};



// TOOD(Alex) are we going to need this or should I just pass the 
// arena to each function that needs it? 
struct Font_State
{
    Arena* arena;
};

void
font_init(void);

Font_Handle
font_open(String path);

Font_Handle
font_open_from_data(String* data);

void
font_close(Font_Handle font);

Font_Metrics
font_metrics_from_font(Font_Handle font);

Font_Raster_Result
font_raster(Arena* arena, Font_Handle handle, f32 size, String string);

// TODO(Alex) we want to no use the stbtt types here and have this
// be under an specific impl when we want to use other providers or
// when we want to have our own.
Font
font_from_handle(Font_Handle handle);

Font_Handle
font_to_handle(Font font);

#endif
