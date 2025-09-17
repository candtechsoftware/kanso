#pragma once

#include "../base/base_inc.h"
#include "font.h"
#include "../renderer/renderer_inc.h"

typedef enum Font_Renderer_Raster_Flags {
    Font_Renderer_Raster_Flag_Smooth = (1 << 0),
    Font_Renderer_Raster_Flag_Hinted = (1 << 1),
} Font_Renderer_Raster_Flags;

typedef struct Font_Renderer_Tag Font_Renderer_Tag;
struct Font_Renderer_Tag {
    u64 data[2];
};

static inline b32
font_tag_equal(Font_Renderer_Tag a, Font_Renderer_Tag b) {
    return a.data[0] == b.data[0] && a.data[1] == b.data[1];
}

typedef struct Font_Renderer_Piece Font_Renderer_Piece;
struct Font_Renderer_Piece {
    Renderer_Handle texture;
    Rng2_s16        subrect;
    Vec2_s16        offset;
    f32             advance;
    u16             decode_size;
};

typedef struct Font_Renderer_Piece_Array Font_Renderer_Piece_Array;
struct Font_Renderer_Piece_Array {
    Font_Renderer_Piece *pieces;
    u64                  count;
};

typedef struct Font_Renderer_Piece_Node Font_Renderer_Piece_Node;
struct Font_Renderer_Piece_Node {
    Font_Renderer_Piece_Node *next;
    Font_Renderer_Piece       v;
};

typedef struct Font_Renderer_Piece_List Font_Renderer_Piece_List;
struct Font_Renderer_Piece_List {
    Font_Renderer_Piece_Node *first;
    Font_Renderer_Piece_Node *last;
    u64                       count;
};

typedef struct Font_Renderer_Run Font_Renderer_Run;
struct Font_Renderer_Run {
    Font_Renderer_Piece *pieces;
    u64                  piece_count;
    Vec2_f32             dim;
    f32                  ascent;
    f32                  descent;
};

typedef struct Font_Renderer_Cache_Node Font_Renderer_Cache_Node;
struct Font_Renderer_Cache_Node {
    Font_Renderer_Cache_Node *hash_next;
    Font_Renderer_Tag         tag;
    Font_Renderer_Handle      handle;
    Font_Renderer_Metrics     metrics;
    String                    path;
};

typedef struct Font_Renderer_Cache_Slot Font_Renderer_Cache_Slot;
struct Font_Renderer_Cache_Slot {
    Font_Renderer_Cache_Node *first;
    Font_Renderer_Cache_Node *last;
};

typedef struct Font_Renderer_Raster_Cache_Info Font_Renderer_Raster_Cache_Info;
struct Font_Renderer_Raster_Cache_Info {
    Rng2_s16 subrect;
    Vec2_s16 raster_dim;
    s16      atlas_num;
    f32      advance;
};

typedef struct Font_Renderer_Hash_To_Info_Cache_Node Font_Renderer_Hash_To_Info_Cache_Node;
struct Font_Renderer_Hash_To_Info_Cache_Node {
    Font_Renderer_Hash_To_Info_Cache_Node *hash_next;
    Font_Renderer_Hash_To_Info_Cache_Node *hash_prev;
    u64                                    hash;
    Font_Renderer_Raster_Cache_Info        info;
};

typedef struct Font_Renderer_Hash_To_Info_Cache_Slot Font_Renderer_Hash_To_Info_Cache_Slot;
struct Font_Renderer_Hash_To_Info_Cache_Slot {
    Font_Renderer_Hash_To_Info_Cache_Node *first;
    Font_Renderer_Hash_To_Info_Cache_Node *last;
};

typedef struct Font_Renderer_Run_Cache_Node Font_Renderer_Run_Cache_Node;
struct Font_Renderer_Run_Cache_Node {
    Font_Renderer_Run_Cache_Node *next;
    String                        string;
    Font_Renderer_Run             run;
};

typedef struct Font_Renderer_Run_Cache_Slot Font_Renderer_Run_Cache_Slot;
struct Font_Renderer_Run_Cache_Slot {
    Font_Renderer_Run_Cache_Node *first;
    Font_Renderer_Run_Cache_Node *last;
};

typedef struct Font_Renderer_Style_Cache_Node Font_Renderer_Style_Cache_Node;
struct Font_Renderer_Style_Cache_Node {
    Font_Renderer_Style_Cache_Node        *hash_next;
    Font_Renderer_Style_Cache_Node        *hash_prev;
    u64                                    style_hash;
    f32                                    ascent;
    f32                                    descent;
    f32                                    column_width;
    Font_Renderer_Raster_Cache_Info       *utf8_class1_direct_map;
    u64                                    utf8_class1_direct_map_mask[4];
    u64                                    hash2info_slots_count;
    Font_Renderer_Hash_To_Info_Cache_Slot *hash2info_slots;
    u64                                    run_slots_count;
    Font_Renderer_Run_Cache_Slot          *run_slots;
    u64                                    run_slots_frame_index;
};

typedef struct Font_Renderer_Style_Cache_Slot Font_Renderer_Style_Cache_Slot;
struct Font_Renderer_Style_Cache_Slot {
    Font_Renderer_Style_Cache_Node *first;
    Font_Renderer_Style_Cache_Node *last;
};

typedef enum Font_Renderer_Atlas_Region_Flags {
    Font_Renderer_Atlas_Region_Flag_Taken = (1 << 0),
} Font_Renderer_Atlas_Region_Flags;

typedef enum Corner {
    Corner_00 = 0,
    Corner_01 = 1,
    Corner_10 = 2,
    Corner_11 = 3,
    Corner_COUNT = 4,
    Corner_Invalid = 0xFF,
} Corner;

typedef struct Font_Renderer_Atlas_Region_Node Font_Renderer_Atlas_Region_Node;
struct Font_Renderer_Atlas_Region_Node {
    Font_Renderer_Atlas_Region_Node *parent;
    Font_Renderer_Atlas_Region_Node *children[Corner_COUNT];
    Vec2_s16                         max_free_size[Corner_COUNT];
    Font_Renderer_Atlas_Region_Flags flags;
    u64                              num_allocated_descendants;
};

typedef struct Font_Renderer_Atlas Font_Renderer_Atlas;
struct Font_Renderer_Atlas {
    Font_Renderer_Atlas             *next;
    Font_Renderer_Atlas             *prev;
    Renderer_Handle                  texture;
    Vec2_s16                         root_dim;
    Font_Renderer_Atlas_Region_Node *root;
};

typedef struct Font_Renderer_Cache_State Font_Renderer_Cache_State;
struct Font_Renderer_Cache_State {
    Arena *permanent_arena;
    Arena *raster_arena;
    Arena *frame_arena;
    u64    frame_index;

    u64                       font_hash_table_size;
    Font_Renderer_Cache_Slot *font_hash_table;

    u64                             hash2style_slots_count;
    Font_Renderer_Style_Cache_Slot *hash2style_slots;

    Font_Renderer_Atlas *first_atlas;
    Font_Renderer_Atlas *last_atlas;
};

extern Font_Renderer_Cache_State *font_cache_state;

u128 font_cache_hash_from_string(String string);
u64  font_cache_little_hash_from_string(u64 seed, String string);

Font_Renderer_Tag
font_tag_zero(void);
Font_Renderer_Handle
font_handle_from_tag(Font_Renderer_Tag tag);
Font_Renderer_Metrics
font_metrics_from_tag(Font_Renderer_Tag tag);
Font_Renderer_Tag
font_tag_from_path(String path);
Font_Renderer_Tag
font_tag_from_data(String *data);
String
font_path_from_tag(Font_Renderer_Tag tag);

Rng2_s16
     font_atlas_region_alloc(Arena *arena, Font_Renderer_Atlas *atlas, Vec2_s16 needed_size);
void font_atlas_region_release(Font_Renderer_Atlas *atlas, Rng2_s16 region);

Font_Renderer_Piece_Array
font_piece_array_from_list(Arena *arena, Font_Renderer_Piece_List *list);
Font_Renderer_Piece_Array
font_piece_array_copy(Arena *arena, Font_Renderer_Piece_Array *src);

Font_Renderer_Style_Cache_Node *
font_style_from_tag_size_flags(Font_Renderer_Tag tag, f32 size, Font_Renderer_Raster_Flags flags);
Font_Renderer_Run
font_run_from_string(Font_Renderer_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, Font_Renderer_Raster_Flags flags, String string);

Vec2_f32
    font_dim_from_tag_size_string(Font_Renderer_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string);
f32 font_column_size_from_tag_size(Font_Renderer_Tag tag, f32 size);
u64 font_char_pos_from_tag_size_string_p(Font_Renderer_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string, f32 p);

Font_Renderer_Metrics
    font_metrics_from_tag_size(Font_Renderer_Tag tag, f32 size);
f32 font_line_height_from_metrics(Font_Renderer_Metrics *metrics);

void font_cache_init(void);
void font_cache_reset(void);
void font_cache_frame(void);
