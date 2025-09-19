#include "renderer_metal.h"
#include "../base/base_inc.h"
#include "renderer_metal_internal.h"
#include <assert.h>
#include <string.h>

#import <Cocoa/Cocoa.h>
#import <CoreVideo/CoreVideo.h>
#import <IOKit/graphics/IOGraphicsLib.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#import <dispatch/dispatch.h>



Renderer_Metal_State *r_metal_state = NULL;

extern const char *renderer_metal_rect_shader_src;
extern const char *renderer_metal_blur_shader_src;
extern const char *renderer_metal_mesh_shader_src;

MTLPixelFormat
renderer_metal_pixel_format_from_tex_2d_format(Renderer_Tex_2D_Format fmt)
{
    switch (fmt)
    {
    case Renderer_Tex_2D_Format_R8:
        return MTLPixelFormatR8Unorm;
    case Renderer_Tex_2D_Format_RG8:
        return MTLPixelFormatRG8Unorm;
    case Renderer_Tex_2D_Format_RGBA8:
        return MTLPixelFormatRGBA8Unorm;
    case Renderer_Tex_2D_Format_BGRA8:
        return MTLPixelFormatBGRA8Unorm;
    case Renderer_Tex_2D_Format_R16:
        return MTLPixelFormatR16Unorm;
    case Renderer_Tex_2D_Format_RGBA16:
        return MTLPixelFormatRGBA16Unorm;
    case Renderer_Tex_2D_Format_R32:
        return MTLPixelFormatR32Float;
    default:
        assert(0 && "Unknown texture format");
        return MTLPixelFormatRGBA8Unorm;
    }
}

Mat4x4_f32
renderer_metal_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format fmt)
{
    Mat4x4_f32 result = mat4x4_identity();

    switch (fmt)
    {
    case Renderer_Tex_2D_Format_R8:
    case Renderer_Tex_2D_Format_R16:
    case Renderer_Tex_2D_Format_R32:
        result.m[0][0] = 1.0f;
        result.m[0][1] = 1.0f;
        result.m[0][2] = 1.0f;
        result.m[0][3] = 0.0f;
        result.m[1][3] = 0.0f;
        result.m[2][3] = 0.0f;
        result.m[3][0] = 0.0f;
        result.m[3][1] = 0.0f;
        result.m[3][2] = 0.0f;
        result.m[3][3] = 1.0f;
        break;

    case Renderer_Tex_2D_Format_RG8:
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][0] = 0.0f;
        result.m[2][1] = 0.0f;
        result.m[2][2] = 0.0f;
        result.m[2][3] = 0.0f;
        result.m[3][0] = 0.0f;
        result.m[3][1] = 0.0f;
        result.m[3][2] = 0.0f;
        result.m[3][3] = 1.0f;
        break;

    case Renderer_Tex_2D_Format_RGBA8:
    case Renderer_Tex_2D_Format_BGRA8:
    case Renderer_Tex_2D_Format_RGBA16:
        break;
    }

    return result;
}

void
renderer_init()
{

    if (r_metal_state)
    {
        assert(0 && "Metal renderer already initialized");
        return;
    }

    Arena *arena = arena_alloc();
    r_metal_state = push_array(arena, Renderer_Metal_State, 1);
    MemoryZeroStruct(r_metal_state);
    r_metal_state->arena = arena;

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device)
    {
        assert(0 && "Failed to create Metal device");
        return;
    }
    r_metal_state->device = metal_retain(device);

    r_metal_state->command_queue = metal_retain([device newCommandQueue]);
    if (!r_metal_state->command_queue)
    {
        assert(0 && "Failed to create Metal command queue");
        arena_release(arena);
        r_metal_state = NULL;
        return;
    }

    r_metal_state->texture_cap = 1024;
    r_metal_state->textures = push_array(arena, Renderer_Metal_Tex_2D, r_metal_state->texture_cap);

    r_metal_state->buffer_cap = 1024;
    r_metal_state->buffers = push_array(arena, Renderer_Metal_Buffer, r_metal_state->buffer_cap);

    r_metal_state->window_equip_cap = 16;
    r_metal_state->window_equips = push_array(arena, Renderer_Metal_Window_Equip, r_metal_state->window_equip_cap);

    renderer_metal_init_shaders();

    // Create blur sampler (linear filtering for smooth blur)
    MTLSamplerDescriptor *sampler_desc = [MTLSamplerDescriptor new];
    sampler_desc.minFilter = MTLSamplerMinMagFilterLinear;
    sampler_desc.magFilter = MTLSamplerMinMagFilterLinear;
    sampler_desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    r_metal_state->blur_sampler = metal_retain([device newSamplerStateWithDescriptor:sampler_desc]);
    
    // Create font sampler (linear filtering for smooth text)
    MTLSamplerDescriptor *font_sampler_desc = [MTLSamplerDescriptor new];
    font_sampler_desc.minFilter = MTLSamplerMinMagFilterLinear;
    font_sampler_desc.magFilter = MTLSamplerMinMagFilterLinear;
    font_sampler_desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    font_sampler_desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    r_metal_state->font_sampler = metal_retain([device newSamplerStateWithDescriptor:font_sampler_desc]);

    // Initialize frame data for triple buffering
    for (u32 i = 0; i < METAL_FRAMES_IN_FLIGHT; i++)
    {
        // Create semaphore for frame synchronization
        // Start with value 1 - frame is initially available
        r_metal_state->frames[i].semaphore = dispatch_semaphore_create(1);

        // Create instance buffer per frame (start larger to reduce reallocations)
        u64 rect_buffer_size = 256 * 1024; // Start with 256KB instead of 64KB
        r_metal_state->frames[i].rect_instance_buffer_size = rect_buffer_size;
        r_metal_state->frames[i].rect_instance_buffer = metal_retain([device newBufferWithLength:rect_buffer_size
                                                                                         options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
        if (!r_metal_state->frames[i].rect_instance_buffer)
        {
            assert(0);
        }

        // Create mesh uniform buffer per frame
        u64 mesh_uniform_buffer_size = 16 * 1024;
        r_metal_state->frames[i].mesh_uniform_buffer = metal_retain([device newBufferWithLength:mesh_uniform_buffer_size
                                                                                        options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
        if (!r_metal_state->frames[i].mesh_uniform_buffer)
        {
            assert(0);
        }

        // Initialize buffer pool entries
        for (u32 j = 0; j < METAL_BUFFER_POOL_SIZE; j++)
        {
            r_metal_state->frames[i].buffer_pool[j].buffer = NULL;
            r_metal_state->frames[i].buffer_pool[j].size = 0;
            r_metal_state->frames[i].buffer_pool[j].in_use = false;
        }
    }
    r_metal_state->current_frame_index = 0;

    // Create shared rect vertex buffer
    Vec2_f32 rect_vertices[] = {
        {{-1.0f, -1.0f}},
        {{1.0f, -1.0f}},
        {{-1.0f, 1.0f}},
        {{1.0f, 1.0f}},
    };
    r_metal_state->rect_vertex_buffer = metal_retain([device newBufferWithBytes:rect_vertices
                                                                         length:sizeof(rect_vertices)
                                                                        options:MTLResourceStorageModeManaged]);
    [metal_buffer(r_metal_state->rect_vertex_buffer) didModifyRange:NSMakeRange(0, sizeof(rect_vertices))];

    // Create cached render pass descriptors
    r_metal_state->ui_render_pass_desc = metal_retain([MTLRenderPassDescriptor renderPassDescriptor]);
    r_metal_state->blur_render_pass_desc = metal_retain([MTLRenderPassDescriptor renderPassDescriptor]);
    r_metal_state->geo_render_pass_desc = metal_retain([MTLRenderPassDescriptor renderPassDescriptor]);

    printf("Metal renderer initialized successfully\n");
    printf("Using %d frames in flight with %d maximum drawables", METAL_FRAMES_IN_FLIGHT, METAL_FRAMES_IN_FLIGHT);
}

Renderer_Handle
renderer_window_equip(OS_Handle handle)
{
    Prof_Begin(__FUNCTION__);
    void *window = os_window_native_handle(handle);
    if (!r_metal_state || !window)
    {
        return renderer_handle_zero();
    }

    NSWindow *ns_window = (__bridge NSWindow *)window;

    // Find free slot
    u64 slot = 0;
    for (; slot < r_metal_state->window_equip_count; slot++)
    {
        if (!r_metal_state->window_equips[slot].layer)
        {
            break;
        }
    }

    if (slot >= r_metal_state->window_equip_cap)
    {
        assert(0 && "Too many window equips");
        return renderer_handle_zero();
    }

    if (slot >= r_metal_state->window_equip_count)
    {
        r_metal_state->window_equip_count = slot + 1;
    }

    Renderer_Metal_Window_Equip *equip = &r_metal_state->window_equips[slot];
    MemoryZeroStruct(equip);

    // Create Metal layer
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = metal_device(r_metal_state->device);
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    layer.displaySyncEnabled = NO;
    layer.allowsNextDrawableTimeout = NO; // Don't wait for drawable timeout
    layer.presentsWithTransaction = NO;

    // Use maximum drawables for better throughput
    layer.maximumDrawableCount = 3; // Max allowed, even with 2 frames in flight
    layer.wantsExtendedDynamicRangeContent = NO;

    if ([layer respondsToSelector:@selector(setDisplaySyncEnabled:)])
    {
        [layer setDisplaySyncEnabled:NO];
    }

    // Note: lowLatency property is not available on CAMetalLayer
    // It's only available on MTKView

    // Additional attempts to disable vsync
    // Disable implicit animations
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [CATransaction setAnimationDuration:0.0];

    // Set layer properties that might affect vsync
    layer.opaque = YES;
    [layer removeAllAnimations];

    [CATransaction commit];

    ns_window.contentView.layer = layer;
    ns_window.contentView.wantsLayer = YES;
    layer.contentsScale = ns_window.backingScaleFactor;

    // Additional vsync bypass attempts
    layer.opaque = YES;

    // Try to disable automatic synchronization with the display
    if ([ns_window respondsToSelector:@selector(setAllowsConcurrentViewDrawing:)])
    {
        [ns_window setAllowsConcurrentViewDrawing:YES];
    }

    equip->layer = metal_retain(layer);

    NSRect frame = ns_window.contentView.frame;
    equip->size = (Vec2_f32){{(f32)frame.size.width, (f32)frame.size.height}};
    equip->scale = layer.contentsScale;
    CGSize drawableSize = CGSizeMake(frame.size.width * layer.contentsScale,
                                     frame.size.height * layer.contentsScale);
    layer.drawableSize = drawableSize;

    MTLTextureDescriptor *depth_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                          width:(NSUInteger)drawableSize.width
                                                                                         height:(NSUInteger)drawableSize.height
                                                                                      mipmapped:NO];
    depth_desc.textureType = MTLTextureType2DMultisample;
    depth_desc.sampleCount = 4; // 4x MSAA
    depth_desc.usage = MTLTextureUsageRenderTarget;
    depth_desc.storageMode = MTLStorageModePrivate;

    equip->depth_texture = metal_retain([metal_device(r_metal_state->device) newTextureWithDescriptor:depth_desc]);

    Renderer_Handle result = renderer_handle_zero();
    result.u64s[0] = slot + 1;
    return result;
}

void
renderer_window_unequip(OS_Handle handle, Renderer_Handle window_equip)
{
    if (!r_metal_state || window_equip.u64s[0] == 0)
    {
        return;
    }

    u64 slot = window_equip.u64s[0] - 1;
    if (slot >= r_metal_state->window_equip_count)
    {
        return;
    }

    Renderer_Metal_Window_Equip *equip = &r_metal_state->window_equips[slot];

    if (equip->depth_texture)
    {
        metal_release(equip->depth_texture);
    }

    MemoryZeroStruct(equip);
}

Renderer_Handle
renderer_tex_2d_alloc(Renderer_Resource_Kind kind, Vec2_f32 size, Renderer_Tex_2D_Format format, void *data)
{
    Prof_Begin(__FUNCTION__);
    if (!r_metal_state)
    {
        return renderer_handle_zero();
    }

    // Find free slot
    u64 slot = 0;
    for (; slot < r_metal_state->texture_count; slot++)
    {
        if (!r_metal_state->textures[slot].texture)
        {
            break;
        }
    }

    if (slot >= r_metal_state->texture_cap)
    {
        assert(0 && "Too many textures");
        return renderer_handle_zero();
    }

    if (slot >= r_metal_state->texture_count)
    {
        r_metal_state->texture_count = slot + 1;
    }

    Renderer_Metal_Tex_2D *tex = &r_metal_state->textures[slot];
    MemoryZeroStruct(tex);

    tex->size = size;
    tex->format = format;
    tex->kind = kind;

    // Create texture descriptor
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:renderer_metal_pixel_format_from_tex_2d_format(format)
                                                                                    width:(NSUInteger)size.x
                                                                                   height:(NSUInteger)size.y
                                                                                mipmapped:NO];

    desc.usage = MTLTextureUsageShaderRead;
    if (kind == Renderer_Resource_Kind_Dynamic)
    {
        desc.usage |= MTLTextureUsageShaderWrite;
    }

    // Create texture
    tex->texture = metal_retain([metal_device(r_metal_state->device) newTextureWithDescriptor:desc]);

    // Upload initial data if provided
    if (data)
    {
        Prof_Begin("MetalTextureUpload");
        NSUInteger bytes_per_row = 0;
        switch (format)
        {
        case Renderer_Tex_2D_Format_R8:
            bytes_per_row = (NSUInteger)size.x;
            break;
        case Renderer_Tex_2D_Format_RG8:
            bytes_per_row = (NSUInteger)size.x * 2;
            break;
        case Renderer_Tex_2D_Format_RGBA8:
        case Renderer_Tex_2D_Format_BGRA8:
            bytes_per_row = (NSUInteger)size.x * 4;
            break;
        case Renderer_Tex_2D_Format_R16:
            bytes_per_row = (NSUInteger)size.x * 2;
            break;
        case Renderer_Tex_2D_Format_RGBA16:
            bytes_per_row = (NSUInteger)size.x * 8;
            break;
        case Renderer_Tex_2D_Format_R32:
            bytes_per_row = (NSUInteger)size.x * 4;
            break;
        }

        [metal_texture(tex->texture) replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)size.x, (NSUInteger)size.y)
                                       mipmapLevel:0
                                         withBytes:data
                                       bytesPerRow:bytes_per_row];
    }

    // Create sampler - default to linear filtering for most textures
    // Font textures will use the dedicated font_sampler with nearest filtering
    MTLSamplerDescriptor *sampler_desc = [MTLSamplerDescriptor new];
    sampler_desc.minFilter = MTLSamplerMinMagFilterLinear;
    sampler_desc.magFilter = MTLSamplerMinMagFilterLinear;
    sampler_desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_desc.tAddressMode = MTLSamplerAddressModeClampToEdge;

    tex->sampler = metal_retain([metal_device(r_metal_state->device) newSamplerStateWithDescriptor:sampler_desc]);

    Renderer_Handle handle = renderer_handle_zero();
    handle.u64s[0] = slot + 1;
    return handle;
}

void
renderer_tex_2d_release(Renderer_Handle texture)
{
    if (!r_metal_state || texture.u64s[0] == 0)
    {
        return;
    }

    u64 slot = texture.u64s[0] - 1;
    if (slot >= r_metal_state->texture_count)
    {
        return;
    }

    Renderer_Metal_Tex_2D *tex = &r_metal_state->textures[slot];

    if (tex->texture)
    {
        metal_release(tex->texture);
    }
    if (tex->sampler)
    {
        metal_release(tex->sampler);
    }

    MemoryZeroStruct(tex);
}

Renderer_Resource_Kind
renderer_kind_from_tex_2d(Renderer_Handle texture)
{
    if (!r_metal_state || texture.u64s[0] == 0)
    {
        return Renderer_Resource_Kind_Static;
    }

    u64 slot = texture.u64s[0] - 1;
    if (slot >= r_metal_state->texture_count)
    {
        return Renderer_Resource_Kind_Static;
    }

    return r_metal_state->textures[slot].kind;
}

Vec2_f32
renderer_size_from_tex_2d(Renderer_Handle texture)
{
    if (!r_metal_state || texture.u64s[0] == 0)
    {
        return (Vec2_f32){{0.0f, 0.0f}};
    }

    u64 slot = texture.u64s[0] - 1;
    if (slot >= r_metal_state->texture_count)
    {
        return (Vec2_f32){{0.0f, 0.0f}};
    }

    return r_metal_state->textures[slot].size;
}

Renderer_Tex_2D_Format
renderer_format_from_tex_2d(Renderer_Handle texture)
{
    if (!r_metal_state || texture.u64s[0] == 0)
    {
        return Renderer_Tex_2D_Format_RGBA8;
    }

    u64 slot = texture.u64s[0] - 1;
    if (slot >= r_metal_state->texture_count)
    {
        return Renderer_Tex_2D_Format_RGBA8;
    }

    return r_metal_state->textures[slot].format;
}

void
renderer_fill_tex_2d_region(Renderer_Handle texture, Rng2_f32 subrect, void *data)
{
    if (!r_metal_state || texture.u64s[0] == 0 || !data)
    {
        return;
    }

    u64 slot = texture.u64s[0] - 1;
    if (slot >= r_metal_state->texture_count)
    {
        return;
    }

    Renderer_Metal_Tex_2D *tex = &r_metal_state->textures[slot];
    if (!tex->texture)
    {
        return;
    }

    NSUInteger bytes_per_row = 0;
    u64        width = (u64)(subrect.max.x - subrect.min.x);

    switch (tex->format)
    {
    case Renderer_Tex_2D_Format_R8:
        bytes_per_row = width;
        break;
    case Renderer_Tex_2D_Format_RG8:
        bytes_per_row = width * 2;
        break;
    case Renderer_Tex_2D_Format_RGBA8:
    case Renderer_Tex_2D_Format_BGRA8:
        bytes_per_row = width * 4;
        break;
    case Renderer_Tex_2D_Format_R16:
        bytes_per_row = width * 2;
        break;
    case Renderer_Tex_2D_Format_RGBA16:
        bytes_per_row = width * 8;
        break;
    case Renderer_Tex_2D_Format_R32:
        bytes_per_row = width * 4;
        break;
    }

    [metal_texture(tex->texture) replaceRegion:MTLRegionMake2D((NSUInteger)subrect.min.x,
                                                               (NSUInteger)subrect.min.y,
                                                               (NSUInteger)(subrect.max.x - subrect.min.x),
                                                               (NSUInteger)(subrect.max.y - subrect.min.y))
                                   mipmapLevel:0
                                     withBytes:data
                                   bytesPerRow:bytes_per_row];
}

Renderer_Handle
renderer_buffer_alloc(Renderer_Resource_Kind kind, u64 size, void *data)
{
    Prof_Begin(__FUNCTION__);
    if (!r_metal_state)
    {
        return renderer_handle_zero();
    }

    // Find free slot
    u64 slot = 0;
    for (; slot < r_metal_state->buffer_count; slot++)
    {
        if (!r_metal_state->buffers[slot].buffer)
        {
            break;
        }
    }

    if (slot >= r_metal_state->buffer_cap)
    {
        assert(0 && "Too many buffers");
        return renderer_handle_zero();
    }

    if (slot >= r_metal_state->buffer_count)
    {
        r_metal_state->buffer_count = slot + 1;
    }

    Renderer_Metal_Buffer *buf = &r_metal_state->buffers[slot];
    MemoryZeroStruct(buf);

    buf->size = size;
    buf->kind = kind;

    MTLResourceOptions options;
    if (kind == Renderer_Resource_Kind_Dynamic)
    {
        // Dynamic buffers need CPU access
        options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
    }
    else
    {
        // Static buffers should use managed storage for better GPU performance
        options = MTLResourceStorageModeManaged;
    }

    if (data)
    {
        Prof_Begin("MetalBufferAllocWithData");
        buf->buffer = metal_retain([metal_device(r_metal_state->device) newBufferWithBytes:data
                                                                                    length:size
                                                                                   options:options]);

        // For managed buffers, we need to inform Metal that CPU modified the contents
        if (kind == Renderer_Resource_Kind_Static)
        {
            [metal_buffer(buf->buffer) didModifyRange:NSMakeRange(0, size)];
        }
    }
    else
    {
        Prof_Begin("MetalBufferAlloc");
        buf->buffer = metal_retain([metal_device(r_metal_state->device) newBufferWithLength:size
                                                                                    options:options]);
    }

    Renderer_Handle handle = renderer_handle_zero();
    handle.u64s[0] = slot + 1;
    return handle;
}

void
renderer_buffer_release(Renderer_Handle buffer)
{
    Prof_Begin(__FUNCTION__);
    if (!r_metal_state || buffer.u64s[0] == 0)
    {
        return;
    }

    u64 slot = buffer.u64s[0] - 1;
    if (slot >= r_metal_state->buffer_count)
    {
        return;
    }

    Renderer_Metal_Buffer *buf = &r_metal_state->buffers[slot];

    if (buf->buffer)
    {
        metal_release(buf->buffer);
    }

    MemoryZeroStruct(buf);
}

void
renderer_begin_frame()
{
    Prof_Begin(__FUNCTION__);
    // Global frame begin operations if needed
}

void
renderer_end_frame()
{
    Prof_Begin(__FUNCTION__);
    // Global frame end operations if needed
}

void
renderer_window_begin_frame(OS_Handle handle, Renderer_Handle window_equip)
{
    Prof_Begin(__FUNCTION__);
    if (!r_metal_state || window_equip.u64s[0] == 0)
    {
        return;
    }

    u64 slot = window_equip.u64s[0] - 1;
    if (slot >= r_metal_state->window_equip_count)
    {
        return;
    }

    Renderer_Metal_Window_Equip *equip = &r_metal_state->window_equips[slot];
    void *window = os_window_native_handle(handle);
    NSWindow                    *ns_window = (__bridge NSWindow *)window;
    NSRect                       frame = ns_window.contentView.frame;
    Vec2_f32                    new_size = (Vec2_f32){{(f32)frame.size.width, (f32)frame.size.height}};

    if (new_size.x != equip->size.x || new_size.y != equip->size.y)
    {
        equip->size = new_size;
        CAMetalLayer *layer = metal_layer(equip->layer);
        CGFloat       scale = ns_window.backingScaleFactor;
        equip->scale = scale;
        layer.contentsScale = scale;
        layer.drawableSize = CGSizeMake(new_size.x * scale, new_size.y * scale);

        if (equip->depth_texture)
        {
            metal_release(equip->depth_texture);
        }

        MTLTextureDescriptor *depth_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                              width:(NSUInteger)(new_size.x * scale)
                                                                                             height:(NSUInteger)(new_size.y * scale)
                                                                                          mipmapped:NO];
        depth_desc.textureType = MTLTextureType2DMultisample;
        depth_desc.sampleCount = 4; // 4x MSAA
        depth_desc.usage = MTLTextureUsageRenderTarget;
        depth_desc.storageMode = MTLStorageModePrivate;

        equip->depth_texture = metal_retain([metal_device(r_metal_state->device) newTextureWithDescriptor:depth_desc]);
    }

    // Reset buffer pool for current frame
    Renderer_Metal_Frame_Data *frame_data = &r_metal_state->frames[r_metal_state->current_frame_index];
    for (u32 i = 0; i < METAL_BUFFER_POOL_SIZE; i++)
    {
        frame_data->buffer_pool[i].in_use = false;
    }
}

void
renderer_window_end_frame(OS_Handle handle, Renderer_Handle window_equip)
{
    Prof_Begin(__FUNCTION__);
    // Any per-window cleanup
}

void
renderer_window_submit(OS_Handle window, Renderer_Handle window_equip, Renderer_Pass_List *passes)
{
    if (!r_metal_state || window_equip.u64s[0] == 0 || !passes)
    {
        return;
    }

    u64 slot = window_equip.u64s[0] - 1;
    if (slot >= r_metal_state->window_equip_count)
    {
        return;
    }

    Renderer_Metal_Window_Equip *equip = &r_metal_state->window_equips[slot];

    Renderer_Metal_Frame_Data *frame = &r_metal_state->frames[r_metal_state->current_frame_index];

    dispatch_semaphore_wait((dispatch_semaphore_t)frame->semaphore, DISPATCH_TIME_FOREVER);

    CAMetalLayer *layer = metal_layer(equip->layer);

    id<MTLCommandBuffer> command_buffer = [metal_command_queue(r_metal_state->command_queue) commandBuffer];

    id<CAMetalDrawable> drawable = NULL;
    {
        Prof_Begin("MetalGetDrawable");
        [CATransaction begin];
        [CATransaction setDisableActions:YES];

        drawable = [layer nextDrawable];

        [CATransaction commit];

        if (!drawable)
        {
            static u64 drawable_miss_count = 0;
            if ((++drawable_miss_count % 100) == 0)
            {
                printf("Missed %d drawables - may be vsync limited", (int)drawable_miss_count);
            }
        }
    }
    if (!drawable)
    {
        dispatch_semaphore_signal((dispatch_semaphore_t)frame->semaphore);
        static u64 consecutive_failures = 0;
        if (++consecutive_failures > 10)
        {
            assert(0);
        }
        return;
    }

    [command_buffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
      dispatch_semaphore_signal((dispatch_semaphore_t)frame->semaphore);
    }];

    Renderer_Pass_Params_UI     *ui_params = NULL;
    Renderer_Pass_Params_Geo_3D *geo_params = NULL;
    bool                         has_blur = false;

    {
        Prof_Begin("MetalAnalyzePasses");
        for (Renderer_Pass_Node *pass_node = passes->first; pass_node; pass_node = pass_node->next)
        {
            Renderer_Pass *pass = &pass_node->v;
            switch (pass->kind)
            {
            case Renderer_Pass_Kind_UI:
                ui_params = pass->params_ui;
                break;
            case Renderer_Pass_Kind_Geo_3D:
                geo_params = pass->params_geo_3d;
                break;
            case Renderer_Pass_Kind_Blur:
                has_blur = true;
                break;
            }
        }
    }

    if ((ui_params || geo_params) && !has_blur)
    {
        renderer_metal_render_pass_combined(ui_params, geo_params, command_buffer, drawable.texture, equip->depth_texture, equip);
    }
    else
    {
        for (Renderer_Pass_Node *pass_node = passes->first; pass_node; pass_node = pass_node->next)
        {
            Renderer_Pass *pass = &pass_node->v;

            switch (pass->kind)
            {
            case Renderer_Pass_Kind_UI:
                renderer_metal_render_pass_ui(pass->params_ui, command_buffer, drawable.texture, equip);
                break;

            case Renderer_Pass_Kind_Blur:
                renderer_metal_render_pass_blur(pass->params_blur, command_buffer, drawable.texture);
                break;

            case Renderer_Pass_Kind_Geo_3D:
                renderer_metal_render_pass_geo_3d(pass->params_geo_3d, command_buffer, drawable.texture, equip->depth_texture, equip);
                break;
            }
        }
    }

    [command_buffer presentDrawable:drawable];
    [command_buffer commit];

    renderer_metal_reset_frame_pools();

    r_metal_state->current_frame_index = (r_metal_state->current_frame_index + 1) % METAL_FRAMES_IN_FLIGHT;
}

void *
renderer_metal_acquire_buffer_from_pool(u64 size)
{
    Renderer_Metal_Frame_Data *frame = &r_metal_state->frames[r_metal_state->current_frame_index];

    size = (size + 255) & ~255;
    for (u32 i = 0; i < METAL_BUFFER_POOL_SIZE; i++)
    {
        Renderer_Metal_Buffer_Pool_Entry *entry = &frame->buffer_pool[i];
        if (!entry->in_use && entry->buffer && entry->size >= size && entry->size <= size * 2)
        {
            entry->in_use = true;
            entry->used_size = size;
            entry->reuse_count++;
            frame->buffer_pool_hit_count++;
            return entry->buffer;
        }
    }

    for (u32 i = 0; i < METAL_BUFFER_POOL_SIZE; i++)
    {
        Renderer_Metal_Buffer_Pool_Entry *entry = &frame->buffer_pool[i];
        if (!entry->in_use && entry->buffer && entry->size >= size)
        {
            entry->in_use = true;
            entry->used_size = size;
            entry->reuse_count++;
            frame->buffer_pool_hit_count++;
            return entry->buffer;
        }
    }

    u32 best_slot = METAL_BUFFER_POOL_SIZE;
    u32 min_reuse_count = UINT32_MAX;

    for (u32 i = 0; i < METAL_BUFFER_POOL_SIZE; i++)
    {
        Renderer_Metal_Buffer_Pool_Entry *entry = &frame->buffer_pool[i];
        if (!entry->in_use)
        {
            if (!entry->buffer || entry->reuse_count < min_reuse_count)
            {
                best_slot = i;
                min_reuse_count = entry->reuse_count;
            }
        }
    }

    if (best_slot < METAL_BUFFER_POOL_SIZE)
    {
        Renderer_Metal_Buffer_Pool_Entry *entry = &frame->buffer_pool[best_slot];

        if (entry->buffer && entry->size < size)
        {
            metal_release(entry->buffer);
            entry->buffer = NULL;
        }

        if (!entry->buffer)
        {
            u64 alloc_size = Max(size, METAL_BUFFER_POOL_MIN_SIZE);
            if (size > METAL_BUFFER_POOL_MIN_SIZE)
            {
                alloc_size = (size * METAL_BUFFER_POOL_GROWTH_FACTOR + 4095) & ~4095;
            }

            entry->size = alloc_size;
            entry->buffer = metal_retain([metal_device(r_metal_state->device)
                newBufferWithLength:alloc_size
                            options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
            if (!entry->buffer)
            {
                assert(0);
                return NULL;
            }
            entry->reuse_count = 0;
        }

        entry->in_use = true;
        entry->used_size = size;
        frame->buffer_pool_miss_count++;
        return entry->buffer;
    }

    frame->buffer_pool_miss_count++;
    return NULL;
}

void
renderer_metal_reset_frame_pools()
{
    Renderer_Metal_Frame_Data *frame = &r_metal_state->frames[r_metal_state->current_frame_index];

    for (u32 i = 0; i < METAL_BUFFER_POOL_SIZE; i++)
    {
        frame->buffer_pool[i].in_use = false;
        frame->buffer_pool[i].used_size = 0;
    }

    frame->rect_instance_buffer_used = 0;
    frame->mesh_uniform_buffer_used = 0;

    static u64 frame_count = 0;
    if ((++frame_count % 1000) == 0)
    {
        u32 total = frame->buffer_pool_hit_count + frame->buffer_pool_miss_count;
        if (total > 0)
        {
            f32 hit_rate = (f32)frame->buffer_pool_hit_count / (f32)total * 100.0f;
            printf("Buffer pool hit rate: %f%% (hits: %d, misses: %d)",
                     hit_rate, frame->buffer_pool_hit_count, frame->buffer_pool_miss_count);
        }
        else
        {
            printf("Buffer pool not used in last 1000 frames");
        }

        frame->buffer_pool_hit_count = 0;
        frame->buffer_pool_miss_count = 0;
    }
}
