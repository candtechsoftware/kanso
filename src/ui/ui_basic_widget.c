#pragma once
#include "ui_basic_widget.h"

internal UI_Signal
ui_label(String string)
{
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_DrawText, string);
    ui_box_equip_display_string(box, string);
    // Size label to its text content if no explicit size is set
    if (ui_state->pref_width == 0) {
        box->semantic_size[Axis2_X] = ui_size_text(0, 0);
    }
    return ui_signal_from_box(box);
}

internal UI_Signal
ui_labelf(char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Signal result = ui_label(string);
    scratch_end(&scratch);
    return result;
}

internal UI_Signal
ui_button(String string)
{
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable |
                                               UI_BoxFlag_DrawBackground |
                                               UI_BoxFlag_DrawBorder |
                                               UI_BoxFlag_DrawText |
                                               UI_BoxFlag_HotAnimation |
                                               UI_BoxFlag_ActiveAnimation,
                                           string);
    ui_box_equip_display_string(box, string);
    return ui_signal_from_box(box);
}

internal UI_Signal
ui_buttonf(char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Signal result = ui_button(string);
    scratch_end(&scratch);
    return result;
}

internal UI_Signal
ui_checkbox(b32 *checked, String string)
{
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_Clickable |
                                               UI_BoxFlag_DrawBackground |
                                               UI_BoxFlag_DrawBorder |
                                               UI_BoxFlag_DrawText |
                                               UI_BoxFlag_HotAnimation |
                                               UI_BoxFlag_ActiveAnimation,
                                           string);

    String display = str_pushf(ui_state->build_arenas[ui_state->build_index],
                               "[%s] %.*s",
                               *checked ? "X" : " ",
                               str_expand(string));
    ui_box_equip_display_string(box, display);

    UI_Signal sig = ui_signal_from_box(box);
    if (sig.clicked)
    {
        *checked = !*checked;
    }
    return sig;
}

internal UI_Signal
ui_checkboxf(b32 *checked, char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Signal result = ui_checkbox(checked, string);
    scratch_end(&scratch);
    return result;
}

internal void
ui_spacer(UI_Size size)
{
    ui_push_pref_width(size);
    ui_push_pref_height(size);
    ui_build_box_from_string(0, str_lit("###spacer"));
    ui_pop_pref_height();
    ui_pop_pref_width();
}

internal UI_Box *
ui_row_begin(void)
{
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_LayoutAxisX, str_lit("###row"));
    box->semantic_size[Axis2_X] = ui_size_pct(1, 0);
    // For rows, inherit height from parent's pushed pref_height if available,
    // otherwise size to children
    if (ui_state->pref_height && ui_state->pref_height->value.kind != UI_SizeKind_Null) {
        box->semantic_size[Axis2_Y] = ui_state->pref_height->value;
    } else {
        box->semantic_size[Axis2_Y] = ui_size_children(0, 0);
    }
    ui_push_parent(box);
    return box;
}

internal UI_Signal
ui_row_end(void)
{
    UI_Box *box = ui_pop_parent();
    return ui_signal_from_box(box);
}

internal UI_Box *
ui_column_begin(void)
{
    UI_Box *box = ui_build_box_from_string(0, str_lit("###column"));
    box->semantic_size[Axis2_X] = ui_size_pct(1, 0);
    box->semantic_size[Axis2_Y] = ui_size_children(0, 0);  // Size to content height
    ui_push_parent(box);
    return box;
}

internal UI_Signal
ui_column_end(void)
{
    UI_Box *box = ui_pop_parent();
    return ui_signal_from_box(box);
}

internal UI_Box *
ui_named_row_begin(String string)
{
    UI_Box *box = ui_build_box_from_string(UI_BoxFlag_LayoutAxisX, string);
    ui_push_parent(box);
    return box;
}

internal UI_Box *
ui_named_row_beginf(char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Box *result = ui_named_row_begin(string);
    scratch_end(&scratch);
    return result;
}

internal UI_Signal
ui_named_row_end(void)
{
    UI_Box *box = ui_pop_parent();
    return ui_signal_from_box(box);
}

internal UI_Box *
ui_named_column_begin(String string)
{
    UI_Box *box = ui_build_box_from_string(0, string);
    ui_push_parent(box);
    return box;
}

internal UI_Box *
ui_named_column_beginf(char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Box *result = ui_named_column_begin(string);
    scratch_end(&scratch);
    return result;
}

internal UI_Signal
ui_named_column_end(void)
{
    UI_Box *box = ui_pop_parent();
    return ui_signal_from_box(box);
}

thread_static u64  ui_table_column_count = 0;
thread_static f32 *ui_table_column_pcts = 0;
thread_static u64  ui_table_current_column = 0;

internal void
ui_table_begin(u64 column_count, f32 *column_pcts, String string)
{
    ui_table_column_count = column_count;
    ui_table_column_pcts = column_pcts;
    ui_table_current_column = 0;

    UI_Box *table = ui_build_box_from_string(0, string);
    ui_push_parent(table);
}

internal void
ui_table_beginf(u64 column_count, f32 *column_pcts, char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    ui_table_begin(column_count, column_pcts, string);
    scratch_end(&scratch);
}

internal void
ui_table_end(void)
{
    ui_pop_parent();
    ui_table_column_count = 0;
    ui_table_column_pcts = 0;
    ui_table_current_column = 0;
}

internal UI_Box *
ui_table_row_begin(void)
{
    UI_Box *row = ui_build_box_from_string(0, str_lit("###table_row"));
    ui_push_parent(row);
    ui_table_current_column = 0;
    return row;
}

internal UI_Signal
ui_table_row_end(void)
{
    UI_Box *row = ui_pop_parent();
    return ui_signal_from_box(row);
}

internal UI_Box *
ui_table_cell_begin(void)
{
    if (ui_table_column_pcts && ui_table_current_column < ui_table_column_count)
    {
        ui_push_pref_width(ui_size_pct(ui_table_column_pcts[ui_table_current_column], 1));
    }

    UI_Box *cell = ui_build_box_from_string(0, str_lit("###table_cell"));
    ui_push_parent(cell);
    ui_table_current_column++;

    if (ui_table_column_pcts)
    {
        ui_pop_pref_width();
    }

    return cell;
}

internal UI_Signal
ui_table_cell_end(void)
{
    UI_Box *cell = ui_pop_parent();
    return ui_signal_from_box(cell);
}

internal UI_Box *
ui_scroll_region_begin(Vec2_f32 *scroll, String string)
{
    UI_Box *region = ui_build_box_from_string(UI_BoxFlag_Clip | UI_BoxFlag_ViewScroll, string);
    region->rel_pos = *scroll;
    ui_push_parent(region);
    return region;
}

internal UI_Box *
ui_scroll_region_beginf(Vec2_f32 *scroll, char *fmt, ...)
{
    Scratch scratch = scratch_begin(tctx_get_scratch(0, 0));
    va_list args;
    va_start(args, fmt);
    String string = str_pushf(scratch.arena, fmt, args);
    va_end(args);
    UI_Box *result = ui_scroll_region_begin(scroll, string);
    scratch_end(&scratch);
    return result;
}

internal UI_Signal
ui_scroll_region_end(void)
{
    UI_Box   *region = ui_pop_parent();
    UI_Signal sig = ui_signal_from_box(region);

    // Temporarily disable scroll handling to prevent crash
    // TODO: Fix event list corruption issue
    /*
    if (sig.hovering && ui_state && ui_state->events.first)
    {
        for (UI_Event *event = ui_state->events.first; event != 0; )
        {
            if (event->kind == UI_EventKind_MouseScroll)
            {
                region->rel_pos.y += event->delta.y * 20;
                if (region->rel_pos.y > 0)
                    region->rel_pos.y = 0;
            }
            
            // Safely advance to next event
            UI_Event *next = event->next;
            if (!next) break;
            event = next;
        }
    }
    */

    return sig;
}