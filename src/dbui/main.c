#include "../base/base_inc.h"
#include "../font/font_inc.h"
#include "../renderer/renderer_inc.h"
#include "../draw/draw_inc.h"
#include "../os/os_inc.h"
#include "../ui/ui_inc.h"

#include "../base/base_inc.c"
#include "../font/font_inc.c"
#include "../os/os_inc.c"
#include "../renderer/renderer_inc.c"
#include "../draw/draw_inc.c"
#include "../ui/ui_inc.c"

#include <stdio.h>
#include <math.h>

typedef struct FileItem FileItem;
struct FileItem
{
    String name;
    b32    is_directory;
    u64    size;
    f32    icon_hue;
};

String test_file_names[] = {
    {(u8 *)"Documents", 9},
    {(u8 *)"Downloads", 9},
    {(u8 *)"Pictures", 8},
    {(u8 *)"Videos", 6},
    {(u8 *)"Music", 5},
    {(u8 *)"readme.txt", 10},
    {(u8 *)"config.json", 11},
    {(u8 *)"main.c", 6},
    {(u8 *)"Makefile", 8},
    {(u8 *)"notes.md", 8},
};

b32 test_file_is_dir[] = {1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
f32 test_file_hues[] = {200.0f, 220.0f, 240.0f, 260.0f, 280.0f, 60.0f, 120.0f, 180.0f, 90.0f, 150.0f};

internal void
draw_file_icon(Rng2_f32 rect, b32 is_directory, f32 hue)
{
    Vec4_f32 color;
    if (is_directory)
    {
        color = (Vec4_f32){{0.329f, 0.584f, 0.894f, 1.0f}};
    }
    else
    {
        f32 h = hue / 360.0f;
        f32 s = 0.6f;
        f32 v = 0.7f;

        f32 c = v * s;
        f32 x = c * (1 - fabsf(fmodf(h * 6, 2) - 1));
        f32 m = v - c;

        if (h < 1.0f / 6.0f)
            color = (Vec4_f32){{c + m, x + m, m, 1}};
        else if (h < 2.0f / 6.0f)
            color = (Vec4_f32){{x + m, c + m, m, 1}};
        else if (h < 3.0f / 6.0f)
            color = (Vec4_f32){{m, c + m, x + m, 1}};
        else if (h < 4.0f / 6.0f)
            color = (Vec4_f32){{m, x + m, c + m, 1}};
        else if (h < 5.0f / 6.0f)
            color = (Vec4_f32){{x + m, m, c + m, 1}};
        else
            color = (Vec4_f32){{c + m, m, x + m, 1}};
    }

    draw_rect(rect, color, 4.0f, 0, 1.0f);
}

int
main()
{
    printf("Initializing File Manager...\n");

    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);

    Prof_Init();

    os_gfx_init();
    renderer_init();
    font_init();
    font_cache_init();

#ifdef __APPLE__
    String font_path = str_lit("/System/Library/Fonts/Helvetica.ttc");
#else
    String font_path = str_lit("assets/fonts/LiberationMono-Regular.ttf");
#endif
    Font_Renderer_Tag default_font = font_tag_from_path(font_path);

    OS_Window_Params window_params = {};
    window_params.size = (Vec2_s32){1280, 800};
    window_params.title = str_lit("Kanso File Manager");

    OS_Handle window = os_window_open_params(window_params);
    if (os_handle_is_zero(window))
    {
        printf("Failed to create window\n");
        return 1;
    }

    void           *native_window = os_window_native_handle(window);
    Renderer_Handle window_equip = renderer_window_equip(native_window);

    UI_State *ui = ui_state_alloc();

    f32      fps = 0.0f;
    Vec2_f32 scroll_pos = {{0, 0}};
    b32      show_hidden = 0;
    b32      grid_view = 1;
    s32      selected_file = -1;

    f64 current_time = os_get_time();
    f64 last_time = current_time;
    f64 fps_update_time = current_time;
    f64 fps_frame_count = 0;

    b32 running = 1;

    while (running)
    {
        last_time = current_time;
        current_time = os_get_time();
        f64 delta_time = current_time - last_time;

        Prof_FrameMark;

        fps_frame_count++;
        if (current_time - fps_update_time >= 0.10)
        {
            fps = (f32)(fps_frame_count / (current_time - fps_update_time));
            fps_frame_count = 0;
            fps_update_time = current_time;
        }

        OS_Event_List events = os_event_list_from_window(window);
        for (OS_Event *event = events.first; event; event = event->next)
        {
            if (event->kind == OS_Event_Window_Close)
            {
                running = 0;
            }
            else if (event->kind == OS_Event_Press && event->key == OS_Key_Esc)
            {
                running = 0;
            }

            UI_Event ui_event = {0};
            if (event->kind == OS_Event_Press && event->key == OS_Key_MouseLeft)
            {
                ui_event.kind = UI_EventKind_MousePress;
                ui_event.pos = event->position;
                ui->mouse_pressed = 1;
                ui->drag_start_mouse = event->position;
            }
            else if (event->kind == OS_Event_Release && event->key == OS_Key_MouseLeft)
            {
                ui_event.kind = UI_EventKind_MouseRelease;
                ui_event.pos = event->position;
                ui->mouse_released = 1;
            }

            if (event->position.x != 0 || event->position.y != 0)
            {
                ui->mouse_pos = event->position;
            }

            if (ui_event.kind != UI_EventKind_Null)
            {
                ui_push_event(&ui->events, &ui_event);
            }
        }

        Rng2_f32 window_rect = os_client_rect_from_window(window);
        f32      window_width = window_rect.max.x - window_rect.min.x;
        f32      window_height = window_rect.max.y - window_rect.min.y;

        renderer_window_begin_frame(native_window, window_equip);
        draw_begin_frame(default_font);

        ui_begin_frame(ui, (f32)delta_time);
        ui->root->semantic_size[Axis2_X] = ui_size_px(window_width, 1);
        ui->root->semantic_size[Axis2_Y] = ui_size_px(window_height, 1);
        ui->root->rect = (Rng2_f32){{{0, 0}}, {{window_width, window_height}}};

        // Main vertical layout
        UI_Column
        {
            // Header bar
            ui_push_pref_height(ui_size_px(45, 1));
            ui_push_background_color((Vec4_f32){{0.110f, 0.110f, 0.110f, 1}});
            ui_push_border_color((Vec4_f32){{0.157f, 0.157f, 0.157f, 1}});

            UI_Box *header = ui_build_box_from_string(
                UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder,
                str_lit("###header"));
            ui_push_parent(header);

            UI_Row
            {
                ui_spacer(ui_size_px(10, 1));

                ui_push_pref_width(ui_size_px(80, 1));
                ui_push_pref_height(ui_size_px(30, 1));
                ui_push_corner_radius(6);
                ui_push_background_color((Vec4_f32){{0.157f, 0.157f, 0.157f, 1}});

                if (ui_button(str_lit("Back")).clicked)
                {
                    // Navigate back
                }
                ui_pop_background_color();
                ui_pop_corner_radius();
                ui_pop_pref_height();
                ui_pop_pref_width();

                ui_spacer(ui_size_px(10, 1));

                ui_push_pref_width(ui_size_pct(1, 0));
                ui_push_text_color((Vec4_f32){{0.933f, 0.933f, 0.933f, 1}});
                ui_label(str_lit("/Users/Documents"));
                ui_pop_text_color();
                ui_pop_pref_width();

                ui_spacer(ui_size_px(10, 1));
            }

            ui_pop_parent();
            ui_pop_border_color();
            ui_pop_background_color();
            ui_pop_pref_height();

            // Main content area with sidebar and file grid
            UI_Row
            {
                // Sidebar
                ui_push_pref_width(ui_size_px(200, 1));
                ui_push_pref_height(ui_size_pct(1, 1));
                ui_push_background_color((Vec4_f32){{0.110f, 0.110f, 0.110f, 1}});
                ui_push_border_color((Vec4_f32){{0.157f, 0.157f, 0.157f, 1}});

                UI_Box *sidebar = ui_build_box_from_string(
                    UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder,
                    str_lit("###sidebar"));
                ui_push_parent(sidebar);

                UI_Column
                {
                    ui_push_pref_height(ui_size_px(30, 1));
                    ui_push_text_color((Vec4_f32){{0.643f, 0.643f, 0.643f, 1}});

                    ui_spacer(ui_size_px(10, 1));
                    ui_label(str_lit("FAVORITES"));
                    ui_spacer(ui_size_px(5, 1));

                    ui_push_text_color((Vec4_f32){{0.933f, 0.933f, 0.933f, 1}});
                    ui_push_pref_height(ui_size_px(28, 1));

                    ui_label(str_lit("  Desktop"));
                    ui_label(str_lit("  Documents"));
                    ui_label(str_lit("  Downloads"));
                    ui_label(str_lit("  Pictures"));

                    ui_pop_pref_height();
                    ui_pop_text_color();

                    ui_spacer(ui_size_px(20, 1));
                    ui_label(str_lit("DEVICES"));
                    ui_spacer(ui_size_px(5, 1));

                    ui_push_text_color((Vec4_f32){{0.933f, 0.933f, 0.933f, 1}});
                    ui_push_pref_height(ui_size_px(28, 1));

                    ui_label(str_lit("  Macintosh HD"));

                    ui_pop_pref_height();
                    ui_pop_text_color();
                    ui_pop_text_color();
                    ui_pop_pref_height();
                }

                ui_pop_parent();
                ui_pop_border_color();
                ui_pop_background_color();
                ui_pop_pref_height();
                ui_pop_pref_width();

                // File grid area
                ui_push_pref_width(ui_size_pct(1, 1));
                ui_push_pref_height(ui_size_pct(1, 1));
                ui_push_background_color((Vec4_f32){{0.082f, 0.082f, 0.082f, 1}});

                UI_Box *content = ui_build_box_from_string(
                    UI_BoxFlag_DrawBackground,
                    str_lit("###content"));
                ui_push_parent(content);

                // File grid
                ui_push_pref_width(ui_size_px(120, 0));
                ui_push_pref_height(ui_size_px(100, 0));

                f32 grid_x = 20;
                f32 grid_y = 20;

                for (s32 i = 0; i < ArrayCount(test_file_names); i++)
                {
                    String name = test_file_names[i];
                    b32    is_dir = test_file_is_dir[i];
                    f32    hue = test_file_hues[i];

                    UI_Box *file_box = ui_build_box_from_stringf(
                        UI_BoxFlag_Clickable | UI_BoxFlag_DrawBackground,
                        "###file_%d", i);

                    file_box->semantic_size[Axis2_X] = ui_size_px(100, 1);
                    file_box->semantic_size[Axis2_Y] = ui_size_px(90, 1);
                    file_box->rect.min.x = content->rect.min.x + grid_x;
                    file_box->rect.min.y = content->rect.min.y + grid_y;
                    file_box->rect.max.x = file_box->rect.min.x + 100;
                    file_box->rect.max.y = file_box->rect.min.y + 90;

                    if (selected_file == i)
                    {
                        file_box->background_color = (Vec4_f32){{0.0f, 0.514f, 0.702f, 0.3f}};
                    }
                    else
                    {
                        file_box->background_color = (Vec4_f32){{0.0f, 0.0f, 0.0f, 0.0f}};
                    }

                    file_box->corner_radius = 8;

                    UI_Signal sig = ui_signal_from_box(file_box);
                    if (sig.clicked)
                    {
                        selected_file = i;
                    }

                    // Draw icon
                    Rng2_f32 icon_rect = {
                        {{file_box->rect.min.x + 26, file_box->rect.min.y + 10}},
                        {{file_box->rect.min.x + 74, file_box->rect.min.y + 58}}};
                    draw_file_icon(icon_rect, is_dir, hue);

                    // Draw label
                    Vec2_f32 text_pos = {{floorf(file_box->rect.min.x + 50),
                                          floorf(file_box->rect.min.y + 72)}};

                    // Center text
                    f32 text_width = name.size * 7;
                    text_pos.x -= text_width / 2;

                    draw_text(text_pos, name, default_font, 12.0f,
                              (Vec4_f32){{0.933f, 0.933f, 0.933f, 1}});

                    grid_x += 120;
                    if (grid_x > window_width - 250)
                    {
                        grid_x = 20;
                        grid_y += 110;
                    }
                }

                ui_pop_pref_height();
                ui_pop_pref_width();

                ui_pop_parent();
                ui_pop_background_color();
                ui_pop_pref_height();
                ui_pop_pref_width();
            }
        }

        ui_end_frame(ui);

        Draw_Bucket *bucket = draw_bucket_make();
        draw_push_bucket(bucket);

        // Clear background with FilePilot dark theme color
        draw_rect((Rng2_f32){{{0, 0}}, {{window_width, window_height}}},
                  (Vec4_f32){{0.082f, 0.082f, 0.082f, 1}}, 0, 0, 0);

        // Draw the UI
        ui_draw(ui->root, default_font);

        draw_pop_bucket();
        draw_submit_bucket(native_window, window_equip, bucket);
        draw_end_frame();

        renderer_window_end_frame(native_window, window_equip);

        ui->mouse_pressed = 0;
        ui->mouse_released = 0;
        ui->hot_box_key = 0;
    }

    ui_state_release(ui);
    renderer_window_unequip(native_window, window_equip);
    os_window_close(window);

    Prof_Report();
    Prof_Shutdown();

    printf("File Manager shutdown complete\n");
    return 0;
}