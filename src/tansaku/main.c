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

#include "tansaku/tansaku.h"

typedef struct App_State App_State;
struct App_State {
    Arena            *arena;
    App_Config       *config;
    OS_Handle         window;
    Renderer_Handle   window_equip;
    Font_Renderer_Tag default_font;
    b32               running;
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
        }

        renderer_window_begin_frame(g_state->window, g_state->window_equip);

        // Begin draw frame
        draw_begin_frame(g_state->default_font);
        Draw_Bucket *bucket = draw_bucket_make();
        draw_push_bucket(bucket);

        // Get window size for positioning
        Rng2_f32 window_rect = os_rect_from_window(g_state->window);
        f32 window_width = window_rect.max.x - window_rect.min.x;
        f32 window_height = window_rect.max.y - window_rect.min.y;

        // Draw a red square in the center
        f32 square_size = 200.0f;
        f32 center_x = window_width / 2.0f;
        f32 center_y = window_height / 2.0f;

        Rng2_f32 square_rect = {
            .min = {{center_x - square_size/2, center_y - square_size/2}},
            .max = {{center_x + square_size/2, center_y + square_size/2}}
        };

        // Draw red square with rounded corners
        Vec4_f32 red_color = {{1.0f, 0.0f, 0.0f, 1.0f}};
        draw_rect(square_rect, red_color, 10.0f, 0.0f, 2.0f);

        // Draw a blue square with border
        Rng2_f32 blue_square = {
            .min = {{100.0f, 100.0f}},
            .max = {{300.0f, 300.0f}}
        };
        Vec4_f32 blue_color = {{0.0f, 0.5f, 1.0f, 1.0f}};
        draw_rect(blue_square, blue_color, 5.0f, 3.0f, 1.0f);

        // Draw a green square in bottom right
        Rng2_f32 green_square = {
            .min = {{window_width - 250.0f, window_height - 250.0f}},
            .max = {{window_width - 50.0f, window_height - 50.0f}}
        };
        Vec4_f32 green_color = {{0.0f, 1.0f, 0.0f, 0.8f}};
        draw_rect(green_square, green_color, 20.0f, 0.0f, 3.0f);

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
