#include "renderer_vulkan.h"
#include "../base/base.h"
#include "../base/array.h"
#include "../base/logger.h"
#include "../base/profiler.h"
#include "../os/os.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib.h>
#endif

#include <cstring>
#include <cstdio>
#include <ctime>
#include <unistd.h>

#ifdef RENDERER_VULKAN_DEBUG
#define RENDERER_VULKAN_VALIDATION_LAYERS
#endif

// Global Vulkan state pointer
Renderer_Vulkan_State* g_vulkan = nullptr;

// Constants
static const int MAX_FRAMES_IN_FLIGHT = 2;
static const u64 STAGING_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB

#ifdef RENDERER_VULKAN_VALIDATION_LAYERS
static const char* validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};
static const u32 validation_layer_count = sizeof(validation_layers) / sizeof(validation_layers[0]);
#endif

static const char* device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
static const u32 device_extension_count = sizeof(device_extensions) / sizeof(device_extensions[0]);

#ifdef RENDERER_VULKAN_VALIDATION_LAYERS
// Debug callback
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    log_error("Vulkan validation: %s", callback_data->pMessage);
    return VK_FALSE;
}
#endif

// Utility functions
VkFormat renderer_vulkan_format_from_tex_2d_format(Renderer_Tex_2D_Format format)
{
    switch (format)
    {
        case Renderer_Tex_2D_Format_R8: return VK_FORMAT_R8_UNORM;
        case Renderer_Tex_2D_Format_RG8: return VK_FORMAT_R8G8_UNORM;
        case Renderer_Tex_2D_Format_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case Renderer_Tex_2D_Format_BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
        case Renderer_Tex_2D_Format_R16: return VK_FORMAT_R16_UNORM;
        case Renderer_Tex_2D_Format_RGBA16: return VK_FORMAT_R16G16B16A16_UNORM;
        case Renderer_Tex_2D_Format_R32: return VK_FORMAT_R32_SFLOAT;
        default: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

u32 renderer_vulkan_find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties)
{
    for (u32 i = 0; i < g_vulkan->memory_properties.memoryTypeCount; i++)
    {
        if ((type_filter & (1 << i)) && 
            (g_vulkan->memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    
    log_error("Failed to find suitable memory type!");
    return 0;
}

void renderer_vulkan_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                   VkDeviceMemory& buffer_memory)
{
    ZoneScopedN("VulkanCreateBuffer");
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(g_vulkan->device, &buffer_info, nullptr, &buffer) != VK_SUCCESS)
    {
        log_error("Failed to create buffer!");
        return;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(g_vulkan->device, buffer, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = renderer_vulkan_find_memory_type(mem_requirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(g_vulkan->device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS)
    {
        log_error("Failed to allocate buffer memory!");
        return;
    }
    
    vkBindBufferMemory(g_vulkan->device, buffer, buffer_memory, 0);
}

void renderer_vulkan_create_image(u32 width, u32 height, VkFormat format, VkImageTiling tiling,
                                  VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                  VkImage& image, VkDeviceMemory& image_memory)
{
    ZoneScopedN("VulkanCreateImage");
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(g_vulkan->device, &image_info, nullptr, &image) != VK_SUCCESS)
    {
        log_error("Failed to create image!");
        return;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(g_vulkan->device, image, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = renderer_vulkan_find_memory_type(mem_requirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(g_vulkan->device, &alloc_info, nullptr, &image_memory) != VK_SUCCESS)
    {
        log_error("Failed to allocate image memory!");
        return;
    }
    
    vkBindImageMemory(g_vulkan->device, image, image_memory, 0);
}

VkImageView renderer_vulkan_create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    
    VkImageView image_view;
    if (vkCreateImageView(g_vulkan->device, &view_info, nullptr, &image_view) != VK_SUCCESS)
    {
        log_error("Failed to create image view!");
        return VK_NULL_HANDLE;
    }
    
    return image_view;
}

VkCommandBuffer renderer_vulkan_begin_single_time_commands()
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = g_vulkan->transient_command_pool;
    alloc_info.commandBufferCount = 1;
    
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(g_vulkan->device, &alloc_info, &command_buffer);
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(command_buffer, &begin_info);
    
    return command_buffer;
}

void renderer_vulkan_end_single_time_commands(VkCommandBuffer command_buffer)
{
    vkEndCommandBuffer(command_buffer);
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    
    vkQueueSubmit(g_vulkan->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vulkan->graphics_queue);
    
    vkFreeCommandBuffers(g_vulkan->device, g_vulkan->transient_command_pool, 1, &command_buffer);
}

void renderer_vulkan_transition_image_layout(VkImage image, VkFormat format,
                                           VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer command_buffer = renderer_vulkan_begin_single_time_commands();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;
    
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        log_error("Unsupported layout transition!");
        return;
    }
    
    vkCmdPipelineBarrier(
        command_buffer,
        source_stage, destination_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    renderer_vulkan_end_single_time_commands(command_buffer);
}

void renderer_vulkan_copy_buffer_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height)
{
    VkCommandBuffer command_buffer = renderer_vulkan_begin_single_time_commands();
    
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    renderer_vulkan_end_single_time_commands(command_buffer);
}

// Simple GLSL to SPIR-V compilation using temporary files
static bool compile_glsl_to_spirv(const char* source, VkShaderStageFlagBits stage, 
                                 u32* spirv_buffer, u64* spirv_size, u64 max_size)
{
    // Use OS temp directory
    const char* temp_dir = "/tmp";
    char glsl_path[256];
    char spirv_path[256];
    
    // Generate unique filenames based on current time
    u64 timestamp = (u64)time(nullptr);
    const char* stage_ext = (stage == VK_SHADER_STAGE_VERTEX_BIT) ? "vert" : "frag";
    snprintf(glsl_path, sizeof(glsl_path), "%s/kanso_shader_%lu.%s", temp_dir, timestamp, stage_ext);
    snprintf(spirv_path, sizeof(spirv_path), "%s/kanso_shader_%lu.spv", temp_dir, timestamp);
    
    // Write GLSL source to temp file
    FILE* glsl_file = fopen(glsl_path, "w");
    if (!glsl_file)
    {
        log_error("Failed to create temporary GLSL file");
        return false;
    }
    fprintf(glsl_file, "%s", source);
    fclose(glsl_file);
    
    // Try to compile with glslangValidator or glslc
    char cmd[1024];  // Increased buffer size to prevent truncation
    bool compiled = false;
    
    // Try glslc first (from shaderc/Google)
    snprintf(cmd, sizeof(cmd), "glslc -fshader-stage=%s %s -o %s 2>/dev/null",
             stage_ext, glsl_path, spirv_path);
    if (system(cmd) == 0)
    {
        compiled = true;
    }
    else
    {
        // Try glslangValidator (from Khronos)
        snprintf(cmd, sizeof(cmd), "glslangValidator -V %s -o %s 2>/dev/null",
                 glsl_path, spirv_path);
        if (system(cmd) == 0)
        {
            compiled = true;
        }
    }
    
    // Clean up GLSL file
    unlink(glsl_path);
    
    if (!compiled)
    {
        log_error("Failed to compile GLSL to SPIR-V. Install glslc or glslangValidator.");
        return false;
    }
    
    // Read SPIR-V file
    FILE* spirv_file = fopen(spirv_path, "rb");
    if (!spirv_file)
    {
        log_error("Failed to open compiled SPIR-V file");
        return false;
    }
    
    fseek(spirv_file, 0, SEEK_END);
    long file_size = ftell(spirv_file);
    fseek(spirv_file, 0, SEEK_SET);
    
    if (file_size < 0 || (u64)file_size > max_size)
    {
        log_error("Compiled SPIR-V is too large: %ld bytes (max %lu)", file_size, max_size);
        fclose(spirv_file);
        unlink(spirv_path);
        return false;
    }
    
    *spirv_size = file_size;
    fread(spirv_buffer, 1, file_size, spirv_file);
    fclose(spirv_file);
    
    // Clean up SPIR-V file
    unlink(spirv_path);
    
    return true;
}

VkShaderModule renderer_vulkan_create_shader_module(const u8* code, u64 size)
{
    // Check if this is GLSL (text) or SPIR-V (binary)
    bool is_spirv = size >= 4 && *((u32*)code) == 0x07230203;
    
    const u32* spirv_code = nullptr;
    u64 spirv_size = 0;
    
    // Static buffer for compiled SPIR-V (128KB should be plenty for shaders)
    static u32 compiled_spirv_buffer[32768]; // 32K u32s = 128KB
    
    if (is_spirv)
    {
        // Already SPIR-V
        spirv_code = (const u32*)code;
        spirv_size = size;
    }
    else
    {
        // Need to compile GLSL to SPIR-V
        // Determine shader stage from the source (hack for now)
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
        const char* source = (const char*)code;
        // Simple string search
        const char* frag_indicators[] = {"gl_FragCoord", "layout(location = 0) out", "frag_", nullptr};
        for (int i = 0; frag_indicators[i]; i++)
        {
            const char* search = frag_indicators[i];
            const char* p = source;
            while (*p)
            {
                const char* s = search;
                const char* t = p;
                while (*s && *t && *s == *t) { s++; t++; }
                if (!*s)
                {
                    stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                    break;
                }
                p++;
            }
            if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) break;
        }
        
        u64 compiled_size = 0;
        if (!compile_glsl_to_spirv(source, stage, compiled_spirv_buffer, &compiled_size, sizeof(compiled_spirv_buffer)))
        {
            return VK_NULL_HANDLE;
        }
        
        spirv_code = compiled_spirv_buffer;
        spirv_size = compiled_size;
    }
    
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv_size;
    create_info.pCode = spirv_code;
    
    VkShaderModule shader_module;
    if (vkCreateShaderModule(g_vulkan->device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
    {
        log_error("Failed to create shader module!");
        return VK_NULL_HANDLE;
    }
    
    return shader_module;
}

// Initialize Vulkan
void renderer_init()
{
    ZoneScoped;
    // Allocate global Vulkan state
    Arena* arena = arena_alloc();
    g_vulkan = push_struct_zero(arena, Renderer_Vulkan_State);
    g_vulkan->arena = arena;
    
    // Create instance
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Kanso";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Kanso Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    
    // Get required extensions
    u32 glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    
    Array<const char*, 16> extensions = {}; // Max 16 extensions should be plenty
    for (u32 i = 0; i < glfw_extension_count; i++)
    {
        array_push(&extensions, glfw_extensions[i]);
    }
    
#ifdef RENDERER_VULKAN_VALIDATION_LAYERS
    array_push(&extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    
    create_info.enabledExtensionCount = extensions.size;
    create_info.ppEnabledExtensionNames = extensions.data;
    
#ifdef RENDERER_VULKAN_VALIDATION_LAYERS
    create_info.enabledLayerCount = validation_layer_count;
    create_info.ppEnabledLayerNames = validation_layers;
    
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = debug_callback;
    
    create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
#else
    create_info.enabledLayerCount = 0;
    create_info.pNext = nullptr;
#endif
    
    if (vkCreateInstance(&create_info, nullptr, &g_vulkan->instance) != VK_SUCCESS)
    {
        log_error("Failed to create Vulkan instance!");
        return;
    }
    
    // Setup debug messenger
#ifdef RENDERER_VULKAN_VALIDATION_LAYERS
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_vulkan->instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(g_vulkan->instance, &debug_create_info, nullptr, &g_vulkan->debug_messenger);
    }
#endif
    
    // Pick physical device
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan->instance, &device_count, nullptr);
    
    if (device_count == 0)
    {
        log_error("Failed to find GPUs with Vulkan support!");
        return;
    }
    
    Array<VkPhysicalDevice, 8> devices = {}; // Max 8 GPUs should be plenty
    devices.size = device_count;
    vkEnumeratePhysicalDevices(g_vulkan->instance, &device_count, devices.data);
    
    // Pick first suitable device (can be improved)
    for (u32 i = 0; i < devices.size; i++)
    {
        VkPhysicalDevice device = devices.data[i];
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        
        // Check for discrete GPU first
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            g_vulkan->physical_device = device;
            break;
        }
    }
    
    // If no discrete GPU found, use the first one
    if (g_vulkan->physical_device == VK_NULL_HANDLE)
    {
        g_vulkan->physical_device = devices.data[0];
    }
    
    // Get memory properties
    vkGetPhysicalDeviceMemoryProperties(g_vulkan->physical_device, &g_vulkan->memory_properties);
    
    // Find queue families
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vulkan->physical_device, &queue_family_count, nullptr);
    
    Array<VkQueueFamilyProperties, 8> queue_families = {}; // Max 8 queue families
    queue_families.size = queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vulkan->physical_device, &queue_family_count, queue_families.data);
    
    // Find graphics queue family
    for (u32 i = 0; i < queue_family_count; i++)
    {
        if (queue_families.data[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            g_vulkan->graphics_queue_family = i;
            g_vulkan->present_queue_family = i; // Will be updated when creating surface
            break;
        }
    }
    
    // Create logical device
    // Note: We'll handle separate present queue later when we have a surface
    Array<VkDeviceQueueCreateInfo, 2> queue_create_infos = {}; // Max 2 queue families (graphics + present)
    float queue_priority = 1.0f;
    
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = g_vulkan->graphics_queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    array_push(&queue_create_infos, queue_create_info);
    
    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;
    
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = queue_create_infos.size;
    device_create_info.pQueueCreateInfos = queue_create_infos.data;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = device_extension_count;
    device_create_info.ppEnabledExtensionNames = device_extensions;
    
#ifdef RENDERER_VULKAN_VALIDATION_LAYERS
    device_create_info.enabledLayerCount = validation_layer_count;
    device_create_info.ppEnabledLayerNames = validation_layers;
#else
    device_create_info.enabledLayerCount = 0;
#endif
    
    if (vkCreateDevice(g_vulkan->physical_device, &device_create_info, nullptr, &g_vulkan->device) != VK_SUCCESS)
    {
        log_error("Failed to create logical device!");
        return;
    }
    
    // Get device queues
    vkGetDeviceQueue(g_vulkan->device, g_vulkan->graphics_queue_family, 0, &g_vulkan->graphics_queue);
    vkGetDeviceQueue(g_vulkan->device, g_vulkan->present_queue_family, 0, &g_vulkan->present_queue);
    
    // Create command pools
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = g_vulkan->graphics_queue_family;
    
    if (vkCreateCommandPool(g_vulkan->device, &pool_info, nullptr, &g_vulkan->command_pool) != VK_SUCCESS)
    {
        log_error("Failed to create command pool!");
        return;
    }
    
    // Create transient command pool
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (vkCreateCommandPool(g_vulkan->device, &pool_info, nullptr, &g_vulkan->transient_command_pool) != VK_SUCCESS)
    {
        log_error("Failed to create transient command pool!");
        return;
    }
    
    // Create descriptor pool
    Array<VkDescriptorPoolSize, 3> pool_sizes = {};
    pool_sizes.data[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes.data[0].descriptorCount = 10000;
    pool_sizes.data[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes.data[1].descriptorCount = 10000;
    pool_sizes.data[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes.data[2].descriptorCount = 10000;
    pool_sizes.size = 3;
    
    VkDescriptorPoolCreateInfo descriptor_pool_info{};
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.poolSizeCount = pool_sizes.size;
    descriptor_pool_info.pPoolSizes = pool_sizes.data;
    descriptor_pool_info.maxSets = 30000;
    descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    
    if (vkCreateDescriptorPool(g_vulkan->device, &descriptor_pool_info, nullptr, &g_vulkan->descriptor_pool) != VK_SUCCESS)
    {
        log_error("Failed to create descriptor pool!");
        return;
    }
    
    // Create samplers
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    
    if (vkCreateSampler(g_vulkan->device, &sampler_info, nullptr, &g_vulkan->sampler_nearest) != VK_SUCCESS)
    {
        log_error("Failed to create nearest sampler!");
        return;
    }
    
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(g_vulkan->device, &sampler_info, nullptr, &g_vulkan->sampler_linear) != VK_SUCCESS)
    {
        log_error("Failed to create linear sampler!");
        return;
    }
    
    // Create staging buffer
    renderer_vulkan_create_buffer(STAGING_BUFFER_SIZE,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  g_vulkan->staging_buffer,
                                  g_vulkan->staging_buffer_memory);
    
    vkMapMemory(g_vulkan->device, g_vulkan->staging_buffer_memory, 0, STAGING_BUFFER_SIZE, 0, &g_vulkan->staging_buffer_mapped);
    g_vulkan->staging_buffer_size = STAGING_BUFFER_SIZE;
    g_vulkan->staging_buffer_offset = 0;
    
    // Create uniform buffer (16MB)
    g_vulkan->uniform_buffer.size = MB(16);
    renderer_vulkan_create_buffer(g_vulkan->uniform_buffer.size,
                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  g_vulkan->uniform_buffer.buffer,
                                  g_vulkan->uniform_buffer.memory);
    
    vkMapMemory(g_vulkan->device, g_vulkan->uniform_buffer.memory, 0, g_vulkan->uniform_buffer.size, 0, &g_vulkan->uniform_buffer.mapped);
    g_vulkan->uniform_buffer.offset = 0;
    
    // Create white texture (1x1)
    {
        renderer_vulkan_create_image(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    g_vulkan->white_texture, g_vulkan->white_texture_memory);
        
        // Transition to transfer destination
        renderer_vulkan_transition_image_layout(g_vulkan->white_texture, VK_FORMAT_R8G8B8A8_UNORM,
                                              VK_IMAGE_LAYOUT_UNDEFINED,
                                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        
        // Upload white pixel
        u32 white_pixel = 0xFFFFFFFF;
        memcpy(g_vulkan->staging_buffer_mapped, &white_pixel, sizeof(white_pixel));
        
        VkCommandBuffer cmd = renderer_vulkan_begin_single_time_commands();
        
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {1, 1, 1};
        
        vkCmdCopyBufferToImage(cmd, g_vulkan->staging_buffer, g_vulkan->white_texture,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        renderer_vulkan_end_single_time_commands(cmd);
        
        // Transition to shader read
        renderer_vulkan_transition_image_layout(g_vulkan->white_texture, VK_FORMAT_R8G8B8A8_UNORM,
                                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
        // Create image view
        g_vulkan->white_texture_view = renderer_vulkan_create_image_view(g_vulkan->white_texture,
                                                                      VK_FORMAT_R8G8B8A8_UNORM,
                                                                      VK_IMAGE_ASPECT_COLOR_BIT);
    }
    
    // Forward declarations from renderer_vulkan_shaders.cpp
    void renderer_vulkan_create_shaders();
    void renderer_vulkan_create_descriptor_set_layouts();
    void renderer_vulkan_create_pipeline_layouts();
    
    // Create shaders and pipelines
    renderer_vulkan_create_shaders();
    renderer_vulkan_create_descriptor_set_layouts();
    renderer_vulkan_create_pipeline_layouts();
    
    // Note: Pipelines are created per window when render pass is available
}

// Cleanup function
void renderer_shutdown()
{
    ZoneScoped;
    if (g_vulkan->device == VK_NULL_HANDLE) return;
    
    vkDeviceWaitIdle(g_vulkan->device);
    
    // Destroy pipelines
    if (g_vulkan->pipelines.ui) vkDestroyPipeline(g_vulkan->device, g_vulkan->pipelines.ui, nullptr);
    if (g_vulkan->pipelines.blur_horizontal) vkDestroyPipeline(g_vulkan->device, g_vulkan->pipelines.blur_horizontal, nullptr);
    if (g_vulkan->pipelines.blur_vertical && g_vulkan->pipelines.blur_vertical != g_vulkan->pipelines.blur_horizontal)
        vkDestroyPipeline(g_vulkan->device, g_vulkan->pipelines.blur_vertical, nullptr);
    if (g_vulkan->pipelines.geo_3d) vkDestroyPipeline(g_vulkan->device, g_vulkan->pipelines.geo_3d, nullptr);
    
    // Destroy pipeline layouts
    if (g_vulkan->pipeline_layouts.ui) vkDestroyPipelineLayout(g_vulkan->device, g_vulkan->pipeline_layouts.ui, nullptr);
    if (g_vulkan->pipeline_layouts.blur) vkDestroyPipelineLayout(g_vulkan->device, g_vulkan->pipeline_layouts.blur, nullptr);
    if (g_vulkan->pipeline_layouts.geo_3d) vkDestroyPipelineLayout(g_vulkan->device, g_vulkan->pipeline_layouts.geo_3d, nullptr);
    
    // Destroy descriptor set layouts
    if (g_vulkan->descriptor_set_layouts.ui_global) 
        vkDestroyDescriptorSetLayout(g_vulkan->device, g_vulkan->descriptor_set_layouts.ui_global, nullptr);
    if (g_vulkan->descriptor_set_layouts.ui_texture) 
        vkDestroyDescriptorSetLayout(g_vulkan->device, g_vulkan->descriptor_set_layouts.ui_texture, nullptr);
    if (g_vulkan->descriptor_set_layouts.blur_global) 
        vkDestroyDescriptorSetLayout(g_vulkan->device, g_vulkan->descriptor_set_layouts.blur_global, nullptr);
    if (g_vulkan->descriptor_set_layouts.geo_3d_global) 
        vkDestroyDescriptorSetLayout(g_vulkan->device, g_vulkan->descriptor_set_layouts.geo_3d_global, nullptr);
    if (g_vulkan->descriptor_set_layouts.geo_3d_texture) 
        vkDestroyDescriptorSetLayout(g_vulkan->device, g_vulkan->descriptor_set_layouts.geo_3d_texture, nullptr);
    
    // Destroy shaders
    void renderer_vulkan_destroy_shaders();
    renderer_vulkan_destroy_shaders();
    
    // Destroy white texture
    if (g_vulkan->white_texture_view) vkDestroyImageView(g_vulkan->device, g_vulkan->white_texture_view, nullptr);
    if (g_vulkan->white_texture) vkDestroyImage(g_vulkan->device, g_vulkan->white_texture, nullptr);
    if (g_vulkan->white_texture_memory) vkFreeMemory(g_vulkan->device, g_vulkan->white_texture_memory, nullptr);
    
    // Destroy uniform buffer
    if (g_vulkan->uniform_buffer.mapped) vkUnmapMemory(g_vulkan->device, g_vulkan->uniform_buffer.memory);
    if (g_vulkan->uniform_buffer.buffer) vkDestroyBuffer(g_vulkan->device, g_vulkan->uniform_buffer.buffer, nullptr);
    if (g_vulkan->uniform_buffer.memory) vkFreeMemory(g_vulkan->device, g_vulkan->uniform_buffer.memory, nullptr);
    
    // Destroy staging buffer
    if (g_vulkan->staging_buffer_mapped) vkUnmapMemory(g_vulkan->device, g_vulkan->staging_buffer_memory);
    if (g_vulkan->staging_buffer) vkDestroyBuffer(g_vulkan->device, g_vulkan->staging_buffer, nullptr);
    if (g_vulkan->staging_buffer_memory) vkFreeMemory(g_vulkan->device, g_vulkan->staging_buffer_memory, nullptr);
    
    // Destroy samplers
    if (g_vulkan->sampler_nearest) vkDestroySampler(g_vulkan->device, g_vulkan->sampler_nearest, nullptr);
    if (g_vulkan->sampler_linear) vkDestroySampler(g_vulkan->device, g_vulkan->sampler_linear, nullptr);
    
    // Destroy descriptor pool
    if (g_vulkan->descriptor_pool) vkDestroyDescriptorPool(g_vulkan->device, g_vulkan->descriptor_pool, nullptr);
    
    // Destroy command pools
    if (g_vulkan->command_pool) vkDestroyCommandPool(g_vulkan->device, g_vulkan->command_pool, nullptr);
    if (g_vulkan->transient_command_pool) vkDestroyCommandPool(g_vulkan->device, g_vulkan->transient_command_pool, nullptr);
    
    // Destroy device
    if (g_vulkan->device) vkDestroyDevice(g_vulkan->device, nullptr);
    
    // Destroy debug messenger
#ifdef RENDERER_VULKAN_VALIDATION_LAYERS
    if (g_vulkan->debug_messenger)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_vulkan->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(g_vulkan->instance, g_vulkan->debug_messenger, nullptr);
        }
    }
#endif
    
    // Destroy instance
    if (g_vulkan->instance) vkDestroyInstance(g_vulkan->instance, nullptr);
    
    arena_release(g_vulkan->arena);
    g_vulkan = nullptr;
}

// Helper function to recreate swapchain
void renderer_vulkan_recreate_swapchain(Renderer_Vulkan_Window_Equipment* equip)
{
    // Get current window size
    // Note: we don't have the window pointer here, so we'll handle differently
    
    vkDeviceWaitIdle(g_vulkan->device);
    
    // Cleanup old swapchain resources
    for (List_Node<VkFramebuffer>* node = equip->framebuffers.first; node; node = node->next)
    {
        vkDestroyFramebuffer(g_vulkan->device, node->v, nullptr);
    }
    equip->framebuffers = list_make<VkFramebuffer>();
    
    for (List_Node<VkImageView>* node = equip->swapchain_image_views.first; node; node = node->next)
    {
        vkDestroyImageView(g_vulkan->device, node->v, nullptr);
    }
    equip->swapchain_image_views = list_make<VkImageView>();
    
    // Cleanup depth resources
    vkDestroyImageView(g_vulkan->device, equip->depth_image_view, nullptr);
    vkDestroyImage(g_vulkan->device, equip->depth_image, nullptr);
    vkFreeMemory(g_vulkan->device, equip->depth_image_memory, nullptr);
    
    // Keep old swapchain for creation
    VkSwapchainKHR old_swapchain = equip->swapchain;
    
    // Query current surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vulkan->physical_device, equip->surface, &capabilities);
    
    // Choose new extent
    VkExtent2D extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX)
    {
        // Can't determine size without window handle
        // For now, use capabilities min extent
        extent = capabilities.minImageExtent;
    }
    
    // Skip if minimized
    if (extent.width == 0 || extent.height == 0)
    {
        return;
    }
    
    equip->swapchain_extent = extent;
    
    // Get formats and present modes
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vulkan->physical_device, equip->surface, &format_count, nullptr);
    Array<VkSurfaceFormatKHR, 32> formats = {};
    formats.size = format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vulkan->physical_device, equip->surface, &format_count, formats.data);
    
    VkSurfaceFormatKHR surface_format = formats.data[0];
    for (u32 i = 0; i < formats.size; i++)
    {
        VkSurfaceFormatKHR& format = formats.data[i];
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surface_format = format;
            break;
        }
    }
    
    // Recreate swapchain
    u32 image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
    {
        image_count = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = equip->surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    u32 queue_family_indices[] = {g_vulkan->graphics_queue_family, g_vulkan->present_queue_family};
    if (g_vulkan->graphics_queue_family != g_vulkan->present_queue_family)
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // No vsync
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = old_swapchain;
    
    if (vkCreateSwapchainKHR(g_vulkan->device, &swapchain_info, nullptr, &equip->swapchain) != VK_SUCCESS)
    {
        log_error("Failed to recreate swap chain!");
        return;
    }
    
    // Destroy old swapchain
    vkDestroySwapchainKHR(g_vulkan->device, old_swapchain, nullptr);
    
    // Get new swap chain images
    vkGetSwapchainImagesKHR(g_vulkan->device, equip->swapchain, &image_count, nullptr);
    
    // Create temp array to get images, then add to list
    Array<VkImage, 8> temp_images = {};
    temp_images.size = image_count;
    vkGetSwapchainImagesKHR(g_vulkan->device, equip->swapchain, &image_count, temp_images.data);
    
    equip->swapchain_images = list_make<VkImage>();
    for (u32 i = 0; i < image_count; i++)
    {
        list_push(g_vulkan->arena, &equip->swapchain_images, temp_images.data[i]);
    }
    
    // Recreate image views
    for (List_Node<VkImage>* node = equip->swapchain_images.first; node; node = node->next)
    {
        VkImageView image_view = renderer_vulkan_create_image_view(
            node->v, equip->swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT);
        list_push(g_vulkan->arena, &equip->swapchain_image_views, image_view);
    }
    
    // Recreate depth resources
    renderer_vulkan_create_image(extent.width, extent.height,
                                VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                equip->depth_image, equip->depth_image_memory);
    
    equip->depth_image_view = renderer_vulkan_create_image_view(equip->depth_image,
                                                               VK_FORMAT_D32_SFLOAT,
                                                               VK_IMAGE_ASPECT_DEPTH_BIT);
    
    // Recreate framebuffers
    equip->framebuffers = list_make<VkFramebuffer>();
    for (List_Node<VkImageView>* node = equip->swapchain_image_views.first; node; node = node->next)
    {
        VkImageView attachments[] = {
            node->v,
            equip->depth_image_view
        };
        
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = equip->render_pass;
        framebuffer_info.attachmentCount = 2;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = extent.width;
        framebuffer_info.height = extent.height;
        framebuffer_info.layers = 1;
        
        VkFramebuffer framebuffer;
        if (vkCreateFramebuffer(g_vulkan->device, &framebuffer_info, nullptr, &framebuffer) != VK_SUCCESS)
        {
            log_error("Failed to recreate framebuffer!");
        }
        else
        {
            list_push(g_vulkan->arena, &equip->framebuffers, framebuffer);
        }
    }
}

// Window equipment functions
Renderer_Handle renderer_window_equip(void* window)
{
    ZoneScoped;
    GLFWwindow* glfw_window = (GLFWwindow*)window;
    
    Arena* arena = arena_alloc();
    Renderer_Vulkan_Window_Equipment* equip = push_array(arena, Renderer_Vulkan_Window_Equipment, 1);
    *equip = {};
    
    // Initialize lists
    equip->swapchain_images = list_make<VkImage>();
    equip->swapchain_image_views = list_make<VkImageView>();
    equip->framebuffers = list_make<VkFramebuffer>();
    
    // Create surface based on platform
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    
#ifdef __linux__
#ifdef USE_WAYLAND
    // Wayland surface creation
    struct wl_display* wayland_display = glfwGetWaylandDisplay();
    if (wayland_display != nullptr)
    {
        VkWaylandSurfaceCreateInfoKHR surface_info{};
        surface_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surface_info.display = wayland_display;
        surface_info.surface = glfwGetWaylandWindow(glfw_window);
        
        auto func = (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(g_vulkan->instance, "vkCreateWaylandSurfaceKHR");
        if (func != nullptr)
        {
            result = func(g_vulkan->instance, &surface_info, nullptr, &equip->surface);
        }
    }
#else
    // X11 surface creation
    Display* x11_display = glfwGetX11Display();
    if (x11_display != nullptr)
    {
        VkXlibSurfaceCreateInfoKHR surface_info{};
        surface_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        surface_info.dpy = x11_display;
        surface_info.window = glfwGetX11Window(glfw_window);
        
        auto func = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(g_vulkan->instance, "vkCreateXlibSurfaceKHR");
        if (func != nullptr)
        {
            result = func(g_vulkan->instance, &surface_info, nullptr, &equip->surface);
        }
    }
#endif
#endif
    
    if (result != VK_SUCCESS)
    {
        // Fallback to GLFW's built-in surface creation
        result = glfwCreateWindowSurface(g_vulkan->instance, glfw_window, nullptr, &equip->surface);
    }
    
    if (result != VK_SUCCESS)
    {
        log_error("Failed to create window surface!");
        arena_release(arena);
        return renderer_handle_zero();
    }
    
    // Check present queue support
    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_vulkan->physical_device, g_vulkan->graphics_queue_family, 
                                       equip->surface, &present_support);
    
    if (!present_support)
    {
        // Find a queue that supports presentation
        u32 queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g_vulkan->physical_device, &queue_family_count, nullptr);
        
        for (u32 i = 0; i < queue_family_count; i++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(g_vulkan->physical_device, i, equip->surface, &present_support);
            if (present_support)
            {
                g_vulkan->present_queue_family = i;
                vkGetDeviceQueue(g_vulkan->device, i, 0, &g_vulkan->present_queue);
                break;
            }
        }
    }
    
    // Query swap chain support
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vulkan->physical_device, equip->surface, &capabilities);
    
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vulkan->physical_device, equip->surface, &format_count, nullptr);
    Array<VkSurfaceFormatKHR, 32> formats = {};
    formats.size = format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vulkan->physical_device, equip->surface, &format_count, formats.data);
    
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vulkan->physical_device, equip->surface, &present_mode_count, nullptr);
    Array<VkPresentModeKHR, 8> present_modes = {};
    present_modes.size = present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_vulkan->physical_device, equip->surface, &present_mode_count, present_modes.data);
    
    // Choose swap surface format
    VkSurfaceFormatKHR surface_format = formats.data[0];
    for (u32 i = 0; i < formats.size; i++)
    {
        VkSurfaceFormatKHR& format = formats.data[i];
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surface_format = format;
            break;
        }
    }
    
    // Choose present mode - prefer immediate mode for no vsync
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    bool found_immediate = false;
    for (u32 i = 0; i < present_modes.size; i++)
    {
        if (present_modes.data[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            found_immediate = true;
            break;
        }
    }
    
    // Fallback to FIFO if immediate not available
    if (!found_immediate)
    {
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }
    
    // Choose swap extent
    VkExtent2D extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX)
    {
        int width, height;
        glfwGetFramebufferSize(glfw_window, &width, &height);
        
        u32 w = static_cast<u32>(width);
        u32 h = static_cast<u32>(height);
        
        extent.width = (w < capabilities.minImageExtent.width) ? capabilities.minImageExtent.width :
                      (w > capabilities.maxImageExtent.width) ? capabilities.maxImageExtent.width : w;
        extent.height = (h < capabilities.minImageExtent.height) ? capabilities.minImageExtent.height :
                       (h > capabilities.maxImageExtent.height) ? capabilities.maxImageExtent.height : h;
    }
    
    // Create swap chain
    u32 image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
    {
        image_count = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = equip->surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    u32 queue_family_indices[] = {g_vulkan->graphics_queue_family, g_vulkan->present_queue_family};
    
    if (g_vulkan->graphics_queue_family != g_vulkan->present_queue_family)
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(g_vulkan->device, &swapchain_info, nullptr, &equip->swapchain) != VK_SUCCESS)
    {
        log_error("Failed to create swap chain!");
        vkDestroySurfaceKHR(g_vulkan->instance, equip->surface, nullptr);
        arena_release(arena);
        return renderer_handle_zero();
    }
    
    equip->swapchain_format = surface_format.format;
    equip->swapchain_extent = extent;
    
    // Get swap chain images
    vkGetSwapchainImagesKHR(g_vulkan->device, equip->swapchain, &image_count, nullptr);
    
    // Create temp array to get images, then add to list
    Array<VkImage, 8> temp_images = {};
    temp_images.size = image_count;
    vkGetSwapchainImagesKHR(g_vulkan->device, equip->swapchain, &image_count, temp_images.data);
    
    for (u32 i = 0; i < image_count; i++)
    {
        list_push(arena, &equip->swapchain_images, temp_images.data[i]);
    }
    
    // Create image views
    for (List_Node<VkImage>* node = equip->swapchain_images.first; node; node = node->next)
    {
        VkImageView image_view = renderer_vulkan_create_image_view(
            node->v, 
            equip->swapchain_format, 
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        list_push(arena, &equip->swapchain_image_views, image_view);
    }
    
    // Create render pass
    VkAttachmentDescription color_attachment{};
    color_attachment.format = equip->swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = VK_FORMAT_D32_SFLOAT;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};
    
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    
    if (vkCreateRenderPass(g_vulkan->device, &render_pass_info, nullptr, &equip->render_pass) != VK_SUCCESS)
    {
        log_error("Failed to create render pass!");
        // Cleanup and return
        return renderer_handle_zero();
    }
    
    // Create depth resources
    renderer_vulkan_create_image(equip->swapchain_extent.width, equip->swapchain_extent.height,
                                VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                equip->depth_image, equip->depth_image_memory);
    
    equip->depth_image_view = renderer_vulkan_create_image_view(equip->depth_image, 
                                                               VK_FORMAT_D32_SFLOAT, 
                                                               VK_IMAGE_ASPECT_DEPTH_BIT);
    
    // Create framebuffers
    for (List_Node<VkImageView>* node = equip->swapchain_image_views.first; node; node = node->next)
    {
        VkImageView attachments[] = {
            node->v,
            equip->depth_image_view
        };
        
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = equip->render_pass;
        framebuffer_info.attachmentCount = 2;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = equip->swapchain_extent.width;
        framebuffer_info.height = equip->swapchain_extent.height;
        framebuffer_info.layers = 1;
        
        VkFramebuffer framebuffer;
        if (vkCreateFramebuffer(g_vulkan->device, &framebuffer_info, nullptr, &framebuffer) != VK_SUCCESS)
        {
            log_error("Failed to create framebuffer!");
            return renderer_handle_zero();
        }
        
        list_push(arena, &equip->framebuffers, framebuffer);
    }
    
    // Allocate command buffers
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_vulkan->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    if (vkAllocateCommandBuffers(g_vulkan->device, &alloc_info, equip->command_buffers) != VK_SUCCESS)
    {
        log_error("Failed to allocate command buffers!");
        return renderer_handle_zero();
    }
    
    // Create synchronization objects (arrays are already sized)
    
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(g_vulkan->device, &semaphore_info, nullptr, &equip->image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_vulkan->device, &semaphore_info, nullptr, &equip->render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(g_vulkan->device, &fence_info, nullptr, &equip->in_flight_fences[i]) != VK_SUCCESS)
        {
            log_error("Failed to create synchronization objects!");
            return renderer_handle_zero();
        }
    }
    
    equip->current_frame = 0;
    
    // Create pipelines if not already created
    if (g_vulkan->pipelines.ui == VK_NULL_HANDLE)
    {
        void renderer_vulkan_create_pipelines(VkRenderPass render_pass);
        renderer_vulkan_create_pipelines(equip->render_pass);
    }
    
    // Allocate descriptor sets for each frame (array already sized)
    
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        // Allocate UI global descriptor set
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_vulkan->descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &g_vulkan->descriptor_set_layouts.ui_global;
        
        if (vkAllocateDescriptorSets(g_vulkan->device, &alloc_info, 
                                    &equip->frame_resources[i].ui_global_set) != VK_SUCCESS)
        {
            log_error("Failed to allocate UI global descriptor set!");
        }
        
        // Allocate blur descriptor set
        alloc_info.pSetLayouts = &g_vulkan->descriptor_set_layouts.blur_global;
        if (vkAllocateDescriptorSets(g_vulkan->device, &alloc_info, 
                                    &equip->frame_resources[i].blur_set) != VK_SUCCESS)
        {
            log_error("Failed to allocate blur descriptor set!");
        }
        
        // Allocate geo 3D global descriptor set
        alloc_info.pSetLayouts = &g_vulkan->descriptor_set_layouts.geo_3d_global;
        if (vkAllocateDescriptorSets(g_vulkan->device, &alloc_info, 
                                    &equip->frame_resources[i].geo_3d_global_set) != VK_SUCCESS)
        {
            log_error("Failed to allocate geo 3D global descriptor set!");
        }
        
        // Assign uniform buffer offsets (align to 256 bytes for uniform buffer alignment)
        equip->frame_resources[i].uniform_offset = i * AlignPow2(KB(256), 256);
    }
    
    // Return handle
    Renderer_Handle handle = {};
    handle.u64s[0] = (u64)equip;
    return handle;
}

void renderer_window_unequip(void* window, Renderer_Handle window_equip)
{
    ZoneScoped;
    Renderer_Vulkan_Window_Equipment* equip = (Renderer_Vulkan_Window_Equipment*)window_equip.u64s[0];
    if (!equip) return;
    
    vkDeviceWaitIdle(g_vulkan->device);
    
    // Cleanup blur resources
    if (equip->blur_framebuffer_a) vkDestroyFramebuffer(g_vulkan->device, equip->blur_framebuffer_a, nullptr);
    if (equip->blur_framebuffer_b) vkDestroyFramebuffer(g_vulkan->device, equip->blur_framebuffer_b, nullptr);
    if (equip->blur_texture_a_view) vkDestroyImageView(g_vulkan->device, equip->blur_texture_a_view, nullptr);
    if (equip->blur_texture_b_view) vkDestroyImageView(g_vulkan->device, equip->blur_texture_b_view, nullptr);
    if (equip->blur_texture_a) vkDestroyImage(g_vulkan->device, equip->blur_texture_a, nullptr);
    if (equip->blur_texture_b) vkDestroyImage(g_vulkan->device, equip->blur_texture_b, nullptr);
    if (equip->blur_texture_a_memory) vkFreeMemory(g_vulkan->device, equip->blur_texture_a_memory, nullptr);
    if (equip->blur_texture_b_memory) vkFreeMemory(g_vulkan->device, equip->blur_texture_b_memory, nullptr);
    
    // Cleanup synchronization
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(g_vulkan->device, equip->render_finished_semaphores[i], nullptr);
        vkDestroySemaphore(g_vulkan->device, equip->image_available_semaphores[i], nullptr);
        vkDestroyFence(g_vulkan->device, equip->in_flight_fences[i], nullptr);
    }
    
    // Cleanup framebuffers
    for (List_Node<VkFramebuffer>* node = equip->framebuffers.first; node; node = node->next)
    {
        vkDestroyFramebuffer(g_vulkan->device, node->v, nullptr);
    }
    
    // Cleanup depth resources
    vkDestroyImageView(g_vulkan->device, equip->depth_image_view, nullptr);
    vkDestroyImage(g_vulkan->device, equip->depth_image, nullptr);
    vkFreeMemory(g_vulkan->device, equip->depth_image_memory, nullptr);
    
    // Cleanup render pass
    vkDestroyRenderPass(g_vulkan->device, equip->render_pass, nullptr);
    
    // Cleanup image views
    for (List_Node<VkImageView>* node = equip->swapchain_image_views.first; node; node = node->next)
    {
        vkDestroyImageView(g_vulkan->device, node->v, nullptr);
    }
    
    // Cleanup swap chain
    vkDestroySwapchainKHR(g_vulkan->device, equip->swapchain, nullptr);
    
    // Cleanup surface
    vkDestroySurfaceKHR(g_vulkan->instance, equip->surface, nullptr);
    
    // Note: Arena is managed by the application, not manually released here
}

// Texture management
Renderer_Handle renderer_tex_2d_alloc(Renderer_Resource_Kind kind, Vec2<f32> size, Renderer_Tex_2D_Format format, void* data)
{
    ZoneScoped;
    Arena* arena = arena_alloc();
    Renderer_Vulkan_Texture_2D* tex = push_array(arena, Renderer_Vulkan_Texture_2D, 1);
    *tex = {};
    
    tex->size = size;
    tex->format = format;
    tex->kind = kind;
    
    VkFormat vk_format = renderer_vulkan_format_from_tex_2d_format(format);
    u32 width = (u32)size.x;
    u32 height = (u32)size.y;
    
    // Calculate data size
    u32 bytes_per_pixel = 0;
    switch (format)
    {
        case Renderer_Tex_2D_Format_R8: bytes_per_pixel = 1; break;
        case Renderer_Tex_2D_Format_RG8: bytes_per_pixel = 2; break;
        case Renderer_Tex_2D_Format_RGBA8:
        case Renderer_Tex_2D_Format_BGRA8: bytes_per_pixel = 4; break;
        case Renderer_Tex_2D_Format_R16: bytes_per_pixel = 2; break;
        case Renderer_Tex_2D_Format_RGBA16: bytes_per_pixel = 8; break;
        case Renderer_Tex_2D_Format_R32: bytes_per_pixel = 4; break;
    }
    
    VkDeviceSize image_size = width * height * bytes_per_pixel;
    
    // Create image
    renderer_vulkan_create_image(width, height, vk_format, VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                tex->image, tex->memory);
    
    // Transition image layout and copy data if provided
    if (data)
    {
        // Copy data to staging buffer
        memcpy((u8*)g_vulkan->staging_buffer_mapped + g_vulkan->staging_buffer_offset, data, image_size);
        
        // Transition image layout
        renderer_vulkan_transition_image_layout(tex->image, vk_format,
                                              VK_IMAGE_LAYOUT_UNDEFINED,
                                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        
        // Copy buffer to image
        VkCommandBuffer command_buffer = renderer_vulkan_begin_single_time_commands();
        
        VkBufferImageCopy region{};
        region.bufferOffset = g_vulkan->staging_buffer_offset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        
        vkCmdCopyBufferToImage(command_buffer, g_vulkan->staging_buffer, tex->image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        renderer_vulkan_end_single_time_commands(command_buffer);
        
        // Transition to shader read
        renderer_vulkan_transition_image_layout(tex->image, vk_format,
                                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    else
    {
        // Just transition to shader read
        renderer_vulkan_transition_image_layout(tex->image, vk_format,
                                              VK_IMAGE_LAYOUT_UNDEFINED,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    
    // Create image view
    tex->view = renderer_vulkan_create_image_view(tex->image, vk_format, VK_IMAGE_ASPECT_COLOR_BIT);
    
    Renderer_Handle handle = {};
    handle.u64s[0] = (u64)tex;
    return handle;
}

void renderer_tex_2d_release(Renderer_Handle texture)
{
    ZoneScoped;
    Renderer_Vulkan_Texture_2D* tex = (Renderer_Vulkan_Texture_2D*)texture.u64s[0];
    if (!tex) return;
    
    vkDeviceWaitIdle(g_vulkan->device);
    
    if (tex->view) vkDestroyImageView(g_vulkan->device, tex->view, nullptr);
    if (tex->image) vkDestroyImage(g_vulkan->device, tex->image, nullptr);
    if (tex->memory) vkFreeMemory(g_vulkan->device, tex->memory, nullptr);
    
    // Note: Arena is managed by the application, not manually released here
}

Renderer_Resource_Kind renderer_kind_from_tex_2d(Renderer_Handle texture)
{
    Renderer_Vulkan_Texture_2D* tex = (Renderer_Vulkan_Texture_2D*)texture.u64s[0];
    return tex ? tex->kind : Renderer_Resource_Kind_Static;
}

Vec2<f32> renderer_size_from_tex_2d(Renderer_Handle texture)
{
    Renderer_Vulkan_Texture_2D* tex = (Renderer_Vulkan_Texture_2D*)texture.u64s[0];
    return tex ? tex->size : Vec2<f32>{0, 0};
}

Renderer_Tex_2D_Format renderer_format_from_tex_2d(Renderer_Handle texture)
{
    Renderer_Vulkan_Texture_2D* tex = (Renderer_Vulkan_Texture_2D*)texture.u64s[0];
    return tex ? tex->format : Renderer_Tex_2D_Format_RGBA8;
}

void renderer_fill_tex_2d_region(Renderer_Handle texture, Rng2<f32> subrect, void* data)
{
    ZoneScoped;
    Renderer_Vulkan_Texture_2D* tex = (Renderer_Vulkan_Texture_2D*)texture.u64s[0];
    if (!tex || !data) return;
    
    VkFormat vk_format = renderer_vulkan_format_from_tex_2d_format(tex->format);
    u32 x = (u32)subrect.min.x;
    u32 y = (u32)subrect.min.y;
    u32 width = (u32)(subrect.max.x - subrect.min.x);
    u32 height = (u32)(subrect.max.y - subrect.min.y);
    
    // Calculate data size
    u32 bytes_per_pixel = 0;
    switch (tex->format)
    {
        case Renderer_Tex_2D_Format_R8: bytes_per_pixel = 1; break;
        case Renderer_Tex_2D_Format_RG8: bytes_per_pixel = 2; break;
        case Renderer_Tex_2D_Format_RGBA8:
        case Renderer_Tex_2D_Format_BGRA8: bytes_per_pixel = 4; break;
        case Renderer_Tex_2D_Format_R16: bytes_per_pixel = 2; break;
        case Renderer_Tex_2D_Format_RGBA16: bytes_per_pixel = 8; break;
        case Renderer_Tex_2D_Format_R32: bytes_per_pixel = 4; break;
    }
    
    VkDeviceSize image_size = width * height * bytes_per_pixel;
    
    // Copy data to staging buffer
    memcpy((u8*)g_vulkan->staging_buffer_mapped + g_vulkan->staging_buffer_offset, data, image_size);
    
    // Transition image layout
    renderer_vulkan_transition_image_layout(tex->image, vk_format,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // Copy buffer to image region
    VkCommandBuffer command_buffer = renderer_vulkan_begin_single_time_commands();
    
    VkBufferImageCopy region{};
    region.bufferOffset = g_vulkan->staging_buffer_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {(s32)x, (s32)y, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyBufferToImage(command_buffer, g_vulkan->staging_buffer, tex->image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    renderer_vulkan_end_single_time_commands(command_buffer);
    
    // Transition back to shader read
    renderer_vulkan_transition_image_layout(tex->image, vk_format,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// Buffer management
Renderer_Handle renderer_buffer_alloc(Renderer_Resource_Kind kind, u64 size, void* data)
{
    ZoneScoped;
    Arena* arena = arena_alloc();
    Renderer_Vulkan_Buffer* buf = push_array(arena, Renderer_Vulkan_Buffer, 1);
    *buf = {};
    
    buf->size = size;
    buf->kind = kind;
    
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VkMemoryPropertyFlags properties = 0;
    
    if (kind == Renderer_Resource_Kind_Dynamic)
    {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else
    {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    
    renderer_vulkan_create_buffer(size, usage, properties, buf->buffer, buf->memory);
    
    if (kind == Renderer_Resource_Kind_Dynamic)
    {
        vkMapMemory(g_vulkan->device, buf->memory, 0, size, 0, &buf->mapped);
    }
    
    if (data)
    {
        if (kind == Renderer_Resource_Kind_Dynamic)
        {
            memcpy(buf->mapped, data, size);
        }
        else
        {
            // Use staging buffer
            memcpy((u8*)g_vulkan->staging_buffer_mapped + g_vulkan->staging_buffer_offset, data, size);
            
            VkCommandBuffer command_buffer = renderer_vulkan_begin_single_time_commands();
            
            VkBufferCopy copy_region{};
            copy_region.srcOffset = g_vulkan->staging_buffer_offset;
            copy_region.dstOffset = 0;
            copy_region.size = size;
            
            vkCmdCopyBuffer(command_buffer, g_vulkan->staging_buffer, buf->buffer, 1, &copy_region);
            
            renderer_vulkan_end_single_time_commands(command_buffer);
        }
    }
    
    Renderer_Handle handle = {};
    handle.u64s[0] = (u64)buf;
    return handle;
}

void renderer_buffer_release(Renderer_Handle buffer)
{
    ZoneScoped;
    Renderer_Vulkan_Buffer* buf = (Renderer_Vulkan_Buffer*)buffer.u64s[0];
    if (!buf) return;
    
    vkDeviceWaitIdle(g_vulkan->device);
    
    if (buf->mapped) vkUnmapMemory(g_vulkan->device, buf->memory);
    if (buf->buffer) vkDestroyBuffer(g_vulkan->device, buf->buffer, nullptr);
    if (buf->memory) vkFreeMemory(g_vulkan->device, buf->memory, nullptr);
    
    // Note: Arena is managed by the application, not manually released here
}

// Frame management
void renderer_begin_frame()
{
    ZoneScoped;
    // Reset staging buffer offset
    g_vulkan->staging_buffer_offset = 0;
}

void renderer_end_frame()
{
    ZoneScoped;
    // Nothing to do here for Vulkan
}

void renderer_window_begin_frame(void* window, Renderer_Handle window_equip)
{
    ZoneScoped;
    Renderer_Vulkan_Window_Equipment* equip = (Renderer_Vulkan_Window_Equipment*)window_equip.u64s[0];
    if (!equip) return;
    
    // Wait for previous frame
    vkWaitForFences(g_vulkan->device, 1, &equip->in_flight_fences[equip->current_frame], VK_TRUE, UINT64_MAX);
    
    // Acquire next image
    VkResult result = vkAcquireNextImageKHR(g_vulkan->device, equip->swapchain, UINT64_MAX,
                                           equip->image_available_semaphores[equip->current_frame],
                                           VK_NULL_HANDLE, &equip->current_image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        // Recreate swapchain
        void renderer_vulkan_recreate_swapchain(Renderer_Vulkan_Window_Equipment* equip);
        renderer_vulkan_recreate_swapchain(equip);
        return;
    }
    
    // Reset fence
    vkResetFences(g_vulkan->device, 1, &equip->in_flight_fences[equip->current_frame]);
    
    // Begin command buffer
    VkCommandBuffer cmd = equip->command_buffers[equip->current_frame];
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    vkBeginCommandBuffer(cmd, &begin_info);
}

void renderer_window_end_frame(void* window, Renderer_Handle window_equip)
{
    ZoneScoped;
    Renderer_Vulkan_Window_Equipment* equip = (Renderer_Vulkan_Window_Equipment*)window_equip.u64s[0];
    if (!equip) return;
    
    VkCommandBuffer cmd = equip->command_buffers[equip->current_frame];
    
    // End command buffer
    vkEndCommandBuffer(cmd);
    
    // Submit command buffer
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore wait_semaphores[] = {equip->image_available_semaphores[equip->current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    
    VkSemaphore signal_semaphores[] = {equip->render_finished_semaphores[equip->current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    
    vkQueueSubmit(g_vulkan->graphics_queue, 1, &submit_info, equip->in_flight_fences[equip->current_frame]);
    
    // Present
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    
    VkSwapchainKHR swapchains[] = {equip->swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    
    u32 image_indices[] = {equip->current_image_index};
    present_info.pImageIndices = image_indices;
    
    VkResult result = vkQueuePresentKHR(g_vulkan->present_queue, &present_info);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        // Recreate swapchain
        renderer_vulkan_recreate_swapchain(equip);
    }
    
    // Update frame index
    equip->current_frame = (equip->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// Implementation moved to renderer_vulkan_passes.cpp