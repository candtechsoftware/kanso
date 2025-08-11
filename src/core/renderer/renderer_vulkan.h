#pragma once

#include "../base/base_inc.h"
#include "renderer_core.h"
#include <vulkan/vulkan.h>

struct Renderer_Vulkan_State
{
    Arena *arena;

    VkInstance               instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice         physical_device;
    VkDevice                 device;
    VkQueue                  graphics_queue;
    VkQueue                  present_queue;
    u32                      graphics_queue_family;
    u32                      present_queue_family;

    // Memory allocator
    VkPhysicalDeviceMemoryProperties memory_properties;

    // Command pools
    VkCommandPool command_pool;
    VkCommandPool transient_command_pool;

    // Descriptor pool
    VkDescriptorPool descriptor_pool;

    // Pipeline cache
    VkPipelineCache pipeline_cache;

    // Shader modules
    struct
    {
        VkShaderModule rect_vert;
        VkShaderModule rect_frag;
        VkShaderModule blur_vert;
        VkShaderModule blur_frag;
        VkShaderModule mesh_vert;
        VkShaderModule mesh_frag;
    } shaders;

    // Pipeline layouts
    struct
    {
        VkPipelineLayout ui;
        VkPipelineLayout blur;
        VkPipelineLayout geo_3d;
    } pipeline_layouts;

    // Descriptor set layouts
    struct
    {
        VkDescriptorSetLayout ui_global;
        VkDescriptorSetLayout ui_texture;
        VkDescriptorSetLayout blur_global;
        VkDescriptorSetLayout geo_3d_global;
        VkDescriptorSetLayout geo_3d_texture;
    } descriptor_set_layouts;

    // Pipelines
    struct
    {
        VkPipeline ui;
        VkPipeline blur_horizontal;
        VkPipeline blur_vertical;
        VkPipeline geo_3d;
    } pipelines;

    // Samplers
    VkSampler sampler_nearest;
    VkSampler sampler_linear;

    // Staging buffer for uploads
    VkBuffer       staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    void          *staging_buffer_mapped;
    u64            staging_buffer_size;
    u64            staging_buffer_offset;

    // Uniform buffers
    struct
    {
        VkBuffer       buffer;
        VkDeviceMemory memory;
        void          *mapped;
        u64            size;
        u64            offset;
    } uniform_buffer;

    // White texture for untextured rendering
    VkImage        white_texture;
    VkImageView    white_texture_view;
    VkDeviceMemory white_texture_memory;
};

struct Renderer_Vulkan_Window_Equipment
{
    VkSurfaceKHR        surface;
    VkSwapchainKHR      swapchain;
    VkFormat            swapchain_format;
    VkExtent2D          swapchain_extent;
    List<VkImage>       swapchain_images;
    List<VkImageView>   swapchain_image_views;
    List<VkFramebuffer> framebuffers;

    // Render pass
    VkRenderPass render_pass;

    // Depth buffer
    VkImage        depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView    depth_image_view;

    // Command buffers (fixed size for frames in flight)
    VkCommandBuffer command_buffers[2]; // MAX_FRAMES_IN_FLIGHT

    // Synchronization (fixed size for frames in flight)
    VkSemaphore image_available_semaphores[2]; // MAX_FRAMES_IN_FLIGHT
    VkSemaphore render_finished_semaphores[2]; // MAX_FRAMES_IN_FLIGHT
    VkFence     in_flight_fences[2];           // MAX_FRAMES_IN_FLIGHT
    u32         current_frame;
    u32         current_image_index;

    // Blur framebuffers
    VkImage        blur_texture_a;
    VkImageView    blur_texture_a_view;
    VkDeviceMemory blur_texture_a_memory;
    VkFramebuffer  blur_framebuffer_a;

    VkImage        blur_texture_b;
    VkImageView    blur_texture_b_view;
    VkDeviceMemory blur_texture_b_memory;
    VkFramebuffer  blur_framebuffer_b;

    // Descriptor sets per frame
    struct Frame_Resources
    {
        VkDescriptorSet ui_global_set;
        VkDescriptorSet blur_set;
        VkDescriptorSet geo_3d_global_set;

        // Uniform buffer slices
        u64 uniform_offset;
    };
    Frame_Resources frame_resources[2]; // MAX_FRAMES_IN_FLIGHT
};

struct Renderer_Vulkan_Texture_2D
{
    VkImage                image;
    VkDeviceMemory         memory;
    VkImageView            view;
    Vec2<f32>              size;
    Renderer_Tex_2D_Format format;
    Renderer_Resource_Kind kind;
};

struct Renderer_Vulkan_Buffer
{
    VkBuffer               buffer;
    VkDeviceMemory         memory;
    u64                    size;
    Renderer_Resource_Kind kind;
    void                  *mapped; // For dynamic buffers
};

// Global state
extern Renderer_Vulkan_State *g_vulkan;

// Vulkan utility functions
VkFormat
renderer_vulkan_format_from_tex_2d_format(Renderer_Tex_2D_Format format);
u32
renderer_vulkan_find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties);
VkShaderModule
renderer_vulkan_create_shader_module(const u8 *code, u64 size);
void
renderer_vulkan_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags properties, VkBuffer &buffer,
                              VkDeviceMemory &buffer_memory);
void
renderer_vulkan_create_image(u32 width, u32 height, VkFormat format, VkImageTiling tiling,
                             VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                             VkImage &image, VkDeviceMemory &image_memory);
VkImageView
renderer_vulkan_create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
void
renderer_vulkan_transition_image_layout(VkImage image, VkFormat format,
                                        VkImageLayout old_layout, VkImageLayout new_layout);
void
renderer_vulkan_copy_buffer_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height);
VkCommandBuffer
renderer_vulkan_begin_single_time_commands();
void
renderer_vulkan_end_single_time_commands(VkCommandBuffer command_buffer);

