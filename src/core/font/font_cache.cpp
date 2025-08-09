#include "font_cache.h"
#include "base/arena.h"
#include "base/logger.h"
#include "base/string_core.h"
#include "font.h"
#include "renderer/renderer_core.h"
#include <cstring>
#include "stb_truetype_impl.h"

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

Font_Handle
font_handle_from_tag(Font_Tag tag)
{
    u64 slot_idx = tag.data[1] % font_cache_state->font_hash_table_size;
    Font_Cache_Node* existing_node = nullptr;

    for (Font_Cache_Node* n = font_cache_state->font_hash_table[slot_idx].first; n != nullptr; n = n->hash_next)
    {
        if (tag == n->tag)
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
        if (tag == n->tag)
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
        if (result == n->tag)
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
        if (result == n->tag)
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
        if (tag == n->tag)
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
    case Corner_00:
        result = {0, 0};
        break;
    case Corner_01:
        result = {0, 1};
        break;
    case Corner_10:
        result = {1, 0};
        break;
    case Corner_11:
        result = {1, 1};
        break;
    default:
        break;
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
    // Find the node that corresponds to this region
    Vec2<s16> region_p0 = region.min;
    Vec2<s16> region_sz = {(s16)(region.max.x - region.min.x), (s16)(region.max.y - region.min.y)};

    Vec2<s16> current_p0 = {0, 0};
    Vec2<s16> current_sz = atlas->root_dim;
    Font_Atlas_Region_Node* node = atlas->root;

    // Traverse down the tree to find the exact node
    while (node != nullptr && (current_sz.x != region_sz.x || current_sz.y != region_sz.y))
    {
        Vec2<s16> child_size = {(s16)(current_sz.x / 2), (s16)(current_sz.y / 2)};

        // Determine which quadrant the region is in
        bool found = false;
        for (Corner corner = (Corner)0; corner < Corner_COUNT; corner = (Corner)(corner + 1))
        {
            Vec2<s32> side_vertex = font_vertex_from_corner(corner);
            Vec2<s16> child_p0 = {
                (s16)(current_p0.x + side_vertex.x * child_size.x),
                (s16)(current_p0.y + side_vertex.y * child_size.y)};

            if (region_p0.x == child_p0.x && region_p0.y == child_p0.y)
            {
                if (node->children[corner] != nullptr)
                {
                    node = node->children[corner];
                    current_p0 = child_p0;
                    current_sz = child_size;
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            // Region not found
            return;
        }
    }

    // Release the node if found and it's taken
    if (node != nullptr && (node->flags & Font_Atlas_Region_Flag_Taken))
    {
        // Clear the taken flag
        node->flags = (Font_Atlas_Region_Flags)((u32)node->flags & ~Font_Atlas_Region_Flag_Taken);

        // Update parent allocated descendants count
        for (Font_Atlas_Region_Node* p = node->parent; p != nullptr; p = p->parent)
        {
            if (p->num_allocated_descendants > 0)
            {
                p->num_allocated_descendants -= 1;
            }
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

                    // If child is not taken, use its size
                    if (!(child->flags & Font_Atlas_Region_Flag_Taken))
                    {
                        // Calculate child's actual size based on parent
                        Vec2<s16> parent_sz = (p->parent != nullptr) ? Vec2<s16>{(s16)(current_sz.x * 2), (s16)(current_sz.y * 2)} : atlas->root_dim;
                        Vec2<s16> child_sz = {(s16)(parent_sz.x / 2), (s16)(parent_sz.y / 2)};

                        // If child has no allocated descendants, it's fully free
                        if (child->num_allocated_descendants == 0)
                        {
                            max_size = child_sz;
                        }
                        else
                        {
                            // Otherwise use the max of its children's free sizes
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
                }

                p->max_free_size[corner] = max_size;
            }
        }
    }
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

        // Calculate column width using average of common characters
        Font_Handle font_handle = font_handle_from_tag(tag);
        Font_Info font = font_from_handle(font_handle);

        if (font.info != nullptr)
        {
            // Calculate scale for the given size
            f32 scale = stbtt_ScaleForPixelHeight(font.info, size);

            // Calculate average width using common characters
            const char* sample_chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
            s32 sample_len = (s32)strlen(sample_chars);
            f32 total_width = 0.0f;
            s32 valid_chars = 0;

            for (s32 i = 0; i < sample_len; i++)
            {
                s32 advance_width, left_side_bearing;
                stbtt_GetCodepointHMetrics(font.info, sample_chars[i], &advance_width, &left_side_bearing);

                if (advance_width > 0)
                {
                    total_width += advance_width * scale;
                    valid_chars++;
                }
            }

            // Use average width if we have valid characters, otherwise use 0.6 * size as fallback
            if (valid_chars > 0)
            {
                node->column_width = total_width / valid_chars;
            }
            else
            {
                node->column_width = size * 0.6f;
            }
        }
        else
        {
            // Fallback if font info is not available
            node->column_width = size * 0.6f;
        }

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
        atlas->texture = tex;

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
    piece->texture = tex_handle;
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
    // Get font information
    Font_Handle font_handle = font_handle_from_tag(tag);
    Font_Info font = font_from_handle(font_handle);

    if (font.info == nullptr || string.size == 0)
    {
        return 0;
    }

    // Calculate scale for the given size
    f32 scale = stbtt_ScaleForPixelHeight(font.info, size);

    f32 current_x = base_align_px;
    u64 result_pos = 0;

    // Iterate through the string
    for (u32 i = 0; i < string.size;)
    {
        // Check if we've passed the target position
        if (current_x >= p)
        {
            // Check if we're closer to current or previous character
            if (i > 0)
            {
                f32 prev_x = current_x;

                // Get previous character width to find midpoint
                u32 prev_i = i;
                while (prev_i > 0 && (string.data[prev_i - 1] & 0xC0) == 0x80)
                {
                    prev_i--;
                }

                if (prev_i > 0)
                {
                    // Get codepoint at prev_i - 1
                    s32 prev_cp = string.data[prev_i - 1];
                    if (prev_cp >= 0x80)
                    {
                        // Handle UTF-8
                        s32 bytes_to_read = 0;
                        if ((prev_cp & 0xE0) == 0xC0)
                            bytes_to_read = 2;
                        else if ((prev_cp & 0xF0) == 0xE0)
                            bytes_to_read = 3;
                        else if ((prev_cp & 0xF8) == 0xF0)
                            bytes_to_read = 4;

                        if (bytes_to_read > 0 && prev_i - 1 + bytes_to_read <= string.size)
                        {
                            prev_cp = 0;
                            for (s32 j = 0; j < bytes_to_read; j++)
                            {
                                prev_cp = (prev_cp << 6) | (string.data[prev_i - 1 + j] & 0x3F);
                            }
                        }
                    }

                    s32 advance_width, left_side_bearing;
                    stbtt_GetCodepointHMetrics(font.info, prev_cp, &advance_width, &left_side_bearing);
                    f32 char_width = advance_width * scale;

                    // Find the midpoint of the previous character
                    f32 midpoint = prev_x - (char_width / 2.0f);

                    if (p < midpoint)
                    {
                        // Closer to the start of the previous character
                        result_pos = prev_i - 1;
                    }
                    else
                    {
                        // Closer to the current position
                        result_pos = i;
                    }
                }
                else
                {
                    result_pos = i;
                }
            }
            else
            {
                result_pos = 0;
            }
            return result_pos;
        }

        // Decode UTF-8 character
        s32 codepoint = string.data[i];
        s32 bytes_consumed = 1;

        if (codepoint >= 0x80)
        {
            // Multi-byte UTF-8
            if ((codepoint & 0xE0) == 0xC0 && i + 1 < string.size)
            {
                codepoint = ((codepoint & 0x1F) << 6) | (string.data[i + 1] & 0x3F);
                bytes_consumed = 2;
            }
            else if ((codepoint & 0xF0) == 0xE0 && i + 2 < string.size)
            {
                codepoint = ((codepoint & 0x0F) << 12) | ((string.data[i + 1] & 0x3F) << 6) | (string.data[i + 2] & 0x3F);
                bytes_consumed = 3;
            }
            else if ((codepoint & 0xF8) == 0xF0 && i + 3 < string.size)
            {
                codepoint = ((codepoint & 0x07) << 18) | ((string.data[i + 1] & 0x3F) << 12) |
                            ((string.data[i + 2] & 0x3F) << 6) | (string.data[i + 3] & 0x3F);
                bytes_consumed = 4;
            }
        }

        // Handle special characters
        if (codepoint == '\t')
        {
            // Tab handling
            if (tab_size_px > 0)
            {
                f32 next_tab_stop = ((s32)(current_x / tab_size_px) + 1) * tab_size_px;
                current_x = next_tab_stop;
            }
            else
            {
                // Default tab width (4 spaces)
                f32 space_width = font_column_size_from_tag_size(tag, size);
                current_x += space_width * 4;
            }
        }
        else if (codepoint == '\n' || codepoint == '\r')
        {
            // Newline - position is at the end of the line
            result_pos = i;
            return result_pos;
        }
        else
        {
            // Regular character
            s32 advance_width, left_side_bearing;
            stbtt_GetCodepointHMetrics(font.info, codepoint, &advance_width, &left_side_bearing);
            current_x += advance_width * scale;
        }

        i += bytes_consumed;
        result_pos = i;
    }

    // If we've reached the end without finding the position, return the end
    return string.size;
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
