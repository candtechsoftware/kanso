#pragma once

#include "../base/base_inc.h"
#include "renderer_core.h"

#ifdef __OBJC__
// Temporarily undefine our macros that conflict with system headers
#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global

#    import <Metal/Metal.h>
#    import <MetalKit/MetalKit.h>
#    import <QuartzCore/CAMetalLayer.h>

// Restore our macros
#pragma pop_macro("global")
#pragma pop_macro("internal")
#endif

enum Renderer_Metal_Shader_Kind
{
    Renderer_Metal_Shader_Kind_Rect,
    Renderer_Metal_Shader_Kind_Blur,
    Renderer_Metal_Shader_Kind_Mesh,
    Renderer_Metal_Shader_Kind_COUNT,
};

struct Renderer_Metal_Tex_2D
{
    void                  *texture;
    void                  *sampler;
    Vec2<f32>              size;
    Renderer_Tex_2D_Format format;
    Renderer_Resource_Kind kind;
};

struct Renderer_Metal_Buffer
{
    void                  *buffer;
    u64                    size;
    Renderer_Resource_Kind kind;
};

struct Renderer_Metal_Window_Equip
{
    void     *layer;
    void     *depth_texture;
    Vec2<f32> size;
};

struct Renderer_Metal_Pipeline
{
    void *render_pipeline_state;
    void *depth_stencil_state;
};

#define METAL_FRAMES_IN_FLIGHT          2
#define METAL_BUFFER_POOL_SIZE          16
#define METAL_BUFFER_POOL_MIN_SIZE      (16 * 1024)
#define METAL_BUFFER_POOL_GROWTH_FACTOR 2

struct Renderer_Metal_Buffer_Pool_Entry
{
    void *buffer;
    u64   size;
    u64   used_size;
    b32   in_use;
    u32   reuse_count;
};

struct Renderer_Metal_Frame_Data
{
    void *semaphore;
    void *command_buffer;
    void *rect_instance_buffer;
    u64   rect_instance_buffer_size;
    u64   rect_instance_buffer_used;
    void *mesh_uniform_buffer;
    u64   mesh_uniform_buffer_used;

    Renderer_Metal_Buffer_Pool_Entry buffer_pool[METAL_BUFFER_POOL_SIZE];
    u32                              buffer_pool_hit_count;
    u32                              buffer_pool_miss_count;
};

struct Renderer_Metal_State
{
    Arena *arena;

    void *device;
    void *command_queue;
    void *library;

    Renderer_Metal_Pipeline pipelines[Renderer_Metal_Shader_Kind_COUNT];

    Renderer_Metal_Frame_Data frames[METAL_FRAMES_IN_FLIGHT];
    u32                       current_frame_index;

    void *rect_vertex_buffer;

    void     *blur_temp_texture;
    Vec2<f32> blur_temp_texture_size;
    void     *blur_sampler;

    void *ui_render_pass_desc;
    void *blur_render_pass_desc;
    void *geo_render_pass_desc;

    Renderer_Metal_Tex_2D *textures;
    u64                    texture_count;
    u64                    texture_cap;

    Renderer_Metal_Buffer *buffers;
    u64                    buffer_count;
    u64                    buffer_cap;

    Renderer_Metal_Window_Equip *window_equips;
    u64                          window_equip_count;
    u64                          window_equip_cap;
};

extern Renderer_Metal_State *r_metal_state;

void
renderer_metal_init_shaders();
#ifdef __OBJC__
MTLPixelFormat
renderer_metal_pixel_format_from_tex_2d_format(Renderer_Tex_2D_Format fmt);
#endif

Mat4x4<f32>
renderer_metal_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format fmt);

void
renderer_metal_render_pass_ui(Renderer_Pass_Params_UI *params, void *command_buffer, void *target_texture);
void
renderer_metal_render_pass_blur(Renderer_Pass_Params_Blur *params, void *command_buffer, void *target_texture);
void
renderer_metal_render_pass_geo_3d(Renderer_Pass_Params_Geo_3D *params, void *command_buffer, void *target_texture, void *depth_texture);

void
renderer_metal_render_pass_combined(Renderer_Pass_Params_UI     *ui_params,
                                    Renderer_Pass_Params_Geo_3D *geo_params,
                                    void                        *command_buffer,
                                    void                        *target_texture,
                                    void                        *depth_texture);

void *
renderer_metal_acquire_buffer_from_pool(u64 size);

void
renderer_metal_reset_frame_pools();

#endif