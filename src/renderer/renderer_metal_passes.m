#include "renderer_metal.h"
#include "renderer_metal_internal.h"
#include "../base/base_inc.h"

#import <Metal/Metal.h>
#import <simd/simd.h>

typedef struct RectUniforms RectUniforms;
struct RectUniforms
{
    Vec2_f32   viewport_size_px;
    f32         opacity;
    f32         _padding;
    Mat4x4_f32 texture_sample_channel_map;
};

typedef struct BlurUniforms BlurUniforms;
struct BlurUniforms
{
    Vec2_f32 viewport_size;
    Vec2_f32 rect_min;
    Vec2_f32 rect_max;
    Vec2_f32 clip_min;
    Vec2_f32 clip_max;
    f32       blur_size;
    f32       corner_radii[4];
};

typedef struct MeshUniforms MeshUniforms;
struct MeshUniforms
{
    Mat4x4_f32 view;
    Mat4x4_f32 projection;
    Vec2_f32   viewport_size;
    Vec2_f32   clip_min;
    Vec2_f32   clip_max;
};

// Helper function to get buffer from pool
static id<MTLBuffer>
renderer_metal_get_temp_buffer(u64 size)
{
    void *buffer = renderer_metal_acquire_buffer_from_pool(size);
    return buffer ? metal_buffer(buffer) : NULL;
}

void
renderer_metal_render_pass_ui(Renderer_Pass_Params_UI *params, void *command_buffer, void *target_texture, Renderer_Metal_Window_Equip *equip)
{
    ZoneScopedN("MetalRenderPassUI");
    if (!r_metal_state || !params || !command_buffer || !target_texture)
    {
        return;
    }

    id<MTLCommandBuffer> mtl_command_buffer = metal_command_buffer(command_buffer);
    id<MTLTexture>       mtl_target_texture = metal_texture(target_texture);

    // Use cached render pass descriptor
    MTLRenderPassDescriptor *render_pass_desc = (MTLRenderPassDescriptor *)r_metal_state->ui_render_pass_desc;
    render_pass_desc.colorAttachments[0].texture = mtl_target_texture;
    render_pass_desc.colorAttachments[0].loadAction = MTLLoadActionClear;
    render_pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    render_pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.1, 1.0); // Slightly lighter background

    id<MTLRenderCommandEncoder> encoder = [mtl_command_buffer renderCommandEncoderWithDescriptor:render_pass_desc];
    [encoder setRenderPipelineState:metal_pipeline_state(r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Rect].render_pipeline_state)];

    // Process each batch group
    for (Renderer_Batch_Group_2D_Node *node = params->rects.first; node; node = node->next)
    {
        Renderer_Batch_Group_2D_Node   *group_node = node;
        Renderer_Batch_Group_2D_Params *group_params = &node->params;

        RectUniforms uniforms;
        f32 scale = equip ? equip->scale : 1.0f;
        uniforms.viewport_size_px = (Vec2_f32){{(f32)mtl_target_texture.width / scale, (f32)mtl_target_texture.height / scale}};
        uniforms.opacity = 1.0f - group_params->transparency;
        uniforms.texture_sample_channel_map = mat4x4_identity();

        if (group_params->tex.u64s[0] != 0)
        {
            u64 tex_slot = group_params->tex.u64s[0] - 1;
            if (tex_slot < r_metal_state->texture_count)
            {
                Renderer_Metal_Tex_2D *tex = &r_metal_state->textures[tex_slot];
                [encoder setFragmentTexture:metal_texture(tex->texture) atIndex:0];
                [encoder setFragmentSamplerState:metal_sampler(tex->sampler) atIndex:0];
                uniforms.texture_sample_channel_map = renderer_metal_sample_channel_map_from_tex_2d_format(tex->format);
            }
        }

        [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
        [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];

        // Set viewport and scissor rect
        MTLViewport viewport;
        viewport.originX = 0;
        viewport.originY = 0;
        viewport.width = mtl_target_texture.width;
        viewport.height = mtl_target_texture.height;
        viewport.znear = 0.0;
        viewport.zfar = 1.0;
        [encoder setViewport:viewport];

        if (group_params->clip.min.x < group_params->clip.max.x && group_params->clip.min.y < group_params->clip.max.y)
        {
            MTLScissorRect scissor;
            scissor.x = (NSUInteger)group_params->clip.min.x;
            scissor.y = (NSUInteger)group_params->clip.min.y;
            scissor.width = (NSUInteger)(group_params->clip.max.x - group_params->clip.min.x);
            scissor.height = (NSUInteger)(group_params->clip.max.y - group_params->clip.min.y);
            [encoder setScissorRect:scissor];
        }

        u64 total_instances = 0;
        u64 total_bytes_needed = 0;
        for (Renderer_Batch_Node *batch_node = group_node->batches.first; batch_node; batch_node = batch_node->next)
        {
            Renderer_Batch *batch = &batch_node->v;
            u64             instance_count = batch->byte_count / sizeof(Renderer_Rect_2D_Inst);
            total_instances += instance_count;
            total_bytes_needed += batch->byte_count;
        }

        // Get current frame's instance buffer
        Renderer_Metal_Frame_Data *frame = &r_metal_state->frames[r_metal_state->current_frame_index];

        if (total_instances > 0)
        {
            // Try to get a buffer from the pool first
            id<MTLBuffer> instance_buffer = renderer_metal_get_temp_buffer(total_bytes_needed);

            if (!instance_buffer)
            {
                // Fallback to frame's instance buffer
                if (total_bytes_needed > frame->rect_instance_buffer_size)
                {
                    metal_release(frame->rect_instance_buffer);
                    // Grow buffer exponentially to reduce future allocations
                    frame->rect_instance_buffer_size = Max(total_bytes_needed * 2, (u64)(256 * 1024));
                    frame->rect_instance_buffer = metal_retain([metal_device(r_metal_state->device)
                        newBufferWithLength:frame->rect_instance_buffer_size
                                    options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
                }
                instance_buffer = metal_buffer(frame->rect_instance_buffer);
            }

            // Note: We could use scratch arena here for temporary batch processing if needed
            // For now, we copy directly to the GPU buffer

            void *buffer_data = [instance_buffer contents];
            u64   offset = 0;
            {
                ZoneScopedN("MetalUIBatchCopy");
                for (Renderer_Batch_Node *batch_node = group_node->batches.first; batch_node; batch_node = batch_node->next)
                {
                    Renderer_Batch *batch = &batch_node->v;
                    if (batch->byte_count > 0)
                    {
                        // Could process/transform data in scratch arena before final copy
                        memcpy((u8 *)buffer_data + offset, batch->v, batch->byte_count);
                        offset += batch->byte_count;
                    }
                }
            }

            [encoder setVertexBuffer:instance_buffer offset:0 atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                        vertexStart:0
                        vertexCount:4
                      instanceCount:total_instances];
        }
    }

    [encoder endEncoding];
}

void
renderer_metal_render_pass_blur(Renderer_Pass_Params_Blur *params, void *command_buffer, void *target_texture)
{
    ZoneScopedN("MetalRenderPassBlur");
    if (!r_metal_state || !params || !command_buffer || !target_texture)
    {
        return;
    }

    id<MTLCommandBuffer> mtl_command_buffer = metal_command_buffer(command_buffer);
    id<MTLTexture>       mtl_target_texture = metal_texture(target_texture);

    Vec2_f32 current_size = (Vec2_f32){{(f32)mtl_target_texture.width, (f32)mtl_target_texture.height}};
    if (!r_metal_state->blur_temp_texture ||
        r_metal_state->blur_temp_texture_size.x != current_size.x ||
        r_metal_state->blur_temp_texture_size.y != current_size.y)
    {
        if (r_metal_state->blur_temp_texture)
        {
            metal_release(r_metal_state->blur_temp_texture);
        }

        MTLTextureDescriptor *temp_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                             width:mtl_target_texture.width
                                                                                            height:mtl_target_texture.height
                                                                                         mipmapped:NO];
        temp_desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        temp_desc.storageMode = MTLStorageModePrivate;
        r_metal_state->blur_temp_texture = metal_retain([metal_device(r_metal_state->device) newTextureWithDescriptor:temp_desc]);
        r_metal_state->blur_temp_texture_size = current_size;
    }

    id<MTLTexture> temp_texture = metal_texture(r_metal_state->blur_temp_texture);

    // Copy current target to temp texture
    id<MTLBlitCommandEncoder> blit_encoder = [mtl_command_buffer blitCommandEncoder];
    [blit_encoder copyFromTexture:mtl_target_texture
                      sourceSlice:0
                      sourceLevel:0
                     sourceOrigin:MTLOriginMake(0, 0, 0)
                       sourceSize:MTLSizeMake(mtl_target_texture.width, mtl_target_texture.height, 1)
                        toTexture:temp_texture
                 destinationSlice:0
                 destinationLevel:0
                destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit_encoder endEncoding];

    // Setup blur uniforms
    BlurUniforms uniforms;
    uniforms.viewport_size = (Vec2_f32){{(f32)mtl_target_texture.width, (f32)mtl_target_texture.height}};
    uniforms.rect_min = params->rect.min;
    uniforms.rect_max = params->rect.max;
    uniforms.clip_min = params->clip.min;
    uniforms.clip_max = params->clip.max;
    uniforms.blur_size = params->blur_size;
    memcpy(uniforms.corner_radii, params->corner_radii, sizeof(f32) * 4);

    // Use cached render pass descriptor for blur
    MTLRenderPassDescriptor *render_pass_desc = (MTLRenderPassDescriptor *)r_metal_state->blur_render_pass_desc;
    render_pass_desc.colorAttachments[0].texture = mtl_target_texture;
    render_pass_desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    render_pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLRenderCommandEncoder> encoder = [mtl_command_buffer renderCommandEncoderWithDescriptor:render_pass_desc];

    id<MTLRenderPipelineState> pipeline_state = metal_pipeline_state(r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Blur].render_pipeline_state);
    if (!pipeline_state)
    {
        assert(0 && "Blur pipeline state is null");
        [encoder endEncoding];
        [temp_texture release];
        return;
    }

    [encoder setRenderPipelineState:pipeline_state];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder setFragmentTexture:temp_texture atIndex:0];

    [encoder setFragmentSamplerState:metal_sampler(r_metal_state->blur_sampler) atIndex:0];

    [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    [encoder endEncoding];
}

void
renderer_metal_render_pass_geo_3d(Renderer_Pass_Params_Geo_3D *params, void *command_buffer, void *target_texture, void *depth_texture, Renderer_Metal_Window_Equip *equip)
{
    ZoneScopedN("MetalRenderPassGeo3D");
    if (!r_metal_state || !params || !command_buffer || !target_texture || !depth_texture)
    {
        return;
    }

    id<MTLCommandBuffer> mtl_command_buffer = metal_command_buffer(command_buffer);
    id<MTLTexture>       mtl_target_texture = metal_texture(target_texture);
    id<MTLTexture>       mtl_depth_texture = metal_texture(depth_texture);

    // Use cached render pass descriptor for geo 3D
    MTLRenderPassDescriptor *render_pass_desc = (MTLRenderPassDescriptor *)r_metal_state->geo_render_pass_desc;
    render_pass_desc.colorAttachments[0].texture = mtl_target_texture;
    render_pass_desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    render_pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    render_pass_desc.depthAttachment.texture = mtl_depth_texture;
    render_pass_desc.depthAttachment.loadAction = MTLLoadActionClear;
    render_pass_desc.depthAttachment.storeAction = MTLStoreActionDontCare;
    render_pass_desc.depthAttachment.clearDepth = 1.0;

    id<MTLRenderCommandEncoder> encoder = [mtl_command_buffer renderCommandEncoderWithDescriptor:render_pass_desc];

    id<MTLRenderPipelineState> pipeline_state = metal_pipeline_state(r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Mesh].render_pipeline_state);
    if (!pipeline_state)
    {
        assert(0 && "Mesh pipeline state is null");
        [encoder endEncoding];
        return;
    }

    [encoder setRenderPipelineState:pipeline_state];
    [encoder setDepthStencilState:metal_depth_stencil_state(r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Mesh].depth_stencil_state)];
    [encoder setCullMode:MTLCullModeBack];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];

    // Setup uniforms
    MeshUniforms uniforms;
    uniforms.view = params->view;
    uniforms.projection = params->projection;
    uniforms.viewport_size = (Vec2_f32){{params->viewport.max.x - params->viewport.min.x, params->viewport.max.y - params->viewport.min.y}};
    uniforms.clip_min = params->clip.min;
    uniforms.clip_max = params->clip.max;

    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:2];

    // Set viewport
    MTLViewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = mtl_target_texture.width;
    viewport.height = mtl_target_texture.height;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    [encoder setViewport:viewport];

    // Process mesh batches
    {
        ZoneScopedN("MetalGeo3DProcessMeshes");
        for (u64 slot_idx = 0; slot_idx < params->mesh_batches.slots_count; slot_idx++)
        {
            Renderer_Batch_Group_3D_Map_Node *node = params->mesh_batches.slots[slot_idx];
            while (node)
            {
                Renderer_Batch_Group_3D_Params *group_params = &node->params;

                // Set mesh buffers
                if (group_params->mesh_vertices.u64s[0] != 0 && group_params->mesh_indices.u64s[0] != 0)
                {
                    u64 vertex_slot = group_params->mesh_vertices.u64s[0] - 1;
                    u64 index_slot = group_params->mesh_indices.u64s[0] - 1;

                    if (vertex_slot < r_metal_state->buffer_count && index_slot < r_metal_state->buffer_count)
                    {
                        Renderer_Metal_Buffer *vertex_buffer = &r_metal_state->buffers[vertex_slot];
                        Renderer_Metal_Buffer *index_buffer = &r_metal_state->buffers[index_slot];

                        [encoder setVertexBuffer:metal_buffer(vertex_buffer->buffer) offset:0 atIndex:0];

                        // Set texture
                        if (group_params->albedo_tex.u64s[0] != 0)
                        {
                            u64 tex_slot = group_params->albedo_tex.u64s[0] - 1;
                            if (tex_slot < r_metal_state->texture_count)
                            {
                                Renderer_Metal_Tex_2D *tex = &r_metal_state->textures[tex_slot];
                                [encoder setFragmentTexture:metal_texture(tex->texture) atIndex:0];
                                [encoder setFragmentSamplerState:metal_sampler(tex->sampler) atIndex:0];
                            }
                        }

                        // Draw instances from batches
                        {
                            ZoneScopedN("MetalGeo3DDrawBatches");
                            for (Renderer_Batch_Node *batch_node = node->batches.first; batch_node; batch_node = batch_node->next)
                            {
                                Renderer_Batch *batch = &batch_node->v;
                                u64             instance_count = batch->byte_count / sizeof(Renderer_Mesh_3D_Inst);

                                if (instance_count > 0)
                                {
                                    // Use buffer pool for instance data instead of setVertexBytes
                                    id<MTLBuffer> instance_buffer = NULL;

                                    if (batch->byte_count <= 4096) // Metal's setVertexBytes limit
                                    {
                                        // For small batches, setVertexBytes is fine
                                        [encoder setVertexBytes:batch->v length:batch->byte_count atIndex:1];
                                    }
                                    else
                                    {
                                        // For larger batches, use buffer pool
                                        instance_buffer = renderer_metal_get_temp_buffer(batch->byte_count);
                                        if (instance_buffer)
                                        {
                                            memcpy([instance_buffer contents], batch -> v, batch -> byte_count);
                                            [encoder setVertexBuffer:instance_buffer offset:0 atIndex:1];
                                        }
                                        else
                                        {
                                            // Fallback: use frame's mesh uniform buffer if available
                                            Renderer_Metal_Frame_Data *frame = &r_metal_state->frames[r_metal_state->current_frame_index];
                                            id<MTLBuffer>              fallback_buffer = metal_buffer(frame->mesh_uniform_buffer);
                                            memcpy([fallback_buffer contents], batch -> v, batch -> byte_count);
                                            [encoder setVertexBuffer:fallback_buffer offset:0 atIndex:1];
                                        }
                                    }

                                    // Determine index count based on buffer size and index type
                                    u64 index_count = index_buffer->size / sizeof(u32);

                                    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                                        indexCount:index_count
                                                         indexType:MTLIndexTypeUInt32
                                                       indexBuffer:metal_buffer(index_buffer->buffer)
                                                 indexBufferOffset:0
                                                     instanceCount:instance_count];
                                }
                            }
                        }
                    }
                }

                node = node->next;
            }
        }
    }

    [encoder endEncoding];
}

// Helper function to render UI content
static void
renderer_metal_render_ui_content(Renderer_Pass_Params_UI *params, id<MTLRenderCommandEncoder> encoder, id<MTLTexture> target_texture, Renderer_Metal_Window_Equip *equip)
{
    if (!params || !params->rects.first)
        return;

    [encoder setRenderPipelineState:metal_pipeline_state(r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Rect].render_pipeline_state)];

    // Process each batch group
    for (Renderer_Batch_Group_2D_Node *node = params->rects.first; node; node = node->next)
    {
        Renderer_Batch_Group_2D_Node   *group_node = node;
        Renderer_Batch_Group_2D_Params *group_params = &node->params;

        RectUniforms uniforms;
        f32 scale = equip ? equip->scale : 1.0f;
        uniforms.viewport_size_px = (Vec2_f32){{(f32)target_texture.width / scale, (f32)target_texture.height / scale}};
        uniforms.opacity = 1.0f - group_params->transparency;
        uniforms.texture_sample_channel_map = mat4x4_identity();

        if (group_params->tex.u64s[0] != 0)
        {
            u64 tex_slot = group_params->tex.u64s[0] - 1;
            if (tex_slot < r_metal_state->texture_count)
            {
                Renderer_Metal_Tex_2D *tex = &r_metal_state->textures[tex_slot];
                [encoder setFragmentTexture:metal_texture(tex->texture) atIndex:0];
                [encoder setFragmentSamplerState:metal_sampler(tex->sampler) atIndex:0];
                uniforms.texture_sample_channel_map = renderer_metal_sample_channel_map_from_tex_2d_format(tex->format);
            }
        }

        [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
        [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];

        // Set viewport and scissor rect
        MTLViewport viewport;
        viewport.originX = 0;
        viewport.originY = 0;
        viewport.width = target_texture.width;
        viewport.height = target_texture.height;
        viewport.znear = 0.0;
        viewport.zfar = 1.0;
        [encoder setViewport:viewport];

        if (group_params->clip.min.x < group_params->clip.max.x && group_params->clip.min.y < group_params->clip.max.y)
        {
            MTLScissorRect scissor;
            scissor.x = (NSUInteger)group_params->clip.min.x;
            scissor.y = (NSUInteger)group_params->clip.min.y;
            scissor.width = (NSUInteger)(group_params->clip.max.x - group_params->clip.min.x);
            scissor.height = (NSUInteger)(group_params->clip.max.y - group_params->clip.min.y);
            [encoder setScissorRect:scissor];
        }

        // Count total instances
        u64 total_instances = 0;
        u64 total_bytes_needed = 0;
        for (Renderer_Batch_Node *batch_node = group_node->batches.first; batch_node; batch_node = batch_node->next)
        {
            Renderer_Batch *batch = &batch_node->v;
            u64             instance_count = batch->byte_count / sizeof(Renderer_Rect_2D_Inst);
            total_instances += instance_count;
            total_bytes_needed += batch->byte_count;
        }

        if (total_instances > 0)
        {
            // Get buffer for instances
            Renderer_Metal_Frame_Data *frame = &r_metal_state->frames[r_metal_state->current_frame_index];
            id<MTLBuffer>              instance_buffer = renderer_metal_get_temp_buffer(total_bytes_needed);

            if (!instance_buffer)
            {
                if (total_bytes_needed > frame->rect_instance_buffer_size)
                {
                    metal_release(frame->rect_instance_buffer);
                    frame->rect_instance_buffer_size = Max(total_bytes_needed * 2, (u64)(256 * 1024));
                    frame->rect_instance_buffer = metal_retain([metal_device(r_metal_state->device)
                        newBufferWithLength:frame->rect_instance_buffer_size
                                    options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
                }
                instance_buffer = metal_buffer(frame->rect_instance_buffer);
            }

            void *buffer_data = [instance_buffer contents];
            u64   offset = 0;
            for (Renderer_Batch_Node *batch_node = group_node->batches.first; batch_node; batch_node = batch_node->next)
            {
                Renderer_Batch *batch = &batch_node->v;
                if (batch->byte_count > 0)
                {
                    memcpy((u8 *)buffer_data + offset, batch->v, batch->byte_count);
                    offset += batch->byte_count;
                }
            }

            [encoder setVertexBuffer:instance_buffer offset:0 atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                        vertexStart:0
                        vertexCount:4
                      instanceCount:total_instances];
        }
    }
}

// Helper function to render 3D content
static void
renderer_metal_render_3d_content(Renderer_Pass_Params_Geo_3D *params, id<MTLRenderCommandEncoder> encoder, id<MTLTexture> target_texture)
{
    if (!params || params->mesh_batches.slots_count == 0)
        return;

    [encoder setRenderPipelineState:metal_pipeline_state(r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Mesh].render_pipeline_state)];
    [encoder setDepthStencilState:metal_depth_stencil_state(r_metal_state->pipelines[Renderer_Metal_Shader_Kind_Mesh].depth_stencil_state)];
    [encoder setCullMode:MTLCullModeBack];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];

    // Setup uniforms
    MeshUniforms uniforms;
    uniforms.view = params->view;
    uniforms.projection = params->projection;
    uniforms.viewport_size = (Vec2_f32){{params->viewport.max.x - params->viewport.min.x, params->viewport.max.y - params->viewport.min.y}};
    uniforms.clip_min = params->clip.min;
    uniforms.clip_max = params->clip.max;

    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:2];

    // Set viewport
    MTLViewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = target_texture.width;
    viewport.height = target_texture.height;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    [encoder setViewport:viewport];

    // Process mesh batches
    for (u64 slot_idx = 0; slot_idx < params->mesh_batches.slots_count; slot_idx++)
    {
        Renderer_Batch_Group_3D_Map_Node *node = params->mesh_batches.slots[slot_idx];
        while (node)
        {
            Renderer_Batch_Group_3D_Params *group_params = &node->params;

            // Set mesh buffers
            if (group_params->mesh_vertices.u64s[0] != 0 && group_params->mesh_indices.u64s[0] != 0)
            {
                u64 vertex_slot = group_params->mesh_vertices.u64s[0] - 1;
                u64 index_slot = group_params->mesh_indices.u64s[0] - 1;

                if (vertex_slot < r_metal_state->buffer_count && index_slot < r_metal_state->buffer_count)
                {
                    Renderer_Metal_Buffer *vertex_buffer = &r_metal_state->buffers[vertex_slot];
                    Renderer_Metal_Buffer *index_buffer = &r_metal_state->buffers[index_slot];

                    [encoder setVertexBuffer:metal_buffer(vertex_buffer->buffer) offset:0 atIndex:0];

                    // Set texture
                    if (group_params->albedo_tex.u64s[0] != 0)
                    {
                        u64 tex_slot = group_params->albedo_tex.u64s[0] - 1;
                        if (tex_slot < r_metal_state->texture_count)
                        {
                            Renderer_Metal_Tex_2D *tex = &r_metal_state->textures[tex_slot];
                            [encoder setFragmentTexture:metal_texture(tex->texture) atIndex:0];
                            [encoder setFragmentSamplerState:metal_sampler(tex->sampler) atIndex:0];
                        }
                    }

                    // Draw instances from batches
                    for (Renderer_Batch_Node *batch_node = node->batches.first; batch_node; batch_node = batch_node->next)
                    {
                        Renderer_Batch *batch = &batch_node->v;
                        u64             instance_count = batch->byte_count / sizeof(Renderer_Mesh_3D_Inst);

                        if (instance_count > 0)
                        {
                            // Use buffer pool for instance data
                            id<MTLBuffer> instance_buffer = renderer_metal_get_temp_buffer(batch->byte_count);
                            if (instance_buffer)
                            {
                                memcpy([instance_buffer contents], batch -> v, batch -> byte_count);
                                [encoder setVertexBuffer:instance_buffer offset:0 atIndex:1];
                            }
                            else
                            {
                                // Fallback to setVertexBytes for small batches
                                [encoder setVertexBytes:batch->v length:batch->byte_count atIndex:1];
                            }

                            u64 index_count = index_buffer->size / sizeof(u32);
                            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                                indexCount:index_count
                                                 indexType:MTLIndexTypeUInt32
                                               indexBuffer:metal_buffer(index_buffer->buffer)
                                         indexBufferOffset:0
                                             instanceCount:instance_count];
                        }
                    }
                }
            }

            node = node->next;
        }
    }
}

void
renderer_metal_render_pass_combined(Renderer_Pass_Params_UI     *ui_params,
                                    Renderer_Pass_Params_Geo_3D *geo_params,
                                    void                        *command_buffer,
                                    void                        *target_texture,
                                    void                        *depth_texture,
                                    Renderer_Metal_Window_Equip *equip)
{
    ZoneScopedN("MetalRenderPassCombined");
    if (!r_metal_state || !command_buffer || !target_texture)
    {
        return;
    }

    id<MTLCommandBuffer> mtl_command_buffer = metal_command_buffer(command_buffer);
    id<MTLTexture>       mtl_target_texture = metal_texture(target_texture);
    id<MTLTexture>       mtl_depth_texture = metal_texture(depth_texture);

    // Use cached render pass descriptor for combined rendering
    MTLRenderPassDescriptor *render_pass_desc = (MTLRenderPassDescriptor *)r_metal_state->geo_render_pass_desc;
    render_pass_desc.colorAttachments[0].texture = mtl_target_texture;
    render_pass_desc.colorAttachments[0].loadAction = MTLLoadActionClear;
    render_pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    render_pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.1, 1.0);

    if (geo_params && mtl_depth_texture)
    {
        render_pass_desc.depthAttachment.texture = mtl_depth_texture;
        render_pass_desc.depthAttachment.loadAction = MTLLoadActionClear;
        render_pass_desc.depthAttachment.storeAction = MTLStoreActionDontCare;
        render_pass_desc.depthAttachment.clearDepth = 1.0;
    }

    id<MTLRenderCommandEncoder> encoder = [mtl_command_buffer renderCommandEncoderWithDescriptor:render_pass_desc];

    // First render UI (if provided)
    if (ui_params && ui_params->rects.first)
    {
        ZoneScopedN("CombinedUIRendering");
        renderer_metal_render_ui_content(ui_params, encoder, mtl_target_texture, equip);
    }

    // Then render 3D geometry (if provided)
    if (geo_params && geo_params->mesh_batches.slots_count > 0)
    {
        ZoneScopedN("Combined3DRendering");
        renderer_metal_render_3d_content(geo_params, encoder, mtl_target_texture);
    }

    [encoder endEncoding];
}