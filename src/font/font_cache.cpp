#include "font_cache.h"
#include "base/arena.h"
#include "base/logger.h"
#include "base/string_core.h"
#include "renderer/renderer_core.h"
#include <cstring>

// Global font cache state
Font_Cache_State* font_cache_state = nullptr;

// Simple hash function for strings (FNV-1a hash)
u64 
font_cache_hash_from_string(String string)
{
    u64 hash = 14695981039346656037ULL;
    for (u32 i = 0; i < string.size; i++)
    {
        hash ^= (u64)string.data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

u64 
font_cache_little_hash_from_string(u64 seed, String string)
{
    u64 hash = seed;
    for (u32 i = 0; i < string.size; i++)
    {
        hash ^= (u64)string.data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Font tag functions
Font_Tag 
font_tag_zero(void)
{
    Font_Tag result = {0};
    return result;
}

bool 
font_tag_match(Font_Tag a, Font_Tag b)
{
    return a.data[0] == b.data[0] && a.data[1] == b.data[1];
}

Font_Handle 
font_handle_from_tag(Font_Tag tag)
{
    u64 slot_idx = tag.data[1] % font_cache_state->font_hash_table_size;
    Font_Cache_Node* existing_node = nullptr;
    
    for (Font_Cache_Node* n = font_cache_state->font_hash_table[slot_idx].first; n != nullptr; n = n->hash_next)
    {
        if (font_tag_match(tag, n->tag))
        {
            existing_node = n;
            break;
        }
    }
    
    Font_Handle result = font_handle_zero();
    if (existing_node != nullptr)
    {
        result = existing_node->handle;
    }
    
    return result;
}

Font_Metrics 
font_metrics_from_tag(Font_Tag tag)
{
    u64 slot_idx = tag.data[1] % font_cache_state->font_hash_table_size;
    Font_Cache_Node* existing_node = nullptr;
    
    for (Font_Cache_Node* n = font_cache_state->font_hash_table[slot_idx].first; n != nullptr; n = n->hash_next)
    {
        if (font_tag_match(tag, n->tag))
        {
            existing_node = n;
            break;
        }
    }
    
    Font_Metrics result = {0};
    if (existing_node != nullptr)
    {
        result = existing_node->metrics;
    }
    
    return result;
}

Font_Tag 
font_tag_from_path(String path)
{
    // Produce tag from hash of path
    Font_Tag result = {0};
    u64 hash = font_cache_hash_from_string(path);
    result.data[0] = hash;
    result.data[1] = hash | (1ULL << 63); // Set high bit to indicate path-based tag
    
    // Tag -> slot index
    u64 slot_idx = result.data[1] % font_cache_state->font_hash_table_size;
    
    // Check for existing node
    Font_Cache_Node* existing_node = nullptr;
    for (Font_Cache_Node* n = font_cache_state->font_hash_table[slot_idx].first; n != nullptr; n = n->hash_next)
    {
        if (font_tag_match(result, n->tag))
        {
            existing_node = n;
            break;
        }
    }
    
    // Allocate new node if needed
    if (existing_node == nullptr)
    {
        Font_Handle handle = font_open(path);
        if (handle != font_handle_zero())
        {
            Font_Cache_Slot* slot = &font_cache_state->font_hash_table[slot_idx];
            existing_node = push_struct(font_cache_state->permanent_arena, Font_Cache_Node);
            existing_node->tag = result;
            existing_node->handle = handle;
            existing_node->metrics = font_metrics_from_font(handle);
            existing_node->path = push_string_copy(font_cache_state->permanent_arena, path);
            existing_node->hash_next = nullptr;
            
            // Add to linked list
            if (slot->last)
            {
                slot->last->hash_next = existing_node;
            }
            else
            {
                slot->first = existing_node;
            }
            slot->last = existing_node;
        }
        else
        {
            // Invalid font, return zero tag
            result = font_tag_zero();
        }
    }
    
    return result;
}

Font_Tag 
font_tag_from_data(String* data)
{
    // Produce tag from hash of data pointer
    Font_Tag result = {0};
    String ptr_str = {(u8*)&data, sizeof(String*)};
    u64 hash = font_cache_hash_from_string(ptr_str);
    result.data[0] = hash;
    result.data[1] = hash & ~(1ULL << 63); // Clear high bit to indicate data-based tag
    
    // Tag -> slot index
    u64 slot_idx = result.data[1] % font_cache_state->font_hash_table_size;
    
    // Check for existing node
    Font_Cache_Node* existing_node = nullptr;
    for (Font_Cache_Node* n = font_cache_state->font_hash_table[slot_idx].first; n != nullptr; n = n->hash_next)
    {
        if (font_tag_match(result, n->tag))
        {
            existing_node = n;
            break;
        }
    }
    
    // Allocate new node if needed
    if (existing_node == nullptr)
    {
        Font_Handle handle = font_open_from_data(data);
        Font_Cache_Slot* slot = &font_cache_state->font_hash_table[slot_idx];
        Font_Cache_Node* new_node = push_struct(font_cache_state->permanent_arena, Font_Cache_Node);
        new_node->tag = result;
        new_node->handle = handle;
        new_node->metrics = font_metrics_from_font(handle);
        new_node->path = to_string("");
        new_node->hash_next = nullptr;
        
        // Add to linked list
        if (slot->last)
        {
            slot->last->hash_next = new_node;
        }
        else
        {
            slot->first = new_node;
        }
        slot->last = new_node;
    }
    
    return result;
}

String 
font_path_from_tag(Font_Tag tag)
{
    u64 slot_idx = tag.data[1] % font_cache_state->font_hash_table_size;
    
    Font_Cache_Node* existing_node = nullptr;
    for (Font_Cache_Node* n = font_cache_state->font_hash_table[slot_idx].first; n != nullptr; n = n->hash_next)
    {
        if (font_tag_match(tag, n->tag))
        {
            existing_node = n;
            break;
        }
    }
    
    String result = {0};
    if (existing_node != nullptr)
    {
        result = existing_node->path;
    }
    
    return result;
}

// Helper function to get vertex from corner
static Vec2<s32> 
font_vertex_from_corner(Corner corner)
{
    Vec2<s32> result = {0, 0};
    switch (corner)
    {
        case Corner_00: result = {0, 0}; break;
        case Corner_01: result = {0, 1}; break;
        case Corner_10: result = {1, 0}; break;
        case Corner_11: result = {1, 1}; break;
        default: break;
    }
    return result;
}

// Atlas region allocation
Rng2<s16> 
font_atlas_region_alloc(Arena* arena, Font_Atlas* atlas, Vec2<s16> needed_size)
{
    // Find node with best-fit size
    Vec2<s16> region_p0 = {0, 0};
    Vec2<s16> region_sz = {0, 0};
    Font_Atlas_Region_Node* node = nullptr;
    
    Vec2<s16> n_supported_size = atlas->root_dim;
    for (Font_Atlas_Region_Node* n = atlas->root; n != nullptr;)
    {
        Font_Atlas_Region_Node* next = nullptr;
        
        // Check if taken
        if (n->flags & Font_Atlas_Region_Flag_Taken)
        {
            break;
        }
        
        // Check if can be allocated
        bool n_can_be_allocated = (n->num_allocated_descendants == 0);
        
        if (n_can_be_allocated)
        {
            region_sz = n_supported_size;
        }
        
        // Calculate child size
        Vec2<s16> child_size = {(s16)(n_supported_size.x / 2), (s16)(n_supported_size.y / 2)};
        
        // Find best child
        Font_Atlas_Region_Node* best_child = nullptr;
        if (child_size.x >= needed_size.x && child_size.y >= needed_size.y)
        {
            for (Corner corner = (Corner)0; corner < Corner_COUNT; corner = (Corner)(corner + 1))
            {
                if (n->children[corner] == nullptr)
                {
                    n->children[corner] = push_struct_zero(arena, Font_Atlas_Region_Node);
                    n->children[corner]->parent = n;
                    n->children[corner]->max_free_size[Corner_00] = 
                    n->children[corner]->max_free_size[Corner_01] = 
                    n->children[corner]->max_free_size[Corner_10] = 
                    n->children[corner]->max_free_size[Corner_11] = {(s16)(child_size.x / 2), (s16)(child_size.y / 2)};
                }
                
                if (n->max_free_size[corner].x >= needed_size.x && 
                    n->max_free_size[corner].y >= needed_size.y)
                {
                    best_child = n->children[corner];
                    Vec2<s32> side_vertex = font_vertex_from_corner(corner);
                    region_p0.x += side_vertex.x * child_size.x;
                    region_p0.y += side_vertex.y * child_size.y;
                    break;
                }
            }
        }
        
        if (best_child && 
            best_child->max_free_size[Corner_00].x >= needed_size.x &&
            best_child->max_free_size[Corner_00].y >= needed_size.y)
        {
            node = n;
            n_supported_size = child_size;
            next = best_child;
        }
        
        n = next;
    }
    
    // Allocate the node
    if (node != nullptr)
    {
        node->flags = (Font_Atlas_Region_Flags)((u32)node->flags | Font_Atlas_Region_Flag_Taken);
        
        // Update parent allocated descendants
        for (Font_Atlas_Region_Node* p = node->parent; p != nullptr; p = p->parent)
        {
            p->num_allocated_descendants += 1;
        }
        
        // Update max free sizes in parents
        for (Font_Atlas_Region_Node* p = node->parent; p != nullptr; p = p->parent)
        {
            // Recalculate max free size for each corner
            for (Corner corner = (Corner)0; corner < Corner_COUNT; corner = (Corner)(corner + 1))
            {
                Vec2<s16> max_size = {0, 0};
                
                if (p->children[corner] != nullptr)
                {
                    Font_Atlas_Region_Node* child = p->children[corner];
                    
                    // If child is not taken, use its max free sizes
                    if (!(child->flags & Font_Atlas_Region_Flag_Taken))
                    {
                        // Find the maximum free size among all corners of the child
                        for (Corner child_corner = (Corner)0; child_corner < Corner_COUNT; child_corner = (Corner)(child_corner + 1))
                        {
                            if (child->max_free_size[child_corner].x > max_size.x)
                            {
                                max_size.x = child->max_free_size[child_corner].x;
                            }
                            if (child->max_free_size[child_corner].y > max_size.y)
                            {
                                max_size.y = child->max_free_size[child_corner].y;
                            }
                        }
                    }
                }
                
                p->max_free_size[corner] = max_size;
            }
        }
    }
    
    // Build result
    Rng2<s16> result = {{region_p0.x, region_p0.y}, {(s16)(region_p0.x + region_sz.x), (s16)(region_p0.y + region_sz.y)}};
    return result;
}

void 
font_atlas_region_release(Font_Atlas* atlas, Rng2<s16> region)
{
    // TODO: Implement region release
}

// Piece array functions
Font_Piece_Array 
font_piece_array_from_list(Arena* arena, Font_Piece_List* list)
{
    Font_Piece_Array result = dynamic_array_make<Font_Piece>();
    dynamic_array_reserve(arena, &result, list->count);
    
    for (List_Node<Font_Piece>* node = list->first; node != nullptr; node = node->next)
    {
        dynamic_array_push(arena, &result, node->v);
    }
    
    return result;
}

Font_Piece_Array 
font_piece_array_copy(Arena* arena, Font_Piece_Array* src)
{
    Font_Piece_Array result = dynamic_array_make<Font_Piece>();
    dynamic_array_reserve(arena, &result, src->size);
    
    for (u64 i = 0; i < src->size; i++)
    {
        dynamic_array_push(arena, &result, src->data[i]);
    }
    
    return result;
}

// Cache usage functions
Font_Style_Cache_Node* 
font_style_from_tag_size_flags(Font_Tag tag, f32 size, Font_Raster_Flags flags)
{
    // Create style hash from tag, size, and flags
    u64 style_hash = tag.data[0] ^ tag.data[1];
    style_hash ^= *(u64*)&size;
    style_hash ^= (u64)flags;
    
    // Find or create style cache node
    u64 slot_idx = style_hash % font_cache_state->hash2style_slots_count;
    Font_Style_Cache_Slot* slot = &font_cache_state->hash2style_slots[slot_idx];
    
    Font_Style_Cache_Node* node = nullptr;
    for (Font_Style_Cache_Node* n = slot->first; n != nullptr; n = n->hash_next)
    {
        if (n->style_hash == style_hash)
        {
            node = n;
            break;
        }
    }
    
    if (node == nullptr)
    {
        // Create new node
        node = push_struct_zero(font_cache_state->permanent_arena, Font_Style_Cache_Node);
        node->style_hash = style_hash;
        
        // Get font metrics
        Font_Metrics metrics = font_metrics_from_tag(tag);
        node->ascent = metrics.accent * size;
        node->descent = metrics.descent * size;
        
        // TODO: Calculate column width
        node->column_width = size * 0.6f; // Rough estimate
        
        // Initialize hash tables
        node->hash2info_slots_count = 256;
        node->hash2info_slots = push_array_zero(font_cache_state->permanent_arena, Font_Hash_To_Info_Cache_Slot, node->hash2info_slots_count);
        
        node->run_slots_count = 64;
        node->run_slots = push_array_zero(font_cache_state->permanent_arena, Font_Run_Cache_Slot, node->run_slots_count);
        
        // Add to linked list
        if (slot->last)
        {
            slot->last->hash_next = node;
            node->hash_prev = slot->last;
        }
        else
        {
            slot->first = node;
        }
        slot->last = node;
    }
    
    return node;
}

Font_Run 
font_run_from_string(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, Font_Raster_Flags flags, String string)
{
    Font_Run result = {{0}};
    
    // Get style cache node
    Font_Style_Cache_Node* style_node = font_style_from_tag_size_flags(tag, size, flags);
    if (style_node == nullptr)
    {
        return result;
    }
    
    // Check run cache
    u64 string_hash = font_cache_hash_from_string(string);
    u64 run_slot_idx = string_hash % style_node->run_slots_count;
    Font_Run_Cache_Slot* run_slot = &style_node->run_slots[run_slot_idx];
    
    // Look for cached run
    for (Font_Run_Cache_Node* n = run_slot->first; n != nullptr; n = n->next)
    {
        if (string_match(n->string, string))
        {
            return n->run;
        }
    }
    
    // Build new run
    Font_Handle font_handle = font_handle_from_tag(tag);
    if (font_handle == font_handle_zero())
    {
        return result;
    }
    
    // Rasterize the string
    Font_Raster_Result raster = font_raster(font_cache_state->frame_arena, font_handle, size, string);
    if (!raster.valid)
    {
        return result;
    }
    
    // Create atlas texture if needed
    if (font_cache_state->first_atlas == nullptr)
    {
        Font_Atlas* atlas = push_struct_zero(font_cache_state->permanent_arena, Font_Atlas);
        atlas->root_dim = {2048, 2048};
        atlas->root = push_struct_zero(font_cache_state->permanent_arena, Font_Atlas_Region_Node);
        atlas->root->max_free_size[Corner_00] = 
        atlas->root->max_free_size[Corner_01] = 
        atlas->root->max_free_size[Corner_10] = 
        atlas->root->max_free_size[Corner_11] = {1024, 1024};
        
        // Create actual texture
        u8* empty_data = push_array_zero(font_cache_state->permanent_arena, u8, 2048 * 2048 * 4);
        Renderer_Handle tex = renderer_tex_2d_alloc(Renderer_Resource_Kind_Dynamic,
                                                   {2048, 2048},
                                                   Renderer_Tex_2D_Format_RGBA8,
                                                   empty_data);
        atlas->texture = tex.u64s[0];
        
        font_cache_state->first_atlas = font_cache_state->last_atlas = atlas;
    }
    
    // For now, create a separate texture for each text run
    Renderer_Handle tex_handle = renderer_tex_2d_alloc(Renderer_Resource_Kind_Dynamic,
                                                      {(f32)raster.atlas_dim.x, (f32)raster.atlas_dim.y},
                                                      Renderer_Tex_2D_Format_RGBA8,
                                                      raster.atlas_data);
    
    // Build font piece
    result.pieces = dynamic_array_make<Font_Piece>();
    Font_Piece* piece = dynamic_array_push_new(font_cache_state->frame_arena, &result.pieces);
    piece->texture = tex_handle.u64s[0];
    piece->subrect = {{0, 0}, {raster.atlas_dim.x, raster.atlas_dim.y}};
    piece->offset = {0, 0};
    piece->advance = (f32)raster.atlas_dim.x;
    piece->decode_size = (u16)size;
    
    result.dim = {(f32)raster.atlas_dim.x, (f32)raster.atlas_dim.y};
    result.ascent = style_node->ascent;
    result.descent = style_node->descent;
    
    // Cache the run
    Font_Run_Cache_Node* cache_node = push_struct(font_cache_state->permanent_arena, Font_Run_Cache_Node);
    cache_node->string = push_string_copy(font_cache_state->permanent_arena, string);
    cache_node->run = result;
    cache_node->run.pieces = font_piece_array_copy(font_cache_state->permanent_arena, &result.pieces);
    cache_node->next = nullptr;
    
    if (run_slot->last)
    {
        run_slot->last->next = cache_node;
    }
    else
    {
        run_slot->first = cache_node;
    }
    run_slot->last = cache_node;
    
    return result;
}

// Helper functions
Vec2<f32> 
font_dim_from_tag_size_string(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string)
{
    Font_Run run = font_run_from_string(tag, size, base_align_px, tab_size_px, Font_Raster_Flag_Smooth, string);
    return run.dim;
}

f32 
font_column_size_from_tag_size(Font_Tag tag, f32 size)
{
    Font_Style_Cache_Node* style_node = font_style_from_tag_size_flags(tag, size, Font_Raster_Flag_Smooth);
    if (style_node != nullptr)
    {
        return style_node->column_width;
    }
    return size * 0.6f;
}

u64 
font_char_pos_from_tag_size_string_p(Font_Tag tag, f32 size, f32 base_align_px, f32 tab_size_px, String string, f32 p)
{
    // TODO: Implement character position from pixel position
    return 0;
}

// Metrics functions
Font_Metrics 
font_metrics_from_tag_size(Font_Tag tag, f32 size)
{
    Font_Metrics base_metrics = font_metrics_from_tag(tag);
    Font_Metrics result = base_metrics;
    result.accent *= size;
    result.descent *= size;
    result.line_gap *= size;
    return result;
}

f32 
font_line_height_from_metrics(Font_Metrics* metrics)
{
    return metrics->accent - metrics->descent + metrics->line_gap;
}

// Main API
void 
font_cache_init(void)
{
    Arena* arena = arena_alloc();
    font_cache_state = push_struct(arena, Font_Cache_State);
    font_cache_state->permanent_arena = arena;
    font_cache_state->raster_arena = arena_alloc();
    font_cache_state->frame_arena = arena_alloc();
    font_cache_state->frame_index = 0;
    
    // Initialize hash tables
    font_cache_state->font_hash_table_size = 256;
    font_cache_state->font_hash_table = push_array_zero(arena, Font_Cache_Slot, font_cache_state->font_hash_table_size);
    
    font_cache_state->hash2style_slots_count = 256;
    font_cache_state->hash2style_slots = push_array_zero(arena, Font_Style_Cache_Slot, font_cache_state->hash2style_slots_count);
    
    log_info("Font cache initialized");
}

void 
font_cache_reset(void)
{
    arena_clear(font_cache_state->raster_arena);
    arena_clear(font_cache_state->frame_arena);
    font_cache_state->frame_index = 0;
}

void 
font_cache_frame(void)
{
    arena_clear(font_cache_state->frame_arena);
    font_cache_state->frame_index++;
}