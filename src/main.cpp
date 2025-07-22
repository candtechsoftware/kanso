#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "base/base.h"
#include "base/logger.h"
#include "base/string_core.h"
#include "font/font.h"
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
        int samples = app->fps_history_count > 0 ? app->fps_history_count : 1;
        for (int i = 0; i < samples; i++)
        {
            avg_fps += app->fps_history[i];
            if (app->fps_history[i] < min_fps)
                min_fps = app->fps_history[i];
            if (app->fps_history[i] > max_fps)
                max_fps = app->fps_history[i];
        }
        avg_fps /= samples;

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

        app->frames = 0;
        app->fps_start_time = now;
    }
    arena_clear(app->frame_arena);

    int width, height;
    glfwGetFramebufferSize(app->window, &width, &height);

    {
        ZoneScopedN("Frame Setup");
        renderer_begin_frame();
        renderer_window_begin_frame(app->window, app->window_equip);
    }

    Renderer_Pass_List passes = list_make<Renderer_Pass>();

    {
        ZoneScopedN("UI Pass Setup");
        Renderer_Pass* ui_pass = renderer_pass_from_kind(app->frame_arena, &passes, Renderer_Pass_Kind_UI);
        ui_pass->params_ui->rects = list_make<Renderer_Batch_Group_2D_Node>();

        Renderer_Batch_Group_2D_Node* group = list_push_new(app->frame_arena, &ui_pass->params_ui->rects);
        group->params.tex = app->white_texture;
        group->params.tex_sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
        group->params.xform = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
        group->params.clip = {{0, 0}, {(f32)width, (f32)height}};
        group->params.transparency = 0.0f;
        group->batches = renderer_batch_list_make(sizeof(Renderer_Rect_2D_Inst));

        Renderer_Rect_2D_Inst* rect1 = (Renderer_Rect_2D_Inst*)
            renderer_batch_list_push_inst(app->frame_arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);
        rect1->dst = {{50, 50}, {200, 150}};
        rect1->src = {{0, 0}, {1, 1}};
        rect1->colors[0] = {1, 0, 0, 1};
        rect1->colors[1] = {0, 1, 0, 1};
        rect1->colors[2] = {0, 0, 1, 1};
        rect1->colors[3] = {1, 1, 0, 1};
        rect1->corner_radii[0] = 10;
        rect1->corner_radii[1] = 10;
        rect1->corner_radii[2] = 10;
        rect1->corner_radii[3] = 10;
        rect1->border_thickness = 2;
        rect1->edge_softness = 0.5;
        rect1->white_texture_override = 1;

        Renderer_Rect_2D_Inst* rect2 = (Renderer_Rect_2D_Inst*)
            renderer_batch_list_push_inst(app->frame_arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);
        rect2->dst = {{width - 250.0f, 50}, {width - 50.0f, 250}};
        rect2->src = {{0, 0}, {1, 1}};
        rect2->colors[0] = {0.5f, 0.5f, 1, 0.8f};
        rect2->colors[1] = {0.5f, 0.5f, 1, 0.8f};
        rect2->colors[2] = {0.5f, 0.5f, 1, 0.8f};
        rect2->colors[3] = {0.5f, 0.5f, 1, 0.8f};
        rect2->corner_radii[0] = 20;
        rect2->corner_radii[1] = 20;
        rect2->corner_radii[2] = 20;
        rect2->corner_radii[3] = 20;
        rect2->border_thickness = 0;
        rect2->edge_softness = 1;
        rect2->white_texture_override = 1;
    }

    {
        ZoneScopedN("3D Pass Setup");
        Renderer_Pass* geo_pass = renderer_pass_from_kind(app->frame_arena, &passes, Renderer_Pass_Kind_Geo_3D);
        geo_pass->params_geo_3d->viewport = {{0, 0}, {(f32)width, (f32)height}};
        geo_pass->params_geo_3d->clip = {{0, 0}, {(f32)width, (f32)height}};

        // Only recalculate projection matrix if window size changed
        if (width != app->last_width || height != app->last_height)
        {
            f32 aspect = (f32)width / (f32)height;
            app->cached_projection = mat4x4_perspective(3.14159f / 4.0f, aspect, 0.1f, 100.0f);
            app->last_width = width;
            app->last_height = height;
        }

        geo_pass->params_geo_3d->projection = app->cached_projection;
        geo_pass->params_geo_3d->view = app->cached_view;

        geo_pass->params_geo_3d->mesh_batches.slots_count = 16;
        geo_pass->params_geo_3d->mesh_batches.slots = push_array_zero(app->frame_arena, Renderer_Batch_Group_3D_Map_Node*, 16);

        Renderer_Batch_Group_3D_Map_Node* mesh_node = push_struct_zero(app->frame_arena, Renderer_Batch_Group_3D_Map_Node);
        mesh_node->hash = 1;
        mesh_node->params.mesh_vertices = app->cube_vertices;
        mesh_node->params.mesh_indices = app->cube_indices;
        mesh_node->params.mesh_geo_topology = Renderer_Geo_Topology_Kind_Triangles;
        mesh_node->params.mesh_geo_vertex_flags = Renderer_Geo_Vertex_Flag_Tex_Coord |
                                                  Renderer_Geo_Vertex_Flag_Normals |
                                                  Renderer_Geo_Vertex_Flag_RGBA;
        mesh_node->params.albedo_tex = app->white_texture;
        mesh_node->params.albedo_tex_sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
        mesh_node->params.xform = mat4x4_identity<f32>();
        mesh_node->batches = renderer_batch_list_make(sizeof(Renderer_Mesh_3D_Inst));

        geo_pass->params_geo_3d->mesh_batches.slots[0] = mesh_node;

        Renderer_Mesh_3D_Inst* inst = (Renderer_Mesh_3D_Inst*)
            renderer_batch_list_push_inst(app->frame_arena, &mesh_node->batches, sizeof(Renderer_Mesh_3D_Inst), 16);
        inst->xform = mat4x4_rotate_y(app->rotation) * mat4x4_scale(1.0f, 1.0f, 1.0f);

        app->rotation += 1.0f * app->frame_time; // Rotate at 1 radian per second
    }

    renderer_window_submit(app->window, app->window_equip, &passes);
    renderer_window_end_frame(app->window, app->window_equip);
    renderer_end_frame();
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

    font_open(to_string("/System/Library/Fonts/Menlo.ttc"));

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
