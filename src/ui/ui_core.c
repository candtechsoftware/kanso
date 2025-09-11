#pragma once
#include "ui_core.h"
#include <stdarg.h>
#include <stdio.h>

internal String
str_pushf(Arena *arena, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[4096];
    int  len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len < 0)
        len = 0;
    if (len >= sizeof(buffer))
        len = sizeof(buffer) - 1;

    String result;
    result.size = len;
    result.data = push_array(arena, u8, len + 1);
    MemoryCopy(result.data, buffer, len);
    result.data[len] = 0;
    return result;
}

thread_static UI_State *ui_state = 0;

internal UI_State *
ui_state_alloc(void)
{
    Arena    *arena = arena_alloc();
    UI_State *state = push_struct(arena, UI_State);
    state->arena = arena;
    state->build_arenas[0] = arena_alloc();
    state->build_arenas[1] = arena_alloc();
    state->box_table_size = 4096;
    state->box_table = push_array(arena, UI_BoxHashSlot, state->box_table_size);
    ui_state = state;
    return state;
}

internal void
ui_state_release(UI_State *state)
{
    if (state)
    {
        arena_release(state->build_arenas[0]);
        arena_release(state->build_arenas[1]);
        arena_release(state->arena);
    }
}

internal void
ui_begin_frame(UI_State *ui, f32 dt)
{
    ui_state = ui;
    ui->build_index = (ui->build_index + 1) % 2;
    arena_clear(ui->build_arenas[ui->build_index]);
    ui->frame_index += 1;
    ui->animation_dt = dt;

    // Clear the box hash table for the new frame
    MemoryZero(ui->box_table, sizeof(UI_BoxHashSlot) * ui->box_table_size);

    ui->parent_stack = 0;
    ui->pref_width = 0;
    ui->pref_height = 0;
    ui->corner_radius = 0;
    ui->text_color = 0;
    ui->background_color = 0;
    ui->border_color = 0;
    ui->border_thickness = 0;

    UI_Box *root = ui_build_box_from_string(0, str_lit("###root"));
    root->semantic_size[Axis2_X] = ui_size_px(1920, 1);
    root->semantic_size[Axis2_Y] = ui_size_px(1080, 1);
    ui_push_parent(root);
    ui->root = root;
}

internal void
ui_end_frame(UI_State *ui)
{
    ui_pop_parent();
    ui_layout(ui->root);
    ui_animate(ui->root, ui->animation_dt);
}

internal void
ui_push_parent(UI_Box *box)
{
    UI_State        *ui = ui_state;
    Arena           *arena = ui->build_arenas[ui->build_index];
    UI_StackBoxNode *node = push_struct(arena, UI_StackBoxNode);
    node->box = box;
    node->next = ui->parent_stack;
    ui->parent_stack = node;
}

internal UI_Box *
ui_pop_parent(void)
{
    UI_State *ui = ui_state;
    UI_Box   *result = 0;
    if (ui->parent_stack)
    {
        result = ui->parent_stack->box;
        ui->parent_stack = ui->parent_stack->next;
    }
    return result;
}

internal UI_Box *
ui_top_parent(void)
{
    UI_State *ui = ui_state;
    UI_Box   *result = 0;
    if (ui->parent_stack)
    {
        result = ui->parent_stack->box;
    }
    return result;
}

#define UI_POP_IMPL(name, member)          \
    internal void                          \
    ui_pop_##name(void)                    \
    {                                      \
        UI_State *ui = ui_state;           \
        if (ui->member)                    \
        {                                  \
            ui->member = ui->member->next; \
        }                                  \
    }

#define UI_PUSH_SIZE_IMPL(name, member)                                \
    internal void                                                      \
    ui_push_##name(UI_Size value)                                      \
    {                                                                  \
        UI_State         *ui = ui_state;                               \
        Arena            *arena = ui->build_arenas[ui->build_index];   \
        UI_StackSizeNode *node = push_struct(arena, UI_StackSizeNode); \
        node->value = value;                                           \
        node->next = ui->member;                                       \
        ui->member = node;                                             \
    }

UI_PUSH_SIZE_IMPL(pref_width, pref_width)
UI_PUSH_SIZE_IMPL(pref_height, pref_height)
#define UI_PUSH_F32_IMPL(name, member)                               \
    internal void                                                    \
    ui_push_##name(f32 value)                                        \
    {                                                                \
        UI_State        *ui = ui_state;                              \
        Arena           *arena = ui->build_arenas[ui->build_index];  \
        UI_StackF32Node *node = push_struct(arena, UI_StackF32Node); \
        node->value = value;                                         \
        node->next = ui->member;                                     \
        ui->member = node;                                           \
    }

UI_PUSH_F32_IMPL(corner_radius, corner_radius)
#define UI_PUSH_VEC4_IMPL(name, member)                                \
    internal void                                                      \
    ui_push_##name(Vec4_f32 value)                                     \
    {                                                                  \
        UI_State         *ui = ui_state;                               \
        Arena            *arena = ui->build_arenas[ui->build_index];   \
        UI_StackVec4Node *node = push_struct(arena, UI_StackVec4Node); \
        node->value = value;                                           \
        node->next = ui->member;                                       \
        ui->member = node;                                             \
    }

UI_PUSH_VEC4_IMPL(text_color, text_color)
UI_PUSH_VEC4_IMPL(background_color, background_color)
UI_PUSH_VEC4_IMPL(border_color, border_color)
UI_PUSH_F32_IMPL(border_thickness, border_thickness)

UI_POP_IMPL(pref_width, pref_width)
UI_POP_IMPL(pref_height, pref_height)
UI_POP_IMPL(corner_radius, corner_radius)
UI_POP_IMPL(text_color, text_color)
UI_POP_IMPL(background_color, background_color)
UI_POP_IMPL(border_color, border_color)
UI_POP_IMPL(border_thickness, border_thickness)

internal UI_Size
ui_size_px(f32 pixels, f32 strictness)
{
    UI_Size result = {0};
    result.kind = UI_SizeKind_Pixels;
    result.value = pixels;
    result.strictness = strictness;
    return result;
}

internal UI_Size
ui_size_text(f32 padding, f32 strictness)
{
    UI_Size result = {0};
    result.kind = UI_SizeKind_TextContent;
    result.value = padding;
    result.strictness = strictness;
    return result;
}

internal UI_Size
ui_size_pct(f32 percent, f32 strictness)
{
    UI_Size result = {0};
    result.kind = UI_SizeKind_PercentOfParent;
    result.value = percent;
    result.strictness = strictness;
    return result;
}

internal UI_Size
ui_size_children(f32 padding, f32 strictness)
{
    UI_Size result = {0};
    result.kind = UI_SizeKind_ChildrenSum;
    result.value = padding;
    result.strictness = strictness;
    return result;
}

internal UI_Key
ui_key_from_string(String string)
{
    u64 hash = 14695981039346656037ull;
    for (u64 i = 0; i < string.size; i++)
    {
        hash = hash ^ string.data[i];
        hash = hash * 1099511628211ull;
    }
    return hash;
}

internal UI_Key
ui_key_from_stringf(char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Key result = ui_key_from_string(string);
    scratch_end(&scratch);
    return result;
}

internal UI_Box *
ui_box_from_key(UI_Key key)
{
    UI_State *ui = ui_state;
    UI_Box   *result = 0;
    if (ui && ui->box_table)
    {
        u64             slot_idx = key % ui->box_table_size;
        UI_BoxHashSlot *slot = &ui->box_table[slot_idx];
        for (UI_Box *box = slot->first; box != 0; box = box->hash_next)
        {
            if (box && box->key == key)
            {
                result = box;
                break;
            }
        }
    }
    return result;
}

internal UI_Box *
ui_build_box_from_string(UI_BoxFlags flags, String string)
{
    UI_State *ui = ui_state;
    Arena    *arena = ui->build_arenas[ui->build_index];

    UI_Key key = ui_key_from_string(string);

    // Always allocate a new box each frame from the frame arena
    UI_Box *box = push_struct(arena, UI_Box);
    b32     first_frame = (ui->frame_index == 1);

    // Add to hash table
    u64             slot_idx = key % ui->box_table_size;
    UI_BoxHashSlot *slot = &ui->box_table[slot_idx];
    if (slot->last)
    {
        slot->last->hash_next = box;
        box->hash_prev = slot->last;
        slot->last = box;
    }
    else
    {
        slot->first = slot->last = box;
    }

    box->first = box->last = 0;
    box->next = box->prev = 0;
    box->child_count = 0;
    box->key = key;
    box->last_frame_touched_index = ui->frame_index;
    box->flags = flags;
    box->string = str_push_copy(arena, string);

    if (ui->pref_width)
        box->semantic_size[Axis2_X] = ui->pref_width->value;
    else
        box->semantic_size[Axis2_X] = ui_size_px(0, 0);

    if (ui->pref_height)
        box->semantic_size[Axis2_Y] = ui->pref_height->value;
    else
        box->semantic_size[Axis2_Y] = ui_size_px(0, 0);

    if (ui->corner_radius)
        box->corner_radius = ui->corner_radius->value;
    else
        box->corner_radius = 0;

    if (ui->text_color)
        box->text_color = ui->text_color->value;
    else
    {
        UI_ModernDesign *design = ui_get_modern_design();
        box->text_color = design->colors.text_primary;
    }

    if (ui->background_color)
        box->background_color = ui->background_color->value;
    else
    {
        UI_ModernDesign *design = ui_get_modern_design();
        box->background_color = design->colors.bg_primary;
    }

    if (ui->border_color)
        box->border_color = ui->border_color->value;
    else
    {
        UI_ModernDesign *design = ui_get_modern_design();
        box->border_color = design->colors.border_subtle;
    }

    if (ui->border_thickness)
        box->border_thickness = ui->border_thickness->value;
    else
        box->border_thickness = 1;

    UI_Box *parent = ui_top_parent();
    if (parent)
    {
        box->parent = parent;
        if (parent->last)
        {
            parent->last->next = box;
            box->prev = parent->last;
            parent->last = box;
        }
        else
        {
            parent->first = parent->last = box;
        }
        parent->child_count += 1;
    }

    if (first_frame)
    {
        box->hot_t = 0;
        box->active_t = 0;
        box->disabled_t = 0;
    }

    return box;
}

internal UI_Box *
ui_build_box_from_stringf(UI_BoxFlags flags, char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Box *result = ui_build_box_from_string(flags, string);
    scratch_end(&scratch);
    return result;
}

internal void
ui_box_equip_display_string(UI_Box *box, String string)
{
    UI_State *ui = ui_state;
    Arena    *arena = ui->build_arenas[ui->build_index];
    box->display_string = str_push_copy(arena, string);
    box->flags |= UI_BoxFlag_HasDisplayString;
}

internal void
ui_box_equip_custom_draw(UI_Box *box, void (*custom_draw)(UI_Box *box, void *user_data), void *user_data)
{
    box->custom_draw = custom_draw;
    box->user_data = user_data;
}

internal void
ui_box_equip_font(UI_Box *box, Font_Renderer_Tag font)
{
    box->font = font;
}

internal UI_Signal
ui_signal_from_box(UI_Box *box)
{
    UI_State *ui = ui_state;
    UI_Signal result = {0};
    result.box = box;
    
    if (!ui || !box) {
        return result;
    }
    
    result.mouse = ui->mouse_pos;

    Vec2_f32 mouse_rel = {ui->mouse_pos.x - box->rect.min.x, ui->mouse_pos.y - box->rect.min.y};
    result.mouse_over = (mouse_rel.x >= 0 && mouse_rel.x < (box->rect.max.x - box->rect.min.x) &&
                         mouse_rel.y >= 0 && mouse_rel.y < (box->rect.max.y - box->rect.min.y));

    if (result.mouse_over && !(box->flags & UI_BoxFlag_Disabled))
    {
        result.hovering = 1;
        if (ui->hot_box_key == 0)
        {
            ui->hot_box_key = box->key;
        }
    }

    if (ui->hot_box_key == box->key && ui->mouse_pressed && !(box->flags & UI_BoxFlag_Disabled))
    {
        ui->active_box_key = box->key;
        result.pressed = 1;
    }

    if (ui->active_box_key == box->key)
    {
        if (ui->mouse_released)
        {
            ui->active_box_key = 0;
            result.released = 1;
            if (result.mouse_over)
            {
                result.clicked = 1;
            }
        }
        else
        {
            result.dragging = 1;
            result.drag_delta.x = ui->mouse_pos.x - ui->drag_start_mouse.x;
            result.drag_delta.y = ui->mouse_pos.y - ui->drag_start_mouse.y;
        }
    }

    return result;
}

internal void
ui_layout_axis(UI_Box *root, Axis2 axis)
{
    if (root->child_count == 0)
        return;

    // Calculate parent size accounting for padding
    f32 parent_size = (axis == Axis2_X) ? (root->rect.max.x - root->rect.min.x) : (root->rect.max.y - root->rect.min.y);
    f32 padding_start = (axis == Axis2_X) ? root->padding_left : root->padding_top;
    f32 padding_end = (axis == Axis2_X) ? root->padding_right : root->padding_bottom;
    f32 available_size = parent_size - padding_start - padding_end;

    f32 total_fixed_size = 0;
    f32 total_weight = 0;
    u64 flex_count = 0;

    for (UI_Box *child = root->first; child != 0; child = child->next)
    {
        UI_Size size = child->semantic_size[axis];
        switch (size.kind)
        {
        case UI_SizeKind_Pixels:
        {
            total_fixed_size += size.value;
        }
        break;
        case UI_SizeKind_TextContent:
        {
            f32 text_size = 100;
            total_fixed_size += text_size + size.value * 2;
        }
        break;
        case UI_SizeKind_PercentOfParent:
        {
            total_fixed_size += available_size * size.value;
        }
        break;
        case UI_SizeKind_ChildrenSum:
        {
            f32 children_size = 0;
            for (UI_Box *grandchild = child->first; grandchild != 0; grandchild = grandchild->next)
            {
                UI_Size grandchild_size = grandchild->semantic_size[axis];
                if (grandchild_size.kind == UI_SizeKind_Pixels)
                {
                    children_size += grandchild_size.value;
                }
            }
            total_fixed_size += children_size + size.value * 2;
        }
        break;
        default:
        {
            flex_count += 1;
            total_weight += (1.0f - size.strictness);
        }
        break;
        }
    }

    f32 flex_space = available_size - total_fixed_size;
    if (flex_space < 0)
        flex_space = 0;

    f32 position = ((axis == Axis2_X) ? root->rect.min.x : root->rect.min.y) + padding_start;
    

    for (UI_Box *child = root->first; child != 0; child = child->next)
    {
        UI_Size size = child->semantic_size[axis];
        f32     child_size = 0;

        switch (size.kind)
        {
        case UI_SizeKind_Pixels:
        {
            child_size = size.value;
        }
        break;
        case UI_SizeKind_TextContent:
        {
            child_size = 100 + size.value * 2;
        }
        break;
        case UI_SizeKind_PercentOfParent:
        {
            child_size = available_size * size.value;
        }
        break;
        case UI_SizeKind_ChildrenSum:
        {
            f32 children_size = 0;
            for (UI_Box *grandchild = child->first; grandchild != 0; grandchild = grandchild->next)
            {
                UI_Size grandchild_size = grandchild->semantic_size[axis];
                if (grandchild_size.kind == UI_SizeKind_Pixels)
                {
                    children_size += grandchild_size.value;
                }
            }
            child_size = children_size + size.value * 2;
        }
        break;
        default:
        {
            if (total_weight > 0)
            {
                child_size = flex_space * ((1.0f - size.strictness) / total_weight);
            }
        }
        break;
        }

        if (axis == Axis2_X)
        {
            child->rect.min.x = position;
            child->rect.max.x = position + child_size;
            
            child->rect.min.y = root->rect.min.y + root->padding_top;
            child->rect.max.y = root->rect.max.y - root->padding_bottom;
        }
        else
        {
            child->rect.min.y = position;
            child->rect.max.y = position + child_size;
            child->rect.min.x = root->rect.min.x + root->padding_left;
            child->rect.max.x = root->rect.max.x - root->padding_right;
            
        }

        position += child_size;
        
    }
}

internal void
ui_layout_recursive(UI_Box *box, Axis2 axis)
{
    ui_layout_axis(box, axis);

    for (UI_Box *child = box->first; child != 0; child = child->next)
    {
        Axis2 child_axis = (child->flags & UI_BoxFlag_LayoutAxisX) ? Axis2_X : Axis2_Y;
        ui_layout_recursive(child, child_axis);
    }
}

internal void
ui_layout(UI_Box *root)
{
    ui_layout_recursive(root, Axis2_Y);
}

internal void
ui_animate_recursive(UI_Box *box, f32 dt)
{
    UI_State *ui = ui_state;

    f32 hot_target = (ui->hot_box_key == box->key) ? 1.0f : 0.0f;
    f32 active_target = (ui->active_box_key == box->key) ? 1.0f : 0.0f;
    f32 disabled_target = (box->flags & UI_BoxFlag_Disabled) ? 1.0f : 0.0f;

    f32 rate = 1.0f - powf(2.0f, -10.0f * dt);

    if (box->flags & UI_BoxFlag_HotAnimation)
    {
        box->hot_t += (hot_target - box->hot_t) * rate;
    }
    else
    {
        box->hot_t = hot_target;
    }

    if (box->flags & UI_BoxFlag_ActiveAnimation)
    {
        box->active_t += (active_target - box->active_t) * rate;
    }
    else
    {
        box->active_t = active_target;
    }

    box->disabled_t += (disabled_target - box->disabled_t) * rate;

    for (UI_Box *child = box->first; child != 0; child = child->next)
    {
        ui_animate_recursive(child, dt);
    }
}

internal void
ui_animate(UI_Box *root, f32 dt)
{
    ui_animate_recursive(root, dt);
}

internal void
ui_draw_recursive(UI_Box *box, Font_Renderer_Tag font)
{
    if (box->flags & UI_BoxFlag_DrawBackground)
    {
        Vec4_f32 color = box->background_color;
        color.w *= (1.0f - box->disabled_t * 0.5f);

        if (box->flags & UI_BoxFlag_HotAnimation)
        {
            color.x += box->hot_t * 0.1f;
            color.y += box->hot_t * 0.1f;
            color.z += box->hot_t * 0.1f;
        }

        if (box->flags & UI_BoxFlag_ActiveAnimation)
        {
            color.x += box->active_t * 0.1f;
            color.y += box->active_t * 0.1f;
            color.z += box->active_t * 0.1f;
        }

        draw_rect(box->rect, color, box->corner_radius, box->border_thickness, 1.0f);
    }

    if (box->flags & UI_BoxFlag_DrawBorder)
    {
        Vec4_f32 border_color = box->border_color;
        border_color.w *= (1.0f - box->disabled_t * 0.5f);

        Rng2_f32 border_rect = box->rect;
        f32      thickness = box->border_thickness;

        draw_rect((Rng2_f32){{{border_rect.min.x, border_rect.min.y}},
                             {{border_rect.max.x, border_rect.min.y + thickness}}},
                  border_color, 0, 0, 1.0f);
        draw_rect((Rng2_f32){{{border_rect.min.x, border_rect.max.y - thickness}},
                             {{border_rect.max.x, border_rect.max.y}}},
                  border_color, 0, 0, 1.0f);
        draw_rect((Rng2_f32){{{border_rect.min.x, border_rect.min.y}},
                             {{border_rect.min.x + thickness, border_rect.max.y}}},
                  border_color, 0, 0, 1.0f);
        draw_rect((Rng2_f32){{{border_rect.max.x - thickness, border_rect.min.y}},
                             {{border_rect.max.x, border_rect.max.y}}},
                  border_color, 0, 0, 1.0f);
    }

    if ((box->flags & UI_BoxFlag_DrawText) && (box->flags & UI_BoxFlag_HasDisplayString))
    {
        f32 box_height = box->rect.max.y - box->rect.min.y;
        f32 box_width = box->rect.max.x - box->rect.min.x;
        
        // Debug: Print text positions for sidebar items
        if (str_match(box->display_string, str_lit("AirDrop")) ||
            str_match(box->display_string, str_lit("Recents")) ||
            str_match(box->display_string, str_lit("Applications")) ||
            str_match(box->display_string, str_lit("Desktop"))) {
            printf("Drawing '%.*s' at rect x[%.1f, %.1f]\n", 
                   (int)box->display_string.size, box->display_string.data,
                   box->rect.min.x, box->rect.max.x);
        }
        
        // Use box's font if set, otherwise use default font
        Font_Renderer_Tag box_font = (box->font.data[0] != 0 || box->font.data[1] != 0) ? box->font : font;
        
        // Check if this is an icon box (has custom font)
        b32 is_icon = (box->font.data[0] != 0 || box->font.data[1] != 0);
        
        f32 font_size = is_icon ? 16.0f : 13.0f; // Bigger font for icons
        // Center text vertically in the box using proper baseline alignment
        // For most fonts, we need to offset by about 1/4 of the font size from center
        f32 baseline_offset = font_size * 0.25f;
        f32 text_y = box->rect.min.y + (box_height / 2.0f) - baseline_offset;
        
        // Center icons in their box, align text to left of box
        f32 text_x = is_icon ? 
            box->rect.min.x + (box_width / 2.0f) - 4.0f :  // Center icons
            box->rect.min.x;                               // Text starts at box edge
            
        Vec2_f32 text_pos = {{roundf(text_x), roundf(text_y)}};
        
        draw_text(text_pos, box->display_string, box_font, font_size, box->text_color);
    }

    if (box->custom_draw)
    {
        box->custom_draw(box, box->user_data);
    }

    for (UI_Box *child = box->first; child != 0; child = child->next)
    {
        ui_draw_recursive(child, font);
    }
}

internal void
ui_draw(UI_Box *root, Font_Renderer_Tag font)
{
    ui_draw_recursive(root, font);
}

internal void
ui_push_event(UI_EventList *list, UI_Event *event)
{
    if (list->last)
    {
        list->last->next = event;
        event->prev = list->last;
        list->last = event;
    }
    else
    {
        list->first = list->last = event;
    }
    list->count += 1;
}

internal b32
ui_get_next_event(UI_EventList *list, UI_Event **event)
{
    b32 result = 0;
    if (list->first)
    {
        *event = list->first;
        list->first = list->first->next;
        if (list->first)
        {
            list->first->prev = 0;
        }
        else
        {
            list->last = 0;
        }
        list->count -= 1;
        result = 1;
    }
    return result;
}

// ========================================
// Layout helper functions
// ========================================

internal void 
ui_set_padding(UI_Box *box, f32 left, f32 right, f32 top, f32 bottom)
{
    box->padding_left = left;
    box->padding_right = right;
    box->padding_top = top;
    box->padding_bottom = bottom;
}

internal void 
ui_set_padding_all(UI_Box *box, f32 padding)
{
    ui_set_padding(box, padding, padding, padding, padding);
}

internal void 
ui_set_margin(UI_Box *box, f32 left, f32 right, f32 top, f32 bottom)
{
    box->margin_left = left;
    box->margin_right = right;
    box->margin_top = top;
    box->margin_bottom = bottom;
}

internal void 
ui_set_margin_all(UI_Box *box, f32 margin)
{
    ui_set_margin(box, margin, margin, margin, margin);
}

// ========================================
// MODERN DESIGN SYSTEM IMPLEMENTATION
// ========================================

static UI_ModernDesign g_modern_design = {0};
static b32 g_modern_design_initialized = 0;

internal UI_ModernDesign *
ui_get_modern_design(void)
{
    if (!g_modern_design_initialized)
    {
        // Finder-like colors (more authentic macOS look)
        g_modern_design.colors.bg_primary      = (Vec4_f32){0.094f, 0.094f, 0.094f, 1.0f};  // #181818 - darker like Finder
        g_modern_design.colors.bg_secondary    = (Vec4_f32){0.125f, 0.125f, 0.125f, 1.0f};  // #202020 - sidebar
        g_modern_design.colors.bg_tertiary     = (Vec4_f32){0.149f, 0.149f, 0.149f, 1.0f};  // #262626 - header
        g_modern_design.colors.bg_elevated     = (Vec4_f32){0.176f, 0.176f, 0.176f, 1.0f};  // #2d2d2d
        
        // Interactive states - more subtle like Windows Explorer
        g_modern_design.colors.interactive_normal   = (Vec4_f32){0.0f, 0.0f, 0.0f, 0.0f};       // transparent
        g_modern_design.colors.interactive_hover    = (Vec4_f32){0.2f, 0.2f, 0.2f, 0.3f};       // subtle gray overlay
        g_modern_design.colors.interactive_active   = (Vec4_f32){0.25f, 0.25f, 0.25f, 0.5f};    // slightly darker
        g_modern_design.colors.interactive_disabled = (Vec4_f32){0.157f, 0.157f, 0.157f, 0.5f};  // #282828 50%
        
        // Text colors with good contrast
        g_modern_design.colors.text_primary   = (Vec4_f32){0.878f, 0.878f, 0.878f, 1.0f};  // #e0e0e0
        g_modern_design.colors.text_secondary = (Vec4_f32){0.659f, 0.659f, 0.659f, 1.0f};  // #a8a8a8
        g_modern_design.colors.text_disabled  = (Vec4_f32){0.467f, 0.467f, 0.467f, 1.0f};  // #777777
        g_modern_design.colors.text_accent    = (Vec4_f32){0.403f, 0.675f, 0.937f, 1.0f};  // #67abef
        
        // Border colors (more subtle like macOS)
        g_modern_design.colors.border_subtle = (Vec4_f32){0.2f, 0.2f, 0.2f, 1.0f};    // #333333 - very subtle
        g_modern_design.colors.border_normal = (Vec4_f32){0.25f, 0.25f, 0.25f, 1.0f};  // #404040 - normal
        g_modern_design.colors.border_strong = (Vec4_f32){0.3f, 0.3f, 0.3f, 1.0f};    // #4d4d4d - strong
        
        // Status colors
        g_modern_design.colors.success = (Vec4_f32){0.325f, 0.733f, 0.408f, 1.0f};  // #53bb68
        g_modern_design.colors.warning = (Vec4_f32){0.898f, 0.671f, 0.192f, 1.0f};  // #e5ab31
        g_modern_design.colors.error   = (Vec4_f32){0.859f, 0.373f, 0.373f, 1.0f};  // #db5f5f
        g_modern_design.colors.info    = (Vec4_f32){0.403f, 0.675f, 0.937f, 1.0f};  // #67abef
        
        // Accent colors
        g_modern_design.colors.accent_primary   = (Vec4_f32){0.403f, 0.675f, 0.937f, 1.0f};  // #67abef
        g_modern_design.colors.accent_secondary = (Vec4_f32){0.565f, 0.427f, 0.859f, 1.0f};  // #906ddb
        
        // Typography system
        g_modern_design.typography.size_xs   = 11.0f;
        g_modern_design.typography.size_sm   = 13.0f;
        g_modern_design.typography.size_base = 14.0f;
        g_modern_design.typography.size_lg   = 16.0f;
        g_modern_design.typography.size_xl   = 18.0f;
        g_modern_design.typography.size_2xl  = 20.0f;
        g_modern_design.typography.size_3xl  = 24.0f;
        
        // Spacing system (8px base unit)
        g_modern_design.spacing.xs   = 4.0f;
        g_modern_design.spacing.sm   = 8.0f;
        g_modern_design.spacing.base = 12.0f;
        g_modern_design.spacing.md   = 16.0f;
        g_modern_design.spacing.lg   = 20.0f;
        g_modern_design.spacing.xl   = 24.0f;
        g_modern_design.spacing.xxl  = 32.0f;
        g_modern_design.spacing.xxxl = 48.0f;
        
        g_modern_design_initialized = 1;
    }
    return &g_modern_design;
}

internal Vec4_f32
ui_color_mix(Vec4_f32 base, Vec4_f32 overlay, f32 alpha)
{
    Vec4_f32 result;
    result.x = base.x * (1.0f - alpha) + overlay.x * alpha;
    result.y = base.y * (1.0f - alpha) + overlay.y * alpha;
    result.z = base.z * (1.0f - alpha) + overlay.z * alpha;
    result.w = base.w;
    return result;
}

internal Vec4_f32
ui_color_lighten(Vec4_f32 color, f32 amount)
{
    Vec4_f32 white = (Vec4_f32){1.0f, 1.0f, 1.0f, 1.0f};
    return ui_color_mix(color, white, amount);
}

internal Vec4_f32
ui_color_darken(Vec4_f32 color, f32 amount)
{
    Vec4_f32 black = (Vec4_f32){0.0f, 0.0f, 0.0f, 1.0f};
    return ui_color_mix(color, black, amount);
}

internal void
ui_style_button(UI_Box *box, b32 is_primary)
{
    UI_ModernDesign *design = ui_get_modern_design();
    
    if (is_primary)
    {
        box->background_color = design->colors.accent_primary;
        box->background_color_hot = ui_color_lighten(design->colors.accent_primary, 0.1f);
        box->background_color_active = ui_color_darken(design->colors.accent_primary, 0.1f);
        box->text_color = (Vec4_f32){1.0f, 1.0f, 1.0f, 1.0f};
    }
    else
    {
        box->background_color = design->colors.interactive_normal;
        box->background_color_hot = design->colors.interactive_hover;
        box->background_color_active = design->colors.interactive_active;
        box->text_color = design->colors.text_primary;
    }
    
    box->corner_radius = 6.0f;  // Base border radius
    box->border_color = design->colors.border_subtle;
    box->border_thickness = 1.0f;
    
    // Add subtle shadow
    box->shadow_color = (Vec4_f32){0.0f, 0.0f, 0.0f, 0.1f};
    box->shadow_offset_x = 0.0f;
    box->shadow_offset_y = 1.0f;
    box->shadow_blur = 4.0f;
    box->use_shadow = 1;
}

internal void
ui_style_input(UI_Box *box)
{
    UI_ModernDesign *design = ui_get_modern_design();
    
    box->background_color = design->colors.bg_tertiary;
    box->background_color_hot = ui_color_lighten(design->colors.bg_tertiary, 0.05f);
    box->text_color = design->colors.text_primary;
    box->corner_radius = 6.0f;
    box->border_color = design->colors.border_subtle;
    box->border_thickness = 1.0f;
}

internal void
ui_style_sidebar(UI_Box *box)
{
    UI_ModernDesign *design = ui_get_modern_design();
    
    box->background_color = design->colors.bg_secondary;
    box->border_color = design->colors.border_subtle;
    box->border_thickness = 1.0f;
}

internal void
ui_style_card(UI_Box *box)
{
    UI_ModernDesign *design = ui_get_modern_design();
    
    box->background_color = design->colors.bg_tertiary;
    box->corner_radius = 8.0f;
    box->border_color = design->colors.border_subtle;
    box->border_thickness = 1.0f;
    
    // Add subtle shadow for depth
    box->shadow_color = (Vec4_f32){0.0f, 0.0f, 0.0f, 0.15f};
    box->shadow_offset_x = 0.0f;
    box->shadow_offset_y = 2.0f;
    box->shadow_blur = 8.0f;
    box->use_shadow = 1;
}

internal void
ui_style_list_item(UI_Box *box, b32 is_selected)
{
    UI_ModernDesign *design = ui_get_modern_design();
    
    if (is_selected)
    {
        box->background_color = ui_color_mix(design->colors.bg_secondary, design->colors.accent_primary, 0.15f);
        box->text_color = design->colors.text_primary;
    }
    else
    {
        box->background_color = design->colors.bg_secondary;
        box->background_color_hot = design->colors.interactive_hover;
        box->text_color = design->colors.text_primary;
        box->text_color_hot = design->colors.text_primary;
    }
    
    box->corner_radius = 3.0f;  // Small border radius
}

internal UI_Signal
ui_modern_button(String text, b32 is_primary)
{
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable |
                                               UI_BoxFlag_DrawBackground |
                                               UI_BoxFlag_DrawBorder |
                                               UI_BoxFlag_DrawText |
                                               UI_BoxFlag_HotAnimation |
                                               UI_BoxFlag_ActiveAnimation,
                                           text);
    ui_box_equip_display_string(box, text);
    ui_style_button(box, is_primary);
    
    // Add padding for better touch targets
    UI_ModernDesign *design = ui_get_modern_design();
    box->padding_left = design->spacing.md;
    box->padding_right = design->spacing.md;
    box->padding_top = design->spacing.sm;
    box->padding_bottom = design->spacing.sm;
    
    return ui_signal_from_box(box);
}

internal UI_Signal
ui_modern_buttonf(b32 is_primary, char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Signal result = ui_modern_button(string, is_primary);
    scratch_end(&scratch);
    return result;
}

internal UI_Signal
ui_modern_list_item(String primary_text, String secondary_text, b32 is_selected)
{
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable |
                                               UI_BoxFlag_DrawBackground |
                                               UI_BoxFlag_DrawText |
                                               UI_BoxFlag_HotAnimation,
                                           primary_text);
    
    // Create display text with primary and secondary
    String display_text = primary_text;
    if (secondary_text.size > 0)
    {
        display_text = str_pushf(ui_state->build_arenas[ui_state->build_index],
                                "%.*s\n%.*s", 
                                str_expand(primary_text),
                                str_expand(secondary_text));
    }
    
    ui_box_equip_display_string(box, display_text);
    ui_style_list_item(box, is_selected);
    
    // Add padding for better spacing
    UI_ModernDesign *design = ui_get_modern_design();
    box->padding_left = design->spacing.md;
    box->padding_right = design->spacing.md;
    box->padding_top = design->spacing.sm;
    box->padding_bottom = design->spacing.sm;
    
    return ui_signal_from_box(box);
}

internal void
ui_modern_separator(void)
{
    UI_ModernDesign *design = ui_get_modern_design();
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_DrawBackground, str_lit("###separator"));
    box->background_color = design->colors.border_subtle;
    box->semantic_size[Axis2_X] = ui_size_pct(1, 1);
    box->semantic_size[Axis2_Y] = ui_size_px(1, 1);
}

internal void
ui_modern_spacer(f32 size)
{
    ui_push_pref_width(ui_size_px(size, 0));
    ui_push_pref_height(ui_size_px(size, 0));
    ui_build_box_from_string(0, str_lit("###modern_spacer"));
    ui_pop_pref_height();
    ui_pop_pref_width();
}

internal UI_Signal
ui_modern_text_input(String *text, String placeholder, b32 is_focused)
{
    UI_ModernDesign *design = ui_get_modern_design();
    
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable |
                                               UI_BoxFlag_DrawBackground |
                                               UI_BoxFlag_DrawBorder |
                                               UI_BoxFlag_DrawText |
                                               UI_BoxFlag_HotAnimation,
                                           str_lit("text_input"));
    
    // Style as input field
    ui_style_input(box);
    
    // Display text or placeholder
    String display_text = (text->size > 0) ? *text : placeholder;
    ui_box_equip_display_string(box, display_text);
    
    // Different color for placeholder
    if (text->size == 0)
    {
        box->text_color = design->colors.text_disabled;
    }
    else
    {
        box->text_color = design->colors.text_primary;
    }
    
    // Focus styling
    if (is_focused)
    {
        box->border_color = design->colors.accent_primary;
        box->border_thickness = 2.0f;
    }
    
    // Add padding for better text placement
    box->padding_left = design->spacing.sm;
    box->padding_right = design->spacing.sm;
    box->padding_top = design->spacing.xs;
    box->padding_bottom = design->spacing.xs;
    
    return ui_signal_from_box(box);
}

internal void
ui_file_row(String name, String type, String size, String date, b32 is_folder, b32 is_selected, u32 row_index)
{
    UI_ModernDesign *design = ui_get_modern_design();
    
    // Very subtle alternating backgrounds like real file managers
    Vec4_f32 row_bg = (row_index % 2 == 0) ? 
        design->colors.bg_primary : 
        ui_color_mix(design->colors.bg_primary, design->colors.bg_secondary, 0.08f);  // very subtle
    
    // Selection highlighting
    if (is_selected)
    {
        row_bg = (Vec4_f32){0.0f, 0.47f, 0.84f, 1.0f};  // Windows blue
    }
    
    ui_push_pref_height(ui_size_px(20, 1.0f));  // Compact 20px rows like real file managers
    ui_push_background_color(row_bg);
    
    UI_Box *row = ui_table_row_begin();
    if (row)
    {
        row->flags |= UI_BoxFlag_Clickable | UI_BoxFlag_HotAnimation;
        
        // Name column with folder icon (60% width)
        ui_push_pref_width(ui_size_pct(0.6f, 1));
        UI_TableCell
        {
            ui_push_text_color(is_selected ? (Vec4_f32){1.0f, 1.0f, 1.0f, 1.0f} : design->colors.text_primary);
            
            // Add folder emoji/icon with left padding
            UI_Row {
                ui_spacer(ui_size_px(12, 1));
                String display_name;
                if (is_folder)
                {
                    display_name = str_pushf(ui_state->build_arenas[ui_state->build_index], "📁 %.*s", str_expand(name));
                }
                else
                {
                    display_name = str_pushf(ui_state->build_arenas[ui_state->build_index], "📄 %.*s", str_expand(name));
                }
                
                ui_labelf("%.*s", str_expand(display_name));
            }
            ui_pop_text_color();
        }
        ui_pop_pref_width();
        
        // Size column (20% width)
        ui_push_pref_width(ui_size_pct(0.2f, 1));
        UI_TableCell
        {
            ui_push_text_color(is_selected ? (Vec4_f32){0.9f, 0.9f, 0.9f, 1.0f} : design->colors.text_secondary);
            ui_labelf("%.*s", str_expand(size));
            ui_pop_text_color();
        }
        ui_pop_pref_width();
        
        // Kind column (20% width) - using type parameter
        ui_push_pref_width(ui_size_pct(0.2f, 1));
        UI_TableCell
        {
            ui_push_text_color(is_selected ? (Vec4_f32){0.9f, 0.9f, 0.9f, 1.0f} : design->colors.text_secondary);
            ui_labelf("%.*s", str_expand(type));
            ui_pop_text_color();
        }
        ui_pop_pref_width();
    }
    
    ui_table_row_end();
    ui_pop_background_color();
    ui_pop_pref_height();
}