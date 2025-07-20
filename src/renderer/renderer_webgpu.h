#ifndef RENDERER_WEBGPU_H
#define RENDERER_WEBGPU_H

#include "../base/arena.h"
#include "../base/base.h"
#include "renderer_core.h"

#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>

enum Renderer_WebGPU_Shader_Kind
{
    Renderer_WebGPU_Shader_Kind_Rect,
    Renderer_WebGPU_Shader_Kind_Blur,
    Renderer_WebGPU_Shader_Kind_Mesh,
    Renderer_WebGPU_Shader_Kind_COUNT,
};

struct Renderer_WebGPU_Tex_2D
{
    wgpu::Texture texture;
    wgpu::TextureView view;
    wgpu::Sampler sampler;
    Vec2<f32> size;
    Renderer_Tex_2D_Format format;
    Renderer_Resource_Kind kind;
};

struct Renderer_WebGPU_Buffer
{
    wgpu::Buffer buffer;
    u64 size;
    Renderer_Resource_Kind kind;
};

struct Renderer_WebGPU_Window_Equip
{
    wgpu::Surface surface;
    wgpu::Texture depth_texture;
    wgpu::TextureView depth_texture_view;
    Vec2<f32> size;
};

struct Renderer_WebGPU_Pipeline
{
    wgpu::RenderPipeline render_pipeline;
    wgpu::BindGroupLayout bind_group_layout;
    wgpu::PipelineLayout pipeline_layout;
};

struct Renderer_WebGPU_State
{
    Arena* arena;

    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;

    Renderer_WebGPU_Pipeline pipelines[Renderer_WebGPU_Shader_Kind_COUNT];

    // Rect rendering resources
    wgpu::Buffer rect_vertex_buffer;
    wgpu::Buffer rect_instance_buffer;
    u64 rect_instance_buffer_size;

    // Mesh rendering resources
    wgpu::Buffer mesh_uniform_buffer;

    // Resource storage
    Renderer_WebGPU_Tex_2D* textures;
    u64 texture_count;
    u64 texture_cap;

    Renderer_WebGPU_Buffer* buffers;
    u64 buffer_count;
    u64 buffer_cap;

    Renderer_WebGPU_Window_Equip* window_equips;
    u64 window_equip_count;
    u64 window_equip_cap;
};

extern Renderer_WebGPU_State* r_wgpu_state;

// Shader creation
void
renderer_webgpu_init_shaders();
wgpu::ShaderModule
renderer_webgpu_create_shader_module(const char* source);

// Texture format conversion
wgpu::TextureFormat
renderer_webgpu_texture_format_from_tex_2d_format(Renderer_Tex_2D_Format fmt);
Mat4x4<f32>
renderer_webgpu_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format fmt);

// Render pass implementations
void
renderer_webgpu_render_pass_ui(Renderer_Pass_Params_UI* params, wgpu::CommandEncoder encoder, wgpu::TextureView target_view);
void
renderer_webgpu_render_pass_blur(Renderer_Pass_Params_Blur* params, wgpu::CommandEncoder encoder, wgpu::TextureView target_view);
void
renderer_webgpu_render_pass_geo_3d(Renderer_Pass_Params_Geo_3D* params, wgpu::CommandEncoder encoder, wgpu::TextureView target_view);

// Shader sources
extern const char* renderer_webgpu_rect_shader_src;
extern const char* renderer_webgpu_blur_shader_src;
extern const char* renderer_webgpu_mesh_shader_src;

#endif // RENDERER_WEBGPU_H
