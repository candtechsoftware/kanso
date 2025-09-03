#include "../base/base_inc.h"
#include "../font/font_inc.h"
#include "../renderer/renderer_inc.h"
#include "../draw/draw_inc.h"
#include "../os/os_inc.h"

#include "../base/base_inc.c"
#include "../font/font_inc.c"
#include "../os/os_inc.c" 
#include "../renderer/renderer_inc.c"
#include "../draw/draw_inc.c"

#include <stdio.h>
#include <math.h>

typedef struct {
    f32 x, y, z;
} Point3D;

typedef struct {
    f32 x, y;
} Point2D;

Point2D project_point(Point3D p3d, f32 center_x, f32 center_y, f32 scale) {
    f32 perspective = 1.0f / (5.0f - p3d.z);
    Point2D p2d;
    p2d.x = center_x + p3d.x * scale * perspective;
    p2d.y = center_y + p3d.y * scale * perspective;
    return p2d;
}

Point3D rotate_point(Point3D p, f32 rx, f32 ry, f32 rz) {
    rx *= 3.14159f / 180.0f;
    ry *= 3.14159f / 180.0f;
    rz *= 3.14159f / 180.0f;
    
    f32 sx = sinf(rx), cx = cosf(rx);
    f32 sy = sinf(ry), cy = cosf(ry);
    f32 sz = sinf(rz), cz = cosf(rz);
    
    Point3D result;
    
    f32 y = p.y * cx - p.z * sx;
    f32 z = p.y * sx + p.z * cx;
    p.y = y;
    p.z = z;
    
    f32 x = p.x * cy + p.z * sy;
    z = -p.x * sy + p.z * cy;
    p.x = x;
    p.z = z;
    
    x = p.x * cz - p.y * sz;
    y = p.x * sz + p.y * cz;
    
    result.x = x;
    result.y = y;
    result.z = z;
    
    return result;
}

int main() {
    printf("Initializing engine...\n");
    
    TCTX tctx = {0};
    tctx_init_and_equip(&tctx);
    
    os_gfx_init();
    printf("OS graphics initialized\n");
    
    renderer_init();
    printf("Renderer initialized\n");
    
    font_init();
    font_cache_init();
    
    String font_path = str_lit("/System/Library/Fonts/Helvetica.ttc");
    Font_Tag default_font = font_tag_from_path(font_path);
    
    if (font_tag_equal(default_font, font_tag_zero())) {
        printf("Warning: Could not load font, using zero tag\n");
        default_font = font_tag_zero();
    }
    
    OS_Window_Params window_params = {}; 
    window_params.size = (Vec2_s32){800, 600};
    window_params.title = str_lit("Engine Test");
    
    OS_Handle window = os_window_open_params(window_params);
    if (os_handle_is_zero(window)) {
        printf("Failed to create window\n");
        return 1;
    }
    printf("Window created\n");
    
    void *native_window = os_window_native_handle(window);
    Renderer_Handle window_equip = renderer_window_equip(native_window);
    printf("Window equipped with renderer\n");
    
    Point3D cube_vertices[8] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
    };
    
    int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    
    b32 running = 1;
    f32 rotation_x = 0.0f;
    f32 rotation_y = 0.0f;
    f32 rotation_z = 0.0f;
    f32 fps = 0.0f;
    
    b32 ui_box_dragging = 0;
    Vec2_f32 ui_box_pos = {{10, 10}};
    Vec2_f32 ui_box_size = {{140, 90}};
    Vec2_f32 drag_offset = {{0, 0}};
    Vec2_f32 mouse_pos = {{0, 0}};
    
    f64 current_time = os_get_time();
    f64 last_time = current_time;
    f64 delta_time = 0.0;
    
    f64 fps_update_time = current_time;
    u64 fps_frame_count = 0;
    
    while (running) {
        last_time = current_time;
        current_time = os_get_time();
        delta_time = current_time - last_time;
        
        f32 dpi_scale = os_window_get_dpi_scale(window);
        Rng2_f32 window_rect = os_client_rect_from_window(window);
        f32 window_width = window_rect.max.x - window_rect.min.x;
        f32 window_height = window_rect.max.y - window_rect.min.y;
        
        fps_frame_count++;
        
        rotation_x += 50.0f * delta_time;
        rotation_y += 30.0f * delta_time;
        rotation_z += 20.0f * delta_time;
        
        f64 fps_delta = current_time - fps_update_time;
        if (fps_delta >= 0.25) {
            fps = (f32)(fps_frame_count / fps_delta);
            fps_frame_count = 0;
            fps_update_time = current_time;
        }
        
        OS_Event_List events = os_event_list_from_window(window);
        for (OS_Event *event = events.first; event; event = event->next) {
            if (event->kind == OS_Event_Window_Close) {
                running = 0;
            }
            else if (event->kind == OS_Event_Press && event->key == OS_Key_Esc) {
                running = 0;
            }
            else if (event->kind == OS_Event_Press && event->key == OS_Key_MouseLeft) {
                mouse_pos = event->position;
                if (mouse_pos.x >= ui_box_pos.x && mouse_pos.x <= ui_box_pos.x + ui_box_size.x &&
                    mouse_pos.y >= ui_box_pos.y && mouse_pos.y <= ui_box_pos.y + ui_box_size.y) {
                    ui_box_dragging = 1;
                    drag_offset.x = mouse_pos.x - ui_box_pos.x;
                    drag_offset.y = mouse_pos.y - ui_box_pos.y;
                }
            }
            else if (event->kind == OS_Event_Release && event->key == OS_Key_MouseLeft) {
                ui_box_dragging = 0;
            }
            
            if (event->position.x != 0 || event->position.y != 0) {
                mouse_pos = event->position;
            }
        }
        
        if (ui_box_dragging) {
            ui_box_pos.x = mouse_pos.x - drag_offset.x;
            ui_box_pos.y = mouse_pos.y - drag_offset.y;
            
            if (ui_box_pos.x < 0) ui_box_pos.x = 0;
            if (ui_box_pos.y < 0) ui_box_pos.y = 0;
            if (ui_box_pos.x + ui_box_size.x > window_width) ui_box_pos.x = window_width - ui_box_size.x;
            if (ui_box_pos.y + ui_box_size.y > window_height) ui_box_pos.y = window_height - ui_box_size.y;
        }
        
        renderer_window_begin_frame(native_window, window_equip);
        
        draw_begin_frame(default_font);
        
        Draw_Bucket *bucket = draw_bucket_make();
        draw_push_bucket(bucket);
        
        Vec4_f32 ui_color = ui_box_dragging ? 
            (Vec4_f32){{1.0f, 0.4f, 0.4f, 1.0f}} :
            (Vec4_f32){{1.0f, 0.2f, 0.2f, 1.0f}};
        
        draw_rect((Rng2_f32){{{ui_box_pos.x, ui_box_pos.y}}, 
                            {{ui_box_pos.x + ui_box_size.x, ui_box_pos.y + ui_box_size.y}}}, 
                  ui_color, 
                  10.0f, 2.0f, 1.0f);
        
        draw_text((Vec2_f32){{ui_box_pos.x + 20, ui_box_pos.y + 45}}, 
                  str_lit("Drag me!"), default_font, 18.0f,
                  (Vec4_f32){{1.0f, 1.0f, 1.0f, 1.0f}});
        
        char fps_buffer[64];
        snprintf(fps_buffer, sizeof(fps_buffer), "FPS: %.1f", fps);
        String fps_str = string8_from_cstr(fps_buffer);

        draw_text((Vec2_f32){{window_width/2 - 50, window_height/2 - 10}}, fps_str, default_font, 32.0f, 
                  (Vec4_f32){{1.0f, 1.0f, 1.0f, 1.0f}});
        
        draw_text((Vec2_f32){{window_width/2 - 80, window_height/2 + 20}}, str_lit("Draw API Test"), default_font, 24.0f,
                  (Vec4_f32){{0.8f, 0.8f, 1.0f, 1.0f}});
        
        f32 viewport_x = window_width / 2.0f;
        f32 viewport_width = window_width - viewport_x;
        f32 viewport_height = window_height / 2.0f;
        
        draw_rect((Rng2_f32){{{viewport_x, 0}}, {{window_width, viewport_height}}}, 
                  (Vec4_f32){{0.05f, 0.05f, 0.1f, 1.0f}}, 
                  0.0f, 0.0f, 0.0f);
        
        f32 center_x = viewport_x + viewport_width / 2.0f;
        f32 center_y = viewport_height / 2.0f;
        f32 scale = 80.0f;
        
        Point2D projected[8];
        for (int i = 0; i < 8; i++) {
            Point3D rotated = rotate_point(cube_vertices[i], rotation_x, rotation_y, rotation_z);
            projected[i] = project_point(rotated, center_x, center_y, scale);
        }
        
        for (int i = 0; i < 12; i++) {
            int v1 = edges[i][0];
            int v2 = edges[i][1];
            
            Vec4_f32 edge_color = {{
                0.5f + 0.5f * sinf((rotation_y + i * 30) * 0.017f),
                0.5f + 0.5f * sinf((rotation_y + i * 30) * 0.017f + 2.094f),
                0.5f + 0.5f * sinf((rotation_y + i * 30) * 0.017f + 4.189f),
                1.0f
            }};
            
            f32 dx = projected[v2].x - projected[v1].x;
            f32 dy = projected[v2].y - projected[v1].y;
            f32 len = sqrtf(dx*dx + dy*dy);
            
            if (len > 0.01f) {
                f32 nx = -dy / len * 1.5f;
                f32 ny = dx / len * 1.5f;
                
                Rng2_f32 line_rect = {
                    {{Min(projected[v1].x, projected[v2].x) - 1, 
                      Min(projected[v1].y, projected[v2].y) - 1}},
                    {{Max(projected[v1].x, projected[v2].x) + 1, 
                      Max(projected[v1].y, projected[v2].y) + 1}}
                };
                
                draw_rect(line_rect, edge_color, 0.0f, 0.0f, 0.5f);
            }
        }
        
        for (int i = 0; i < 8; i++) {
            Vec4_f32 vertex_color = {{
                0.5f + 0.5f * sinf((rotation_y + i * 45) * 0.017f),
                0.5f + 0.5f * sinf((rotation_y + i * 45) * 0.017f + 2.094f),
                0.5f + 0.5f * sinf((rotation_y + i * 45) * 0.017f + 4.189f),
                1.0f
            }};
            
            draw_rect((Rng2_f32){{{projected[i].x - 3, projected[i].y - 3}}, 
                                {{projected[i].x + 3, projected[i].y + 3}}}, 
                      vertex_color, 2.0f, 0.0f, 1.0f);
        }
        
        draw_text((Vec2_f32){{viewport_x + viewport_width/2 - 35, 20}}, str_lit("3D Cube"), default_font, 20.0f,
                  (Vec4_f32){{1.0f, 1.0f, 1.0f, 1.0f}});
        
        draw_rect((Rng2_f32){{{viewport_x - 2, 0}}, {{viewport_x + 2, viewport_height}}}, 
                  (Vec4_f32){{0.3f, 0.3f, 0.3f, 1.0f}}, 
                  0.0f, 0.0f, 0.0f);
        draw_rect((Rng2_f32){{{viewport_x, viewport_height - 2}}, {{window_width, viewport_height + 2}}}, 
                  (Vec4_f32){{0.3f, 0.3f, 0.3f, 1.0f}}, 
                  0.0f, 0.0f, 0.0f);
        
        draw_pop_bucket();
        draw_submit_bucket(native_window, window_equip, bucket);
        draw_end_frame();
        
        renderer_window_end_frame(native_window, window_equip);
    }
    
    renderer_window_unequip(native_window, window_equip);
    os_window_close(window);
    
    printf("Engine shutdown complete\n");
    return 0;
}
