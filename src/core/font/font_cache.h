#pragma once

#include "../../base/base_inc.h"
#include "font.h"
#include "../renderer/renderer_core.h"

enum Font_Raster_Flags : u8
{
    Font_Raster_Flag_Smooth = (1 << 0),
    Font_Raster_Flag_Hinted = (1 << 1),
};

struct Font_Tag
{
    u64 data[2];
};

inline bool
operator==(Font_Tag a, Font_Tag b)
{
    return a.data[0] == b.data[0] && a.data[1] == b.data[1];
}

struct Font_Piece
{
    Renderer_Handle texture;
    Rng2<s16>       subrect;
    Vec2<s16>       offset;
    f32             advance;
    u16             decode_size;
};

typedef Array<Font_Piece> Font_Piece_Array;

typedef List<Font_Piece> Font_Piece_List;

struct Font_Run
{
    Font_Piece_Array pieces;
    Vec2<f32>        dim;
    f32              ascent;
    f32              descent;
};

struct Font_Cache_Node
{
    Font_Cache_Node *hash_next;
    Font_Tag         tag;
    Font_Handle      handle;
    Font_Metrics     metrics;
    String           path;
};

struct Font_Cache_Slot
{
    Font_Cache_Node *first;
    Font_Cache_Node *last;
};

struct Font_Raster_Cache_Info
{
    Rng2<s16> subrect;
    Vec2<s16> raster_dim;
    s16       atlas_num;
    f32       advance;
};

struct Font_Hash_To_Info_Cache_Node
{
    Font_Hash_To_Info_Cache_Node *hash_next;
    Font_Hash_To_Info_Cache_Node *hash_prev;
    u64                           hash;
    Font_Raster_Cache_Info        info;
};

struct Font_Hash_To_Info_Cache_Slot
{
    Font_Hash_To_Info_Cache_Node *first;
    Font_Hash_To_Info_Cache_Node *last;
};

struct Font_Run_Cache_Node
{
    Font_Run_Cache_Node *next;
    String               string;
    Font_Run             run;
};

struct Font_Run_Cache_Slot
{
    Font_Run_Cache_Node *first;
    Font_Run_Cache_Node *last;
};

struct Font_Style_Cache_Node
{
    Font_Style_Cache_Node        *hash_next;
    Font_Style_Cache_Node        *hash_prev;
    u64                           style_hash;
    f32                           ascent;
    f32                           descent;
    f32                           column_width;
    Font_Raster_Cache_Info       *utf8_class1_direct_map;
    u64                           utf8_class1_direct_map_mask[4];
    u64                           hash2info_slots_count;
    Font_Hash_To_Info_Cache_Slot *hash2info_slots;
    u64                           run_slots_count;
    Font_Run_Cache_Slot          *run_slots;
    u64                           run_slots_frame_index;
};

struct Font_Style_Cache_Slot
{
    Font_Style_Cache_Node *first;
    Font_Style_Cache_Node *last;
};

enum Font_Atlas_Region_Flags : u8
{
    Font_Atlas_Region_Flag_Taken = (1 << 0),
};

enum Corner : u8
{
    Corner_00 = 0,
    Corner_01 = 1,
    Corner_10 = 2,
    Corner_11 = 3,
    Corner_COUNT = 4,
    Corner_Invalid = 0xFF,
};

struct Font_Atlas_Region_Node
{
    Font_Atlas_Region_Node *parent;
    Font_Atlas_Region_Node *children[Corner_COUNT];
    Vec2<s16>               max_free_size[Corner_COUNT];
    Font_Atlas_Region_Flags flags;
    u64                     num_allocated_descendants;
};

struct Font_Atlas
{
    Font_Atlas             *next;
    Font_Atlas             *prev;
    Renderer_Handle         texture;
    Vec2<s16>               root_dim;
    Font_Atlas_Region_Node *root;
};

struct Font_Cache_State
{
    Arena *permanent_arena;
    Arena *raster_arena;
    Arena *frame_arena;
    u64    frame_index;

    u64              font_hash_table_size;
    Font_Cache_Slot *font_hash_table;

    u64                    hash2style_slots_count;
    Font_Style_Cache_Slot *hash2style_slots;

    Font_Atlas *first_atlas;
    Font_Atlas *last_atlas;
};

extern Font_Cache_State *font_cache_state;

u128
font_cache_hash_from_string(String string);
u64
font_cache_little_hash_from_string(u64 seed, String string);

Font_Tag
font_tag_zero(void);
Font_Handle
font_handle_from_tag(Font_Tag tag);
Font_Metrics
font_metrics_from_tag(Font_Tag tag);
Font_Tag
font_tag_from_path(String path);
Font_Tag
font_tag_from_data(String *data);
String
font_path_from_tag(Font_Tag tag);

Rng2<s16>
font_atlas_region_alloc(Arena *arena, Font_Atlas *atlas, Vec2<s16> needed_size);
void
font_atlas_region_release(Font_Atlas *atlas, Rng2<s16> region);

Font_Piece_Array
font_piece_array_from_list(Arena *arena, Font_Piece_List *list);
Font_Piece_Array
font_piece_array_copy(Arena *arena, Font_Piece_Array *src);

Font_Style_Cache_Node *
font_style_from_tag_size_flags(Font_Tag tag, f32 size, Font_Raster_Flags flags);
Font_Run
font_run_from_string(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, Font_Raster_Flags flags, String string);

Vec2<f32>
font_dim_from_tag_size_string(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string);
f32
font_column_size_from_tag_size(Font_Tag tag, f32 size);
u64
font_char_pos_from_tag_size_string_p(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string, f32 p);

Font_Metrics
font_metrics_from_tag_size(Font_Tag tag, f32 size);
f32
font_line_height_from_metrics(Font_Metrics *metrics);

void
font_cache_init(void);
void
font_cache_reset(void);
void
font_cache_frame(void);
