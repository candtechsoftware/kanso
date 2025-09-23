#include "../base/system_headers.h"
#include "../os/os_inc.h"
#include "../base/base_inc.h"
#include "../base/profile.h"
#include "../renderer/renderer_inc.h"
#include "../font/font_inc.h"
#include "../draw/draw_inc.h"
#include <stdio.h>

#include "../base/base_inc.c"
#include "../base/profile.c"
#include "../os/os_inc.c"
#include "../renderer/renderer_inc.c"
#include "../font/font_inc.c"
#include "../draw/draw_inc.c"

#include "dbui/dbui.h"
#include "postgres.h"
#include "postgres.c"

typedef struct Node_Connection Node_Connection;
struct Node_Connection {
    u32 from_node;
    u32 to_node;
};

typedef struct Node_Box Node_Box;
struct Node_Box {
    Vec2_f32 center;
    Vec2_f32 size;
    Vec4_f32 color;
    String   name;

    DB_Schema schema;
    DB_Table *table_info;
    b32       is_expanded;

    Node_Box *next;
};

typedef struct Node_Box_List Node_Box_List;
struct Node_Box_List {
    Node_Box *first;
    Node_Box *last;
};

typedef struct App_State App_State;
struct App_State {
    Arena            *arena;
    App_Config       *config;
    OS_Handle         window;
    Renderer_Handle   window_equip;
    Font_Renderer_Tag default_font;
    b32               running;
    b32               mouse_down;
    b32               is_dragging;
    Vec2_f64          mouse_pos;
    Vec2_f64          mouse_drag_offset;
    s32               selected_node; // -1 = no selection

    Node_Box_List   *nodes;
    Node_Connection *connections;
    u64              connection_count;
    u64              node_count;

    DB_Schema_List schemas;
    DB_Conn       *db_conn;
    Dyn_Array     *list;

    f32      zoom_level;
    Vec2_f32 pan_offset;
    b32      is_panning;
    Vec2_f64 pan_start_pos;
};

App_State *g_state = nullptr;

internal DB_Conn *db_connect(DB_Config config) {
    switch (config.kind) {
    case DB_KIND_POSTGRES:
        return pg_connect(config);
    default:
        ASSERT(false, "Unimplemented");
    }
    return 0;
}

internal DB_Schema_List db_get_all_schemas(DB_Conn *conn) {
    DB_Schema_List list = {0};
    switch (conn->kind) {
    case DB_KIND_POSTGRES:
        return pg_get_all_schemas(conn);
    default:
        ASSERT(false, "Unimplemented");
    }
    return list;
}

internal DB_Table *db_get_schema_info(DB_Conn *conn, DB_Schema schema) {
    switch (conn->kind) {
    case DB_KIND_POSTGRES:
        return pg_get_schema_info(conn, schema);
    default:
        ASSERT(false, "Unimplemented");
    }
    return 0;
}

internal void db_free_schema_info(DB_Table *table) {
    if (table && table->arena) {
        arena_release(table->arena);
    }
}

internal DB_Table *db_get_data_from_schema(DB_Conn *conn, DB_Schema schema, u32 limit) {
    switch (conn->kind) {
    case DB_KIND_POSTGRES:
        return pg_get_data_from_schema(conn, schema, limit);
    default:
        ASSERT(false, "Unimplemented");
    }
    return 0;
}

internal b32
parse_args(Cmd_Line *cmd_line, App_Config *config) {
    if (cmd_line_has_flag(cmd_line, str_lit("help")) ||
        cmd_line_has_flag(cmd_line, str_lit("h"))) {
        print_help(cmd_line->bin_name);
        return 0;
    }

    if (cmd_line_has_flag(cmd_line, str_lit("files")) ||
        cmd_line_has_flag(cmd_line, str_lit("f"))) {
        config->search_files_only = true;
    }

    if (cmd_line_has_flag(cmd_line, str_lit("recursive")) ||
        cmd_line_has_flag(cmd_line, str_lit("r"))) {
        config->recursive = true;
    }

    String n = cmd_line_string(cmd_line, str_lit("path"));
    if (str_match(n, str_zero())) {
        n = str_lit(".");
    }
    config->pattern = n;

    return 1;
}

internal void
print_help(String bin_name) {
    print("Usage: {s} [path]                           \n", bin_name);
    print("\nOptions:\n");
    print("  -h, --help          Show this help message\n");
    print("  --path              Local config file path\n");
}

internal void
app_init(App_Config *config) {
    Arena *arena = arena_alloc();
    g_state = push_struct_zero(arena, App_State);
    g_state->arena = arena;
    { // Systems inits
        os_gfx_init();
        renderer_init();
        font_init();
        font_cache_init();
    }

    String font_path = str_lit("assets/fonts/sfpro.otf");
    if (!os_file_exists(font_path)) {
        font_path = str_lit("assets/fonts/LiberationMono-Regular.ttf");
    }

    Font_Renderer_Tag default_font = font_tag_from_path(font_path);
    if (default_font.data[0] == 0 && default_font.data[1] == 0) {
        log_error("Font failed to load\n");
        return;
    }

    OS_Window_Params window_params = {0};
    window_params.size = (Vec2_s32){1400, 900};
    window_params.title = str_lit("Dbui - Data base ui");
    g_state->window = os_window_open_params(window_params);
    ASSERT(!os_handle_is_zero(g_state->window), "OS window failed to open\n");

    g_state->window_equip = renderer_window_equip(g_state->window);
    g_state->default_font = default_font;
    g_state->running = 1;
    g_state->selected_node = -1;
    g_state->node_count = 0;
    g_state->zoom_level = 1.0f;
    g_state->pan_offset = (Vec2_f32){{0.0f, 0.0f}};
    g_state->is_panning = 0;

    DB_Config db_config;
    db_config.kind = DB_KIND_POSTGRES;
    db_config.connection_string = str_lit("postgresql://dbui_user:dbui_pass@localhost:5434/package_manager");
    g_state->db_conn = db_connect(db_config);
    ASSERT(g_state->db_conn != NULL, "Failed to connect to db");
    g_state->schemas = db_get_all_schemas(g_state->db_conn);

    g_state->nodes = push_array(g_state->arena, Node_Box_List, 1);
    g_state->connections = NULL;
    g_state->connection_count = 0;

    f32 x_offset = 300.0f;
    f32 y_offset = 200.0f;

    for (DB_Schema_Node *db_node = g_state->schemas.first; db_node; db_node = db_node->next) {
        if (db_node->v.kind == DB_SCHEMA_KIND_TABLE) {

            f32               font_size = 18.0f;
            Font_Renderer_Run text_run = font_run_from_string(
                g_state->default_font, font_size, 0, font_size * 4,
                Font_Renderer_Raster_Flag_Smooth, db_node->v.name);

            f32 padding = 20.0f;
            f32 box_width = text_run.dim.x + padding * 2;
            f32 box_height = text_run.dim.y + padding * 2;

            if (box_width < 150.0f)
                box_width = 150.0f;
            if (box_height < 60.0f)
                box_height = 60.0f;

            Node_Box *n = push_array(g_state->arena, Node_Box, 1);
            n->center = (Vec2_f32){{x_offset + box_width / 2, y_offset}};
            n->size = (Vec2_f32){{box_width, box_height}};
            n->color = (Vec4_f32){{0.0f, 0.5f, 1.0f, 1.0f}};
            n->name = str_push_copy(g_state->arena, db_node->v.name);
            n->schema = db_node->v;
            n->table_info = 0;
            n->is_expanded = false;
            SLLQueuePush(g_state->nodes->first, g_state->nodes->last, n);
            g_state->node_count++;

            x_offset += box_width + 30.0f;
            if (x_offset > 1100.0f) {
                x_offset = 50.0f;
                y_offset += 100.0f;
            }
        }
    }

    for (Node_Box *from_node = g_state->nodes->first; from_node; from_node = from_node->next) {
        if (!from_node->table_info) {
            from_node->table_info = db_get_schema_info(g_state->db_conn, from_node->schema);
        }

        if (from_node->table_info) {
            for (u32 i = 0; i < from_node->table_info->column_count; i++) {
                DB_Column_Info *col = dyn_array_get(&from_node->table_info->columns, DB_Column_Info, i);
                if (col && col->is_fk && col->foreign_table_name.size > 0) {
                    u32       from_idx = 0;
                    u32       to_idx = 0;
                    Node_Box *current = g_state->nodes->first;

                    while (current && current != from_node) {
                        from_idx++;
                        current = current->next;
                    }

                    current = g_state->nodes->first;
                    to_idx = 0;
                    while (current) {
                        if (str_match(current->name, col->foreign_table_name)) {
                            if (!g_state->connections) {
                                g_state->connections = push_array(g_state->arena, Node_Connection, 100);
                            }
                            g_state->connections[g_state->connection_count].from_node = from_idx;
                            g_state->connections[g_state->connection_count].to_node = to_idx;
                            g_state->connection_count++;
                            break;
                        }
                        to_idx++;
                        current = current->next;
                    }
                }
            }
        }
    }
}

internal Vec2_f32
mat3x3_transform_point(Mat3x3_f32 m, Vec2_f32 p) {
    Vec2_f32 result;
    result.x = m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2];
    result.y = m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2];
    return result;
}

internal Vec2_f32
screen_to_world(Vec2_f64 screen_pos) {
    Vec2_f32 result;
    result.x = (f32)(screen_pos.x - g_state->pan_offset.x) / g_state->zoom_level;
    result.y = (f32)(screen_pos.y - g_state->pan_offset.y) / g_state->zoom_level;
    return result;
}

internal Vec2_f32
world_to_screen(Vec2_f32 world_pos) {
    Vec2_f32 result;
    result.x = g_state->pan_offset.x + world_pos.x * g_state->zoom_level;
    result.y = g_state->pan_offset.y + world_pos.y * g_state->zoom_level;
    return result;
}

internal b32
point_in_rect(Vec2_f64 point, Vec2_f32 center, Vec2_f32 size) {
    f32 half_width = size.x * 0.5f;
    f32 half_height = size.y * 0.5f;
    return (point.x >= center.x - half_width &&
            point.x <= center.x + half_width &&
            point.y >= center.y - half_height &&
            point.y <= center.y + half_height);
}

internal void
app_update() {
    f64 current_time = os_get_time();
    f64 last_time = current_time;

    while (g_state->running) {
        last_time = current_time;
        current_time = os_get_time();
        f64 delta_time = current_time - last_time;
        Prof_FrameMark;

        OS_Event_List evs = os_event_list_from_window(g_state->window);
        for (OS_Event *ev = evs.first; ev; ev = ev->next) {

            if (ev->kind == OS_Event_Window_Close || (ev->kind == OS_Event_Press && ev->key == OS_Key_Esc)) {
                g_state->running = 0;
                break;
            }

            if (ev->kind == OS_Event_Scroll) {
                Vec2_f32 world_mouse_before = screen_to_world(g_state->mouse_pos);

                f32 old_zoom = g_state->zoom_level;
                f32 zoom_delta = ev->scroll.y * 0.1f;
                g_state->zoom_level *= (1.0f + zoom_delta);
                if (g_state->zoom_level < 0.2f)
                    g_state->zoom_level = 0.2f;
                if (g_state->zoom_level > 5.0f)
                    g_state->zoom_level = 5.0f;

                g_state->pan_offset.x = (f32)g_state->mouse_pos.x - world_mouse_before.x * g_state->zoom_level;
                g_state->pan_offset.y = (f32)g_state->mouse_pos.y - world_mouse_before.y * g_state->zoom_level;
            }

            if (ev->key == OS_Key_MouseMiddle && ev->kind == OS_Event_Press) {
                g_state->is_panning = 1;
                g_state->pan_start_pos = (Vec2_f64){{ev->position.x, ev->position.y}};
            }
            if (ev->key == OS_Key_MouseMiddle && ev->kind == OS_Event_Release) {
                g_state->is_panning = 0;
            }
            if (ev->key == OS_Key_MouseLeft && ev->kind == OS_Event_Press) {
                Vec2_f64 event_mouse_pos = (Vec2_f64){{ev->position.x, ev->position.y}};
                g_state->mouse_pos = event_mouse_pos;

                if (ev->modifiers & OS_Modifier_Ctrl) {
                    g_state->is_panning = 1;
                    g_state->pan_start_pos = g_state->mouse_pos;
                } else {
                    g_state->mouse_down = 1;
                    g_state->is_dragging = 0;

                    Vec2_f32 world_mouse = screen_to_world(event_mouse_pos);

                    g_state->selected_node = -1;
                    s32 node_index = 0;
                    for (Node_Box *node = g_state->nodes->first; node; node = node->next) {
                        if (point_in_rect((Vec2_f64){{world_mouse.x, world_mouse.y}}, node->center, node->size)) {
                            g_state->selected_node = node_index;
                            g_state->mouse_drag_offset.x = node->center.x - world_mouse.x;
                            g_state->mouse_drag_offset.y = node->center.y - world_mouse.y;
                            break;
                        }
                        node_index++;
                    }
                }
            }
            if (ev->key == OS_Key_MouseLeft && ev->kind == OS_Event_Release) {
                if (g_state->is_panning) {
                    g_state->is_panning = 0;
                } else {
                    if (!g_state->is_dragging && g_state->selected_node >= 0) {
                        Vec2_f32 world_mouse = screen_to_world(g_state->mouse_pos);
                        s32      node_index = 0;
                        for (Node_Box *node = g_state->nodes->first; node; node = node->next) {
                            if (node_index == g_state->selected_node &&
                                point_in_rect((Vec2_f64){{world_mouse.x, world_mouse.y}}, node->center, node->size)) {

                                node->is_expanded = !node->is_expanded;

                                if (node->is_expanded) {
                                    if (!node->table_info) {
                                        node->table_info = db_get_schema_info(g_state->db_conn, node->schema);
                                    }
                                    if (node->table_info) {
                                        Font_Renderer_Metrics metrics = font_metrics_from_tag_size(g_state->default_font, 14.0f);
                                        f32 line_spacing = font_line_height_from_metrics(&metrics);

                                        f32 expanded_height = 80.0f + ((float)node->table_info->column_count * line_spacing);
                                        if (expanded_height < node->size.y)
                                            expanded_height = node->size.y;
                                        node->size.y = expanded_height;

                                        if (node->size.x < 400.0f)
                                            node->size.x = 400.0f;
                                    }
                                } else {
                                    if (node->table_info) {
                                        db_free_schema_info(node->table_info);
                                        node->table_info = NULL;
                                    }

                                    f32 font_size = 18.0f;

                                    Font_Renderer_Run text_run = font_run_from_string(
                                        g_state->default_font, font_size, 0, font_size * 4,
                                        Font_Renderer_Raster_Flag_Smooth, node->schema.name);

                                    f32 padding = 20.0f;
                                    f32 box_width = text_run.dim.x + padding * 2;
                                    f32 box_height = text_run.dim.y + padding * 2;

                                    if (box_width < 150.0f)
                                        box_width = 150.0f;
                                    if (box_height < 60.0f)
                                        box_height = 60.0f;

                                    node->size = (Vec2_f32){{box_width, box_height}};
                                }
                                break;
                            }
                            node_index++;
                        }
                    }

                    g_state->mouse_down = 0;
                    g_state->is_dragging = 0;
                    g_state->selected_node = -1;
                }
            }
            if (ev->kind == OS_Event_Drag) {
                Vec2_f64 old_pos = g_state->mouse_pos;
                g_state->mouse_pos = (Vec2_f64){{ev->position.x, ev->position.y}};

                if (g_state->is_panning) {
                    g_state->pan_offset.x += (f32)(g_state->mouse_pos.x - old_pos.x);
                    g_state->pan_offset.y += (f32)(g_state->mouse_pos.y - old_pos.y);
                } else if (g_state->mouse_down && g_state->selected_node >= 0) {
                    g_state->is_dragging = 1;
                    Vec2_f32 world_mouse = screen_to_world(g_state->mouse_pos);
                    s32      node_index = 0;
                    for (Node_Box *node = g_state->nodes->first; node; node = node->next) {
                        if (node_index == g_state->selected_node) {
                            node->center.x = world_mouse.x + g_state->mouse_drag_offset.x;
                            node->center.y = world_mouse.y + g_state->mouse_drag_offset.y;
                            break;
                        }
                        node_index++;
                    }
                }
            }
        }

        renderer_window_begin_frame(g_state->window, g_state->window_equip);

        draw_begin_frame(g_state->default_font);
        Draw_Bucket *bucket = draw_bucket_make();
        draw_push_bucket(bucket);

        Rng2_f32 window_rect = os_rect_from_window(g_state->window);
        f32      window_width = window_rect.max.x - window_rect.min.x;
        f32      window_height = window_rect.max.y - window_rect.min.y;

        Mat3x3_f32 scale_matrix = mat3x3_scale(g_state->zoom_level);
        Mat3x3_f32 translate_matrix = mat3x3_translate(g_state->pan_offset.x, g_state->pan_offset.y);
        Mat3x3_f32 view_transform = mat3x3_mul(translate_matrix, scale_matrix);
        draw_push_xform2d(view_transform);

        Prof_Begin("DrawConnections");
        for (u64 i = 0; i < g_state->connection_count; i++) {
            Node_Connection *conn = &g_state->connections[i];

            Node_Box *from_node = NULL;
            Node_Box *to_node = NULL;
            u32       idx = 0;
            for (Node_Box *node = g_state->nodes->first; node; node = node->next) {
                if (idx == conn->from_node)
                    from_node = node;
                if (idx == conn->to_node)
                    to_node = node;
                idx++;
            }

            if (from_node && to_node) {
                Vec2_f32 from_center = from_node->center;
                Vec2_f32 to_center = to_node->center;

                Vec2_f32 dir = {{to_center.x - from_center.x, to_center.y - from_center.y}};
                f32      len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                if (len > 0.0f) {
                    dir.x /= len;
                    dir.y /= len;

                    Vec2_f32 from_edge = {{from_center.x + dir.x * (from_node->size.x / 2),
                                           from_center.y + dir.y * (from_node->size.y / 2)}};
                    Vec2_f32 to_edge = {{to_center.x - dir.x * (to_node->size.x / 2),
                                         to_center.y - dir.y * (to_node->size.y / 2)}};

                    f32      line_thickness = 2.0f;
                    Vec4_f32 line_color = {{0.3f, 0.8f, 0.3f, 0.8f}};

                    draw_line(from_edge, to_edge, line_thickness, line_color);

                    f32      arrow_size = 10.0f;
                    Vec2_f32 arrow_p1 = {{to_edge.x - dir.x * arrow_size - dir.y * arrow_size * 0.5f,
                                          to_edge.y - dir.y * arrow_size + dir.x * arrow_size * 0.5f}};
                    Vec2_f32 arrow_p2 = {{to_edge.x - dir.x * arrow_size + dir.y * arrow_size * 0.5f,
                                          to_edge.y - dir.y * arrow_size - dir.x * arrow_size * 0.5f}};

                    draw_line(to_edge, arrow_p1, line_thickness, line_color);
                    draw_line(to_edge, arrow_p2, line_thickness, line_color);
                }
            }
        }
        Prof_End();

        Prof_Begin("DrawNodes");
        s32 node_index = 0;
        for (Node_Box *node = g_state->nodes->first; node; node = node->next) {
            Rng2_f32 node_rect = {
                .min = {{node->center.x - node->size.x / 2, node->center.y - node->size.y / 2}},
                .max = {{node->center.x + node->size.x / 2, node->center.y + node->size.y / 2}}};

            f32 border_thickness = (node_index == g_state->selected_node) ? 4.0f : 2.0f;

            Vec4_f32 box_color = node->is_expanded ? (Vec4_f32){{0.1f, 0.3f, 0.6f, 1.0f}} : node->color;

            draw_rect(node_rect, box_color, 10.0f, border_thickness, 1.0f);

            if (node->name.size > 0) {
                String            label = node->name;
                Font_Renderer_Run run = font_run_from_string(g_state->default_font, 18.0f, 0, 18.0f * 4, Font_Renderer_Raster_Flag_Smooth, label);
                Vec2_f32          text_pos = {{node->center.x - run.dim.x * 0.5f,
                                               node->center.y - node->size.y / 2 + 10.0f}};
                Vec4_f32          text_color = {{1.0f, 1.0f, 1.0f, 1.0f}};
                draw_text(text_pos, label, g_state->default_font, 18.0f, text_color);

                if (node->is_expanded && node->table_info) {
                    Prof_Begin("DrawColumnInfo");
                    Scratch scratch = scratch_begin(g_state->arena);

                    f32      column_y = text_pos.y + 30.0f;
                    f32      small_font_size = 14.0f;
                    Vec4_f32 column_color = {{0.9f, 0.9f, 0.9f, 1.0f}};
                    Vec4_f32 fk_color = {{0.5f, 1.0f, 0.5f, 1.0f}};

                    Font_Renderer_Metrics metrics = font_metrics_from_tag_size(g_state->default_font, small_font_size);
                    f32 line_spacing = font_line_height_from_metrics(&metrics);

                    f32 node_bottom = node->center.y + node->size.y / 2 - 10.0f; // 10px padding

                    for (u32 i = 0; i < node->table_info->column_count; i++) {
                        if (column_y + line_spacing > node_bottom) {
                            break;
                        }

                        DB_Column_Info *col = dyn_array_get(&node->table_info->columns, DB_Column_Info, i);

                        if (col && col->display_text) {
                            String   col_string = cstr_to_string(col->display_text, strlen(col->display_text));
                            Vec2_f32 col_pos = {{node->center.x - node->size.x / 2 + 20.0f, column_y}};

                            Vec4_f32 current_color = col->is_fk ? fk_color : column_color;

                            draw_text(col_pos, col_string, g_state->default_font, small_font_size, current_color);

                            if (col->is_fk && col->fk_display) {
                                String            fk_string = cstr_to_string(col->fk_display, strlen(col->fk_display));
                                Font_Renderer_Run col_run = font_run_from_string(g_state->default_font, small_font_size, 0,
                                                                                 small_font_size * 4, Font_Renderer_Raster_Flag_Smooth, col_string);
                                Vec2_f32          fk_pos = {{col_pos.x + col_run.dim.x + 10.0f, column_y}};
                                draw_text(fk_pos, fk_string, g_state->default_font, small_font_size, fk_color);
                            }

                            column_y += line_spacing;
                        }
                    }

                    scratch_end(&scratch);
                    Prof_End();
                }
            }
            node_index++;
        }
        Prof_End();

        draw_pop_xform2d();
        draw_pop_bucket();
        draw_end_frame();
        draw_submit_bucket(g_state->window, g_state->window_equip, bucket);

        renderer_window_end_frame(g_state->window, g_state->window_equip);
    }
}
internal void
app_shutdown() {
    renderer_window_unequip(g_state->window, g_state->window_equip);
    os_window_close(g_state->window);
}

int main(int argc, char **argv) {
    Prof_Init();
    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);
    tctx_set_thread_name(str_lit("main"));

    Scratch     scratch = scratch_begin(tctx.arenas[0]);
    String_List str_list = os_string_list_from_argcv(scratch.arena, argc, argv);
    Cmd_Line    cmd_line = cmd_line_from_string_list(scratch.arena, str_list);

    App_Config config = default_config();
    if (!parse_args(&cmd_line, &config)) {
        return 1;
    }
    app_init(&config);
    ASSERT(g_state, "App state should be initialized");

    app_update();

    scratch_end(&scratch);
    arena_release(tctx.arenas[0]);
    app_shutdown();
    Prof_Shutdown();
}
