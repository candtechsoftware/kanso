#ifndef FONT_CACHE_H
#define FONT_CACHE_H

#include "base/base.h"
#include "base/types.h"
#include "base/array.h"
#include "base/list.h"
#include "font/font.h"

// Rasterization flags for font rendering
enum Font_Raster_Flags : u32 {
    Font_Raster_Flag_Smooth = (1 << 0),
    Font_Raster_Flag_Hinted = (1 << 1),
};

// Font tag for cache lookups
struct Font_Tag {
    u64 data[2];
};

// Glyph piece information for rendering
struct Font_Piece {
    // TODO(Alex): replace with renderer texture handle when available
    u64 texture;
    Rng2<s16> subrect;
    Vec2<s16> offset;
    f32 advance;
    u16 decode_size;
};

// Font piece array using Dynamic_Array
typedef Dynamic_Array<Font_Piece> Font_Piece_Array;

// List of font pieces for building arrays
typedef List<Font_Piece> Font_Piece_List;

// Run of glyphs representing rendered text
struct Font_Run {
    Font_Piece_Array pieces;
    Vec2<f32> dim;
    f32 ascent;
    f32 descent;
};

// Font hash node for path -> handle cache
struct Font_Cache_Node {
    Font_Cache_Node* hash_next;
    Font_Tag tag;
    Font_Handle handle;
    Font_Metrics metrics;
    String path;
};

// Hash slot for font cache
struct Font_Cache_Slot {
    Font_Cache_Node* first;
    Font_Cache_Node* last;
};

// Glyph raster cache info
struct Font_Raster_Cache_Info {
    Rng2<s16> subrect;
    Vec2<s16> raster_dim;
    s16 atlas_num;
    f32 advance;
};

// Hash node for glyph info cache
struct Font_Hash_To_Info_Cache_Node {
    Font_Hash_To_Info_Cache_Node* hash_next;
    Font_Hash_To_Info_Cache_Node* hash_prev;
    u64 hash;
    Font_Raster_Cache_Info info;
};

// Hash slot for glyph info cache
struct Font_Hash_To_Info_Cache_Slot {
    Font_Hash_To_Info_Cache_Node* first;
    Font_Hash_To_Info_Cache_Node* last;
};

// Run cache node for string -> run cache
struct Font_Run_Cache_Node {
    Font_Run_Cache_Node* next;
    String string;
    Font_Run run;
};

// Run cache slot
struct Font_Run_Cache_Slot {
    Font_Run_Cache_Node* first;
    Font_Run_Cache_Node* last;
};

// Style hash -> metrics cache node
struct Font_Style_Cache_Node {
    Font_Style_Cache_Node* hash_next;
    Font_Style_Cache_Node* hash_prev;
    u64 style_hash;
    f32 ascent;
    f32 descent;
    f32 column_width;
    Font_Raster_Cache_Info* utf8_class1_direct_map;
    u64 utf8_class1_direct_map_mask[4];
    u64 hash2info_slots_count;
    Font_Hash_To_Info_Cache_Slot* hash2info_slots;
    u64 run_slots_count;
    Font_Run_Cache_Slot* run_slots;
    u64 run_slots_frame_index;
};

// Style cache slot
struct Font_Style_Cache_Slot {
    Font_Style_Cache_Node* first;
    Font_Style_Cache_Node* last;
};

// Atlas region node flags
enum Font_Atlas_Region_Flags : u32 {
    Font_Atlas_Region_Flag_Taken = (1 << 0),
};

// Corner enum for atlas regions
enum Corner : u8 {
    Corner_00 = 0,
    Corner_01 = 1,
    Corner_10 = 2,
    Corner_11 = 3,
    Corner_COUNT = 4,
    Corner_Invalid = 0xFF,
};

// Atlas region node for texture allocation
struct Font_Atlas_Region_Node {
    Font_Atlas_Region_Node* parent;
    Font_Atlas_Region_Node* children[Corner_COUNT];
    Vec2<s16> max_free_size[Corner_COUNT];
    Font_Atlas_Region_Flags flags;
    u64 num_allocated_descendants;
};

// Font atlas for glyph storage
struct Font_Atlas {
    Font_Atlas* next;
    Font_Atlas* prev;
    // TODO(Alex): replace with renderer texture handle
    u64 texture;
    Vec2<s16> root_dim;
    Font_Atlas_Region_Node* root;
};

// Main font cache state
struct Font_Cache_State {
    Arena* permanent_arena;
    Arena* raster_arena;
    Arena* frame_arena;
    u64 frame_index;
    
    // Font table
    u64 font_hash_table_size;
    Font_Cache_Slot* font_hash_table;
    
    // Style -> raster cache table
    u64 hash2style_slots_count;
    Font_Style_Cache_Slot* hash2style_slots;
    
    // Atlas list
    Font_Atlas* first_atlas;
    Font_Atlas* last_atlas;
};

// Global state
extern Font_Cache_State* font_cache_state;

// Basic hash functions
u64 font_cache_hash_from_string(String string);
u64 font_cache_little_hash_from_string(u64 seed, String string);

// Font tag functions
Font_Tag font_tag_zero(void);
bool font_tag_match(Font_Tag a, Font_Tag b);
Font_Handle font_handle_from_tag(Font_Tag tag);
Font_Metrics font_metrics_from_tag(Font_Tag tag);
Font_Tag font_tag_from_path(String path);
Font_Tag font_tag_from_data(String* data);
String font_path_from_tag(Font_Tag tag);

// Atlas functions
Rng2<s16> font_atlas_region_alloc(Arena* arena, Font_Atlas* atlas, Vec2<s16> needed_size);
void font_atlas_region_release(Font_Atlas* atlas, Rng2<s16> region);

// Piece array functions
Font_Piece_Array font_piece_array_from_list(Arena* arena, Font_Piece_List* list);
Font_Piece_Array font_piece_array_copy(Arena* arena, Font_Piece_Array* src);

// Cache usage functions
Font_Style_Cache_Node* font_style_from_tag_size_flags(Font_Tag tag, f32 size, Font_Raster_Flags flags);
Font_Run font_run_from_string(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, Font_Raster_Flags flags, String string);

// Helper functions
Vec2<f32> font_dim_from_tag_size_string(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string);
f32 font_column_size_from_tag_size(Font_Tag tag, f32 size);
u64 font_char_pos_from_tag_size_string_p(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string, f32 p);

// Metrics functions
Font_Metrics font_metrics_from_tag_size(Font_Tag tag, f32 size);
f32 font_line_height_from_metrics(Font_Metrics* metrics);

// Main API
void font_cache_init(void);
void font_cache_reset(void);
void font_cache_frame(void);

#endif 

