#ifndef RENDERER_METAL_H
#define RENDERER_METAL_H

#include "../base/arena.h"
#include "../base/base.h"
#include "renderer_core.h"

#ifdef __OBJC__
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
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
    void* texture; // id<MTLTexture>
    void* sampler; // id<MTLSamplerState>
    Vec2<f32> size;
    Renderer_Tex_2D_Format format;
    Renderer_Resource_Kind kind;
};

struct Renderer_Metal_Buffer
{
    void* buffer; // id<MTLBuffer>
    u64 size;
    Renderer_Resource_Kind kind;
};

struct Renderer_Metal_Window_Equip
{
    void* layer; // CAMetalLayer*
    void* depth_texture; // id<MTLTexture>
    Vec2<f32> size;
};

struct Renderer_Metal_Pipeline
{
    void* render_pipeline_state; // id<MTLRenderPipelineState>
    void* depth_stencil_state; // id<MTLDepthStencilState>
};

// Frame synchronization
// Note: 2 frames provides lower latency and can achieve higher FPS
// 3 frames may be limited by drawable availability
#define METAL_FRAMES_IN_FLIGHT 2

// Buffer pool for reducing allocations
#define METAL_BUFFER_POOL_SIZE 16

struct Renderer_Metal_Buffer_Pool_Entry
{
    void* buffer; // id<MTLBuffer>
    u64 size;
    b32 in_use;
};

struct Renderer_Metal_Frame_Data
{
    void* semaphore; // dispatch_semaphore_t
    void* command_buffer; // id<MTLCommandBuffer> - pre-allocated
    void* rect_instance_buffer; // id<MTLBuffer>
    u64 rect_instance_buffer_size;
    void* mesh_uniform_buffer; // id<MTLBuffer>
    
    // Buffer pool for temporary allocations
    Renderer_Metal_Buffer_Pool_Entry buffer_pool[METAL_BUFFER_POOL_SIZE];
};

struct Renderer_Metal_State
{
    Arena* arena;
    
    void* device; // id<MTLDevice>
    void* command_queue; // id<MTLCommandQueue>
    void* library; // id<MTLLibrary>
    
    Renderer_Metal_Pipeline pipelines[Renderer_Metal_Shader_Kind_COUNT];
    
    // Frame data for triple buffering
    Renderer_Metal_Frame_Data frames[METAL_FRAMES_IN_FLIGHT];
    u32 current_frame_index;
    
    // Rect rendering resources
    void* rect_vertex_buffer; // id<MTLBuffer>
    
    // Blur pass cached resources
    void* blur_temp_texture; // id<MTLTexture>
    Vec2<f32> blur_temp_texture_size;
    void* blur_sampler; // id<MTLSamplerState>
    
    // Cached render pass descriptors
    void* ui_render_pass_desc; // MTLRenderPassDescriptor*
    void* blur_render_pass_desc; // MTLRenderPassDescriptor*
    void* geo_render_pass_desc; // MTLRenderPassDescriptor*
    
    // Resource storage
    Renderer_Metal_Tex_2D* textures;
    u64 texture_count;
    u64 texture_cap;
    
    Renderer_Metal_Buffer* buffers;
    u64 buffer_count;
    u64 buffer_cap;
    
    Renderer_Metal_Window_Equip* window_equips;
    u64 window_equip_count;
    u64 window_equip_cap;
};

extern Renderer_Metal_State* r_metal_state;

// Shader creation
void
renderer_metal_init_shaders();

// Texture format conversion
#ifdef __OBJC__
MTLPixelFormat
renderer_metal_pixel_format_from_tex_2d_format(Renderer_Tex_2D_Format fmt);
#endif

Mat4x4<f32>
renderer_metal_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format fmt);

// Render pass implementations
void
renderer_metal_render_pass_ui(Renderer_Pass_Params_UI* params, void* command_buffer, void* target_texture);
void
renderer_metal_render_pass_blur(Renderer_Pass_Params_Blur* params, void* command_buffer, void* target_texture);
void
renderer_metal_render_pass_geo_3d(Renderer_Pass_Params_Geo_3D* params, void* command_buffer, void* target_texture, void* depth_texture);

// Combined render pass for better performance
void
renderer_metal_render_pass_combined(Renderer_Pass_Params_UI* ui_params, 
                                   Renderer_Pass_Params_Geo_3D* geo_params,
                                   void* command_buffer, 
                                   void* target_texture, 
                                   void* depth_texture);

#endif // RENDERER_METAL_H