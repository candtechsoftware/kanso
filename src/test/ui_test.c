// UI Core Test - Minimal box-based UI demonstration

#include "../base/system_headers.h"
#include "../os/os_inc.h"
#include "../base/base_inc.h"
#include "../base/profile.h"
#include "../renderer/renderer_inc.h"
#include "../font/font_inc.h"
#include "../draw/draw_inc.h"
#include "../ui/ui_core.h"

#include "../base/base_inc.c"
#include "../base/profile.c"
#include "../os/os_inc.c"
#include "../renderer/renderer_inc.c"
#include "../font/font_inc.c"
#include "../draw/draw_inc.c"
#include "../ui/ui_core.c"

#include <stdio.h>

typedef struct App_State App_State;
struct App_State {
    Arena            *arena;
    OS_Handle         window;
    Renderer_Handle   renderer;
    Font_Renderer_Tag font;
    b32               running;
    Vec2              mouse_pos;
    u64               frame_count;
};

App_State *g_state = 0;

int main(int argc, char **argv) {
    os_gfx_init();
    renderer_init();
    font_init();
    font_cache_init();

    Arena *arena = arena_alloc();
    g_state = push_struct(arena, App_State);
    g_state->arena = arena;
    g_state->running = 1;

    OS_Window_Params window_params = {0};
    window_params.size = (Vec2_s32){1280, 720};
    window_params.title = str_lit("UI Core Test");
    g_state->window = os_window_open_params(window_params);

    g_state->renderer = renderer_window_equip(g_state->window);

    String font_path = str_lit("assets/fonts/LiberationMono-Regular.ttf");
    g_state->font = font_tag_from_path(font_path);
    if (g_state->font.data[0] == 0 && g_state->font.data[1] == 0) {
        printf("Warning: Font failed to load, using default\n");
    }

    f64 last_time = os_get_time();

    while (g_state->running) {
        f64 now = os_get_time();
        f32 dt = (f32)(now - last_time);
        last_time = now;

        // Process events
        OS_Event_List os_events = os_event_list_from_window(g_state->window);

        for (OS_Event *ev = os_events.first; ev != 0; ev = ev->next) {
            switch (ev->kind) {
            case OS_Event_Window_Close:
                g_state->running = 0;
                break;
            default:
                break;
            }

            // Track mouse position
            if (ev->kind == OS_Event_Press || ev->kind == OS_Event_Release) {
                g_state->mouse_pos = ev->position;
            }
        }


        renderer_window_begin_frame(g_state->window, g_state->renderer);
        draw_begin_frame(g_state->font);

        Draw_Bucket *bucket = draw_bucket_make();
        draw_push_bucket(bucket);


        draw_pop_bucket();
        draw_end_frame();
        draw_submit_bucket(g_state->window, g_state->renderer, bucket);
        renderer_window_end_frame(g_state->window, g_state->renderer);

        g_state->frame_count++;

        arena_clear(g_state->arena);
    }

    renderer_window_unequip(g_state->window, g_state->renderer);
    os_window_close(g_state->window);

    printf("UI Core Test Ended\n");
    return 0;
}
