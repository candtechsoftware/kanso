#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "base/base.h"
#include "base/logger.h"
#include "base/string_core.h"
#include "draw/draw.h"
#include "font/font.h"
#include "font/font_cache.h"
#include "os/os.h"
#include "renderer/renderer_core.h"

#include <GLFW/glfw3.h>

#include "base/profiler.h"

struct App
{
    // Window and rendering
    GLFWwindow* window;
    Renderer_Handle window_equip;
    Arena* frame_arena;

    // Resources
    Renderer_Handle white_texture;
    Renderer_Handle cube_vertices;
    Renderer_Handle cube_indices;
    Font_Tag font;
    String fps_string;

    // 3D scene state
    f32 rotation;
    int last_width;
    int last_height;
    Mat4x4<f32> cached_projection;
    Mat4x4<f32> cached_view;

    // FPS tracking
    static constexpr int FPS_HISTORY_SIZE = 60;
    f32 fps_history[FPS_HISTORY_SIZE];
    int fps_history_index;
    int fps_history_count;
    f32 frames;
    struct timespec fps_start_time;
    struct timespec last_frame_time;
    f64 frame_time;
};

App* app = nullptr;

// Forward declaration
static void
app_draw(App* app);

static void
error_callback(int error, const char* description)
{
    log_error("GLFW Error {d}: {s}", error, description);
}

static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void
frame_size_callback(GLFWwindow* window, int width, int height)
{
    // Redraw when window is resized
    app_draw(app);
}

static void
app_draw(App* app)
{
    ZoneScoped;

    // Begin draw frame
    draw_begin_frame(app->font);
    Draw_Bucket* bucket = draw_bucket_make();
    draw_push_bucket(bucket);

    // Get current time
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Calculate frame time
    app->frame_time = (now.tv_sec - app->last_frame_time.tv_sec) +
                      (now.tv_nsec - app->last_frame_time.tv_nsec) / 1000000000.0;
    app->last_frame_time = now;

    app->frames++;

    // Calculate elapsed time since FPS counter start
    f64 elapsed = (now.tv_sec - app->fps_start_time.tv_sec) +
                  (now.tv_nsec - app->fps_start_time.tv_nsec) / 1000000000.0;

    if (elapsed >= 0.1)
    { // Update every 100ms for smoother display
        f32 instant_fps = app->frames / elapsed;

        // Add to rolling average
        app->fps_history[app->fps_history_index] = instant_fps;
        app->fps_history_index = (app->fps_history_index + 1) % App::FPS_HISTORY_SIZE;
        if (app->fps_history_count < App::FPS_HISTORY_SIZE)
        {
            app->fps_history_count++;
        }

        // Calculate average, min, max of actual samples
        f32 avg_fps = 0;
        f32 min_fps = 999999.0f;
        f32 max_fps = 0.0f;
        u32 samples = app->fps_history_count > 0 ? app->fps_history_count : 1;
        for (u32 i = 0; i < samples; i++)
        {
            avg_fps += app->fps_history[i];
            if (app->fps_history[i] < min_fps)
                min_fps = app->fps_history[i];
            if (app->fps_history[i] > max_fps)
                max_fps = app->fps_history[i];
        }
        avg_fps /= (f32)samples;

        // Log every second with more detail
        static f64 log_timer = 0;
        log_timer += elapsed;
        if (log_timer >= 1.0)
        {
            // Calculate frame time in milliseconds
            f32 avg_frame_time = 1000.0f / avg_fps;
            log_info("FPS: {f} (avg: {f}, min: {f}, max: {f}) | Frame time: {f} ms",
                     instant_fps, avg_fps, min_fps, max_fps, avg_frame_time);
            log_timer = 0;
        }

        // Update FPS text periodically
        static f64 fps_update_timer = 0;
        static s64 last_fps = -1;
        fps_update_timer += elapsed;

        f64 current_fps = avg_fps;

        // Update when FPS changes significantly or every second
        if (!(app->font == font_tag_zero()) &&
            (fps_update_timer >= 1.0f || abs(current_fps - (f64)last_fps) > 5))
        {
            fps_update_timer = 0;
            last_fps = (s64)current_fps;

            // Create FPS string
            char fps_buffer[32];
            snprintf(fps_buffer, sizeof(fps_buffer), "FPS: %f", current_fps);
            app->fps_string = push_string_copy(app->frame_arena, cstr_to_string(fps_buffer, strlen(fps_buffer)));
        }

        app->frames = 0;
        app->fps_start_time = now;
    }

    int width, height;
    glfwGetFramebufferSize(app->window, &width, &height);

    {
        ZoneScopedN("Frame Setup");
        renderer_begin_frame();
        renderer_window_begin_frame(app->window, app->window_equip);
    }

    {
        ZoneScopedN("UI Pass Setup");

        // Draw colored rectangles using draw API
        Renderer_Rect_2D_Inst* rect1 = draw_rect({{50, 50}, {200, 150}}, {1, 0, 0, 1}, 10, 2, 0.5);
        if (rect1)
        {
            // Set gradient colors
            rect1->colors[0] = {1, 0, 0, 1};
            rect1->colors[1] = {0, 1, 0, 1};
            rect1->colors[2] = {0, 0, 1, 1};
            rect1->colors[3] = {1, 1, 0, 1};
        }

        draw_rect({{(f32)width - 250.0f, 50}, {(f32)width - 50.0f, 250}}, {0.5f, 0.5f, 1, 0.8f}, 20, 0, 1);

        // Draw FPS text
        if (app->fps_string.size > 0)
        {
            draw_text({10, 10}, app->fps_string, app->font, 32.0f, {1, 1, 1, 1});
        }
    }

    {
        ZoneScopedN("3D Pass Setup");

        // Only recalculate projection matrix if window size changed
        if (width != app->last_width || height != app->last_height)
        {
            f32 aspect = (f32)width / (f32)height;
            app->cached_projection = mat4x4_perspective(3.14159f / 4.0f, aspect, 0.1f, 100.0f);
            app->last_width = width;
            app->last_height = height;
        }

        // Begin 3D rendering
        draw_geo3d_begin({{0, 0}, {(f32)width, (f32)height}}, app->cached_view, app->cached_projection);

        // Draw mesh
        Mat4x4<f32> transform = mat4x4_rotate_y(app->rotation) * mat4x4_scale(1.0f, 1.0f, 1.0f);
        draw_mesh(app->cube_vertices, app->cube_indices,
                  Renderer_Geo_Topology_Kind_Triangles,
                  Renderer_Geo_Vertex_Flag_Tex_Coord | Renderer_Geo_Vertex_Flag_Normals | Renderer_Geo_Vertex_Flag_RGBA,
                  app->white_texture, transform);

        app->rotation += f32(1.0f * app->frame_time); // Rotate at 1 radian per second
    }

    draw_pop_bucket();
    draw_submit_bucket(app->window, app->window_equip, bucket);
    renderer_window_end_frame(app->window, app->window_equip);
    renderer_end_frame();
    draw_end_frame();
}

int
main(void)
{
    log_init("kanso.log");

    Arena* arena = arena_alloc();
    Arena* frame_arena = arena_alloc();

    GLFWwindow* window;

    glfwSetErrorCallback(error_callback);

// TODO(Alex) should not need to do this in a main file
#ifdef USE_WAYLAND
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#endif

    if (!glfwInit())
    {
        return -1;
    }
    App a = {};
    app = &a;
    app->frame_arena = frame_arena;
    app->rotation = 0.0f;
    app->last_width = 0;
    app->last_height = 0;
    app->cached_view = mat4x4_translate(0.0f, 0.0f, -3.0f);
    app->fps_history_index = 0;
    app->fps_history_count = 0;
    app->frames = 0;
    app->frame_time = 0.016667; // Default to 60 FPS frame time

    font_init();
    font_cache_init();

    Font_Tag font = font_tag_from_path(to_string("assets/fonts/NotoSans-VariableFont_wdth,wght.ttf"));
    if ((font == font_tag_zero()))
    {
        log_error("Failed to load font");
    }
    else
    {
        log_info("Font loaded successfully");
        app->font = font;
    }

    // Initialize FPS history
    for (int i = 0; i < App::FPS_HISTORY_SIZE; i++)
    {
        app->fps_history[i] = 60.0f; // Initialize with reasonable value
    }

    // Initialize timing
    clock_gettime(CLOCK_MONOTONIC, &app->fps_start_time);
    app->last_frame_time = app->fps_start_time;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Try to disable refresh rate syncing
    glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);

    // Disable GLFW's internal frame pacing
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_FALSE);

    window = glfwCreateWindow(800, 600, "Renderer Demo", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, frame_size_callback);
    glfwSetKeyCallback(window, key_callback);

    renderer_init();

    Renderer_Handle window_equip = renderer_window_equip(window);

    // Set window and window_equip in app
    app->window = window;
    app->window_equip = window_equip;

    Renderer_Handle white_texture = renderer_handle_zero();
    {
        u32 white_pixel = 0xFFFFFFFF;
        white_texture = renderer_tex_2d_alloc(Renderer_Resource_Kind_Static,
                                              Vec2<f32>{1, 1},
                                              Renderer_Tex_2D_Format_RGBA8,
                                              &white_pixel);
    }
    app->white_texture = white_texture;

    Renderer_Handle cube_vertices = renderer_handle_zero();
    Renderer_Handle cube_indices = renderer_handle_zero();
    {
        struct Vertex
        {
            Vec3<f32> pos;
            Vec2<f32> uv;
            Vec3<f32> normal;
            Vec4<f32> color;
        };

        Vertex vertices[] = {
            // Front face (z = -0.5)
            {{-0.5f, -0.5f, -0.5f}, {0, 0}, {0, 0, -1}, {1, 0, 0, 1}},
            {{0.5f, -0.5f, -0.5f}, {1, 0}, {0, 0, -1}, {0, 1, 0, 1}},
            {{0.5f, 0.5f, -0.5f}, {1, 1}, {0, 0, -1}, {0, 0, 1, 1}},
            {{-0.5f, 0.5f, -0.5f}, {0, 1}, {0, 0, -1}, {1, 1, 0, 1}},

            // Back face (z = 0.5)
            {{-0.5f, -0.5f, 0.5f}, {0, 0}, {0, 0, 1}, {1, 0, 1, 1}},
            {{0.5f, -0.5f, 0.5f}, {1, 0}, {0, 0, 1}, {0, 1, 1, 1}},
            {{0.5f, 0.5f, 0.5f}, {1, 1}, {0, 0, 1}, {1, 1, 1, 1}},
            {{-0.5f, 0.5f, 0.5f}, {0, 1}, {0, 0, 1}, {0.5f, 0.5f, 0.5f, 1}},

            // Left face (x = -0.5)
            {{-0.5f, -0.5f, -0.5f}, {0, 0}, {-1, 0, 0}, {1, 0, 0, 1}},
            {{-0.5f, 0.5f, -0.5f}, {1, 0}, {-1, 0, 0}, {1, 1, 0, 1}},
            {{-0.5f, 0.5f, 0.5f}, {1, 1}, {-1, 0, 0}, {0.5f, 0.5f, 0.5f, 1}},
            {{-0.5f, -0.5f, 0.5f}, {0, 1}, {-1, 0, 0}, {1, 0, 1, 1}},

            // Right face (x = 0.5)
            {{0.5f, -0.5f, -0.5f}, {0, 0}, {1, 0, 0}, {0, 1, 0, 1}},
            {{0.5f, -0.5f, 0.5f}, {1, 0}, {1, 0, 0}, {0, 1, 1, 1}},
            {{0.5f, 0.5f, 0.5f}, {1, 1}, {1, 0, 0}, {1, 1, 1, 1}},
            {{0.5f, 0.5f, -0.5f}, {0, 1}, {1, 0, 0}, {0, 0, 1, 1}},

            // Top face (y = 0.5)
            {{-0.5f, 0.5f, -0.5f}, {0, 0}, {0, 1, 0}, {1, 1, 0, 1}},
            {{0.5f, 0.5f, -0.5f}, {1, 0}, {0, 1, 0}, {0, 0, 1, 1}},
            {{0.5f, 0.5f, 0.5f}, {1, 1}, {0, 1, 0}, {1, 1, 1, 1}},
            {{-0.5f, 0.5f, 0.5f}, {0, 1}, {0, 1, 0}, {0.5f, 0.5f, 0.5f, 1}},

            // Bottom face (y = -0.5)
            {{-0.5f, -0.5f, -0.5f}, {0, 0}, {0, -1, 0}, {1, 0, 0, 1}},
            {{-0.5f, -0.5f, 0.5f}, {1, 0}, {0, -1, 0}, {1, 0, 1, 1}},
            {{0.5f, -0.5f, 0.5f}, {1, 1}, {0, -1, 0}, {0, 1, 1, 1}},
            {{0.5f, -0.5f, -0.5f}, {0, 1}, {0, -1, 0}, {0, 1, 0, 1}},
        };

        u32 indices[] = {
            // Front face
            0, 1, 2, 2, 3, 0,
            // Back face
            4, 7, 6, 6, 5, 4,
            // Left face
            8, 9, 10, 10, 11, 8,
            // Right face
            12, 13, 14, 14, 15, 12,
            // Top face
            16, 17, 18, 18, 19, 16,
            // Bottom face
            20, 21, 22, 22, 23, 20};

        cube_vertices = renderer_buffer_alloc(Renderer_Resource_Kind_Static,
                                              sizeof(vertices), vertices);
        cube_indices = renderer_buffer_alloc(Renderer_Resource_Kind_Static,
                                             sizeof(indices), indices);
    }
    app->cube_vertices = cube_vertices;
    app->cube_indices = cube_indices;

    while (!glfwWindowShouldClose(window))
    {
        arena_clear(app->frame_arena);
        app_draw(app);
        {
            ZoneScopedN("Frame Submit");
        }

        {
            ZoneScopedN("Poll Events");
            glfwPollEvents();
        }

        FrameMark;
    }

    renderer_tex_2d_release(white_texture);
    renderer_buffer_release(cube_vertices);
    renderer_buffer_release(cube_indices);
    renderer_window_unequip(window, window_equip);

    glfwDestroyWindow(window);
    glfwTerminate();

    arena_release(frame_arena);
    arena_release(arena);

    log_info("Application shutting down");
    log_shutdown();

    return 0;
}
