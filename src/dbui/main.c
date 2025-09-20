#include "../base/system_headers.h"
#include "../os/os_inc.h"
#include "../base/base_inc.h"
#include "../base/profile.h"
#include "../renderer/renderer_inc.h"
#include "../font/font_inc.h"
#include "../draw/draw_inc.h"

#include "../base/base_inc.c"
#include "../base/profile.c"
#include "../os/os_inc.c"
#include "../renderer/renderer_inc.c"
#include "../font/font_inc.c"
#include "../draw/draw_inc.c"

#include "dbui/dbui.h"

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
    char    *name;
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
    Vec2_f64          mouse_pos;
    Vec2_f64          mouse_drag_offset;
    s32               selected_node;  // -1 = no selection

    // Node editor state
    Node_Box        nodes[3];
    Node_Connection connections[3];
    u32             connection_count;
};

App_State *g_state = nullptr;

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
    g_state->selected_node = -1;  // No node selected initially

    // Initialize nodes
    g_state->nodes[0] = (Node_Box){
        .center = {{300.0f, 200.0f}},
        .size = {{200.0f, 200.0f}},
        .color = {{0.0f, 0.5f, 1.0f, 1.0f}}, // Blue
        .name = "Input Node"};

    g_state->nodes[1] = (Node_Box){
        .center = {{700.0f, 450.0f}},
        .size = {{200.0f, 200.0f}},
        .color = {{1.0f, 0.0f, 0.0f, 1.0f}}, // Red
        .name = "Process Node"};

    g_state->nodes[2] = (Node_Box){
        .center = {{1100.0f, 200.0f}},
        .size = {{200.0f, 200.0f}},
        .color = {{0.0f, 1.0f, 0.0f, 0.8f}}, // Green
        .name = "Output Node"};

    // Initialize connections (0->1, 1->2)
    g_state->connections[0] = (Node_Connection){.from_node = 0, .to_node = 1};
    g_state->connections[1] = (Node_Connection){.from_node = 1, .to_node = 2};
    g_state->connection_count = 2;
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
            if (ev->key == OS_Key_MouseLeft && ev->kind == OS_Event_Press) {
                g_state->mouse_down = 1;
                g_state->mouse_pos = (Vec2_f64){{ev->position.x, ev->position.y}};

                g_state->selected_node = -1;
                for (s32 i = 0; i < 3; i++) {
                    if (point_in_rect(g_state->mouse_pos, g_state->nodes[i].center, g_state->nodes[i].size)) {
                        g_state->selected_node = i;
                        g_state->mouse_drag_offset.x = g_state->nodes[i].center.x - g_state->mouse_pos.x;
                        g_state->mouse_drag_offset.y = g_state->nodes[i].center.y - g_state->mouse_pos.y;
                        break;
                    }
                }
            }
            if (ev->key == OS_Key_MouseLeft && ev->kind == OS_Event_Release) {
                g_state->mouse_down = 0;
                g_state->selected_node = -1;
            }
            if (ev->kind == OS_Event_Drag && g_state->mouse_down) {
                Vec2_f64 old_pos = g_state->mouse_pos;
                g_state->mouse_pos = (Vec2_f64){{ev->position.x, ev->position.y}};

                if (g_state->selected_node >= 0 && g_state->selected_node < 3) {
                    g_state->nodes[g_state->selected_node].center.x = (f32)(g_state->mouse_pos.x + g_state->mouse_drag_offset.x);
                    g_state->nodes[g_state->selected_node].center.y = (f32)(g_state->mouse_pos.y + g_state->mouse_drag_offset.y);
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

        // Draw connections first (so they appear behind nodes)main
        Vec4_f32 connection_color = {{0.8f, 0.8f, 0.8f, 1.0f}};
        for (u32 i = 0; i < g_state->connection_count; i++) {
            Node_Connection *conn = &g_state->connections[i];
            Node_Box        *from = &g_state->nodes[conn->from_node];
            Node_Box        *to = &g_state->nodes[conn->to_node];

            // Calculate connection points (right side of from node, left side of to node)
            Vec2_f32 start_point = {{from->center.x + from->size.x * 0.5f,
                                     from->center.y}};
            Vec2_f32 end_point = {{to->center.x - to->size.x * 0.5f,
                                   to->center.y}};

            // Draw line
            draw_line(start_point, end_point, 1.0f, connection_color);

            // Draw connection dots at attachment points
            Vec4_f32 dot_color = {{1.0f, 1.0f, 1.0f, 1.0f}};
            f32      dot_size = 12.0f;

            Rng2_f32 start_dot = {
                .min = {{start_point.x - dot_size / 2, start_point.y - dot_size / 2}},
                .max = {{start_point.x + dot_size / 2, start_point.y + dot_size / 2}}};
            draw_rect(start_dot, dot_color, dot_size / 2, 0.0f, 1.0f);

            Rng2_f32 end_dot = {
                .min = {{end_point.x - dot_size / 2, end_point.y - dot_size / 2}},
                .max = {{end_point.x + dot_size / 2, end_point.y + dot_size / 2}}};
            draw_rect(end_dot, dot_color, dot_size / 2, 0.0f, 1.0f);
        }

        // Draw nodes
        for (u32 i = 0; i < 3; i++) {
            Node_Box *node = &g_state->nodes[i];

            Rng2_f32 node_rect = {
                .min = {{node->center.x - node->size.x / 2, node->center.y - node->size.y / 2}},
                .max = {{node->center.x + node->size.x / 2, node->center.y + node->size.y / 2}}};

            f32 border_thickness = (i == g_state->selected_node) ? 4.0f : 2.0f;
            draw_rect(node_rect, node->color, 10.0f, border_thickness, 1.0f);

            // Draw node label
            if (node->name) {
                String   label = {.data = (u8 *)node->name, .size = strlen(node->name)};
                Font_Renderer_Run run = font_run_from_string(g_state->default_font, 18.0f, 0, 18.0f * 4, Font_Renderer_Raster_Flag_Smooth, label);
                Vec2_f32 text_pos = {{node->center.x - run.dim.x * 0.5f,
                                      node->center.y - run.dim.y * 0.5f}};
                Vec4_f32 text_color = {{1.0f, 1.0f, 1.0f, 1.0f}};
                draw_text(text_pos, label, g_state->default_font, 18.0f, text_color);
            }
        }

        // Submit drawing
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
