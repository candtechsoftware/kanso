#include <chrono>
#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#    define GL_SILENCE_DEPRECATION
#    include <OpenGL/gl3.h>
#endif

#include <GLFW/glfw3.h>

#include "base/base.h"
#include "os/os.h"
#include "renderer/renderer_core.h"

static void
error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void
test_simd_performance()
{
    printf("\n=== SIMD Matrix Multiplication Performance Test ===\n");

    Mat4x4<f32> a = {{{1, 2, 3, 4},
                      {5, 6, 7, 8},
                      {9, 10, 11, 12},
                      {13, 14, 15, 16}}};

    Mat4x4<f32> b = {{{16, 15, 14, 13},
                      {12, 11, 10, 9},
                      {8, 7, 6, 5},
                      {4, 3, 2, 1}}};

    const int iterations = 1000000;

    auto start = std::chrono::high_resolution_clock::now();
    Mat4x4<f32> result;
    for (int i = 0; i < iterations; i++)
    {
        result = a * b;
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    printf("SIMD 4x4 multiplication: %lld microseconds for %d iterations\n",
           duration.count(), iterations);
    printf("Average per operation: %.3f nanoseconds\n",
           (duration.count() * 1000.0) / iterations);

    printf("\n4x4 Result:\n");
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            printf("%6.1f ", result.m[i][j]);
        }
        printf("\n");
    }

    Mat3x3<f32> a3 = {{{1, 2, 3},
                       {4, 5, 6},
                       {7, 8, 9}}};

    Mat3x3<f32> b3 = {{{9, 8, 7},
                       {6, 5, 4},
                       {3, 2, 1}}};

    start = std::chrono::high_resolution_clock::now();
    Mat3x3<f32> result3;
    for (int i = 0; i < iterations; i++)
    {
        result3 = a3 * b3;
    }
    end = std::chrono::high_resolution_clock::now();

    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    printf("\nSIMD 3x3 multiplication: %lld microseconds for %d iterations\n",
           duration.count(), iterations);
    printf("Average per operation: %.3f nanoseconds\n",
           (duration.count() * 1000.0) / iterations);

    printf("\n3x3 Result:\n");
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            printf("%6.1f ", result3.m[i][j]);
        }
        printf("\n");
    }
}

int
main(void)
{
    Arena* arena = arena_alloc();
    Arena* frame_arena = arena_alloc();

    test_simd_performance();

    GLFWwindow* window;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
    {
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(800, 600, "Renderer Demo", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    renderer_init();

    Renderer_Handle window_equip = renderer_window_equip(window);

    Renderer_Handle white_texture = renderer_handle_zero();
    {
        u32 white_pixel = 0xFFFFFFFF;
        white_texture = renderer_tex_2d_alloc(Renderer_Resource_Kind_Static,
                                              Vec2<f32>{1, 1},
                                              Renderer_Tex_2D_Format_RGBA8,
                                              &white_pixel);
    }

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
            {{-0.5f, -0.5f, -0.5f}, {0, 0}, {0, 0, -1}, {1, 0, 0, 1}},
            {{0.5f, -0.5f, -0.5f}, {1, 0}, {0, 0, -1}, {0, 1, 0, 1}},
            {{0.5f, 0.5f, -0.5f}, {1, 1}, {0, 0, -1}, {0, 0, 1, 1}},
            {{-0.5f, 0.5f, -0.5f}, {0, 1}, {0, 0, -1}, {1, 1, 0, 1}},

            {{-0.5f, -0.5f, 0.5f}, {0, 0}, {0, 0, 1}, {1, 0, 1, 1}},
            {{0.5f, -0.5f, 0.5f}, {1, 0}, {0, 0, 1}, {0, 1, 1, 1}},
            {{0.5f, 0.5f, 0.5f}, {1, 1}, {0, 0, 1}, {1, 1, 1, 1}},
            {{-0.5f, 0.5f, 0.5f}, {0, 1}, {0, 0, 1}, {0.5f, 0.5f, 0.5f, 1}},
        };

        u32 indices[] = {
            0, 1, 2, 2, 3, 0, // Front face (CCW)
            4, 7, 6, 6, 5, 4, // Back face (CCW when viewed from front)
            0, 3, 7, 7, 4, 0, // Left face
            1, 5, 6, 6, 2, 1, // Right face
            3, 2, 6, 6, 7, 3, // Top face
            0, 4, 5, 5, 1, 0  // Bottom face
        };

        cube_vertices = renderer_buffer_alloc(Renderer_Resource_Kind_Static,
                                              sizeof(vertices), vertices);
        cube_indices = renderer_buffer_alloc(Renderer_Resource_Kind_Static,
                                             sizeof(indices), indices);
    }

    f32 rotation = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        arena_clear(frame_arena);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        renderer_begin_frame();
        renderer_window_begin_frame(window, window_equip);

        Renderer_Pass_List passes = list_make<Renderer_Pass>();

        {
            Renderer_Pass* ui_pass = renderer_pass_from_kind(frame_arena, &passes, Renderer_Pass_Kind_UI);
            ui_pass->params_ui->rects = list_make<Renderer_Batch_Group_2D_Node>();

            Renderer_Batch_Group_2D_Node* group = list_push_new(frame_arena, &ui_pass->params_ui->rects);
            group->params.tex = white_texture;
            group->params.tex_sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
            group->params.xform = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
            group->params.clip = {{0, 0}, {(f32)width, (f32)height}};
            group->params.transparency = 0.0f;
            group->batches = renderer_batch_list_make(sizeof(Renderer_Rect_2D_Inst));

            Renderer_Rect_2D_Inst* rect1 = (Renderer_Rect_2D_Inst*)
                renderer_batch_list_push_inst(frame_arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);
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
            rect1->edge_softness = 1;
            rect1->white_texture_override = 1;

            Renderer_Rect_2D_Inst* rect2 = (Renderer_Rect_2D_Inst*)
                renderer_batch_list_push_inst(frame_arena, &group->batches, sizeof(Renderer_Rect_2D_Inst), 256);
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
            rect2->edge_softness = 2;
            rect2->white_texture_override = 1;
        }

        {
            Renderer_Pass* geo_pass = renderer_pass_from_kind(frame_arena, &passes, Renderer_Pass_Kind_Geo_3D);
            geo_pass->params_geo_3d->viewport = {{0, 0}, {(f32)width, (f32)height}};
            geo_pass->params_geo_3d->clip = {{0, 0}, {(f32)width, (f32)height}};

            f32 aspect = (f32)width / (f32)height;
            geo_pass->params_geo_3d->projection = mat4x4_perspective(3.14159f / 4.0f, aspect, 0.1f, 100.0f);
            geo_pass->params_geo_3d->view = mat4x4_translate(0.0f, 0.0f, -3.0f);

            geo_pass->params_geo_3d->mesh_batches.slots_count = 16;
            geo_pass->params_geo_3d->mesh_batches.slots = push_array_zero(frame_arena, Renderer_Batch_Group_3D_Map_Node*, 16);

            Renderer_Batch_Group_3D_Map_Node* mesh_node = push_struct_zero(frame_arena, Renderer_Batch_Group_3D_Map_Node);
            mesh_node->hash = 1;
            mesh_node->params.mesh_vertices = cube_vertices;
            mesh_node->params.mesh_indices = cube_indices;
            mesh_node->params.mesh_geo_topology = Renderer_Geo_Topology_Kind_Triangles;
            mesh_node->params.mesh_geo_vertex_flags = Renderer_Geo_Vertex_Flag_Tex_Coord |
                                                      Renderer_Geo_Vertex_Flag_Normals |
                                                      Renderer_Geo_Vertex_Flag_RGBA;
            mesh_node->params.albedo_tex = white_texture;
            mesh_node->params.albedo_tex_sample_kind = Renderer_Tex_2D_Sample_Kind_Linear;
            mesh_node->params.xform = mat4x4_identity<f32>();
            mesh_node->batches = renderer_batch_list_make(sizeof(Renderer_Mesh_3D_Inst));

            geo_pass->params_geo_3d->mesh_batches.slots[0] = mesh_node;

            Renderer_Mesh_3D_Inst* inst = (Renderer_Mesh_3D_Inst*)
                renderer_batch_list_push_inst(frame_arena, &mesh_node->batches, sizeof(Renderer_Mesh_3D_Inst), 16);
            inst->xform = mat4x4_rotate_y(rotation) * mat4x4_scale(1.0f, 1.0f, 1.0f);

            rotation += 0.01f;
        }

        renderer_window_submit(window, window_equip, &passes);
        renderer_window_end_frame(window, window_equip);
        renderer_end_frame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer_tex_2d_release(white_texture);
    renderer_buffer_release(cube_vertices);
    renderer_buffer_release(cube_indices);
    renderer_window_unequip(window, window_equip);

    glfwDestroyWindow(window);
    glfwTerminate();

    arena_release(frame_arena);
    arena_release(arena);

    return 0;
}