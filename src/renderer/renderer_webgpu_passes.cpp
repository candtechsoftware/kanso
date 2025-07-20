#include "../base/logger.h"
#include "base/arena.h"
#include "renderer_webgpu.h"
#include <cstring>

void
renderer_webgpu_render_pass_ui(Renderer_Pass_Params_UI* params, WGPUCommandEncoder encoder, WGPUTextureView target_view)
{
    if (!params || !params->rects.first)
    {
        return;
    }

    u64 total_instance_count = 0;
    for (List_Node<Renderer_Batch_Group_2D_Node>* group_node = params->rects.first; group_node != nullptr; group_node = group_node->next)
    {
        Renderer_Batch_Group_2D_Node* group = &group_node->v;
        for (List_Node<Renderer_Batch>* batch_node = group->batches.first; batch_node != nullptr; batch_node = batch_node->next)
        {
            Renderer_Batch* batch = &batch_node->v;
            total_instance_count += batch->byte_count / sizeof(Renderer_Rect_2D_Inst);
        }
    }

    if (total_instance_count == 0)
    {
        return;
    }

    // Get window equip for viewport size
    Renderer_WebGPU_Window_Equip* equip = nullptr;
    for (u64 i = 0; i < r_wgpu_state->window_equip_count; i++)
    {
        if (r_wgpu_state->window_equips[i].surface)
        {
            equip = &r_wgpu_state->window_equips[i];
            break;
        }
    }

    if (!equip)
    {
        log_error("No window equip found for UI rendering");
        return;
    }

    u64 required_size = total_instance_count * sizeof(Renderer_Rect_2D_Inst);
    if (required_size > r_wgpu_state->rect_instance_buffer_size)
    {
        // Recreate buffer with larger size
        wgpuBufferRelease(r_wgpu_state->rect_instance_buffer);

        WGPUBufferDescriptor bufDesc = {};
        bufDesc.nextInChain = nullptr;
        bufDesc.label = {"Rect Instance Buffer", WGPU_STRLEN};
        bufDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        bufDesc.size = required_size * 2; // Double the size
        bufDesc.mappedAtCreation = false;

        r_wgpu_state->rect_instance_buffer = wgpuDeviceCreateBuffer(r_wgpu_state->device, &bufDesc);
        r_wgpu_state->rect_instance_buffer_size = bufDesc.size;
    }

    {
        Scratch scratch_arena = scratch_begin(r_wgpu_state->arena);
        Renderer_Rect_2D_Inst* all_instances = push_array(scratch_arena.arena, Renderer_Rect_2D_Inst, total_instance_count);
        u64 instance_idx = 0;

        for (List_Node<Renderer_Batch_Group_2D_Node>* group_node = params->rects.first; group_node != nullptr; group_node = group_node->next)
        {
            Renderer_Batch_Group_2D_Node* group = &group_node->v;
            for (List_Node<Renderer_Batch>* batch_node = group->batches.first; batch_node != nullptr; batch_node = batch_node->next)
            {
                Renderer_Batch* batch = &batch_node->v;
                Renderer_Rect_2D_Inst* batch_instances = (Renderer_Rect_2D_Inst*)batch->v;
                u64 instance_count = batch->byte_count / sizeof(Renderer_Rect_2D_Inst);

                memcpy(&all_instances[instance_idx], batch_instances, instance_count * sizeof(Renderer_Rect_2D_Inst));
                instance_idx += instance_count;
            }
        }

        wgpuQueueWriteBuffer(r_wgpu_state->queue, r_wgpu_state->rect_instance_buffer, 0, all_instances, required_size);
        scratch_end(&scratch_arena);
    }

    // Create render pass
    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = target_view;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Load;
    colorAttachment.storeOp = WGPUStoreOp_Store;

    WGPURenderPassDescriptor passDesc = {};
    passDesc.nextInChain = nullptr;
    passDesc.label = {"UI Render Pass", WGPU_STRLEN};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = nullptr;
    passDesc.occlusionQuerySet = nullptr;
    passDesc.timestampWrites = nullptr;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);

    // Set pipeline
    Renderer_WebGPU_Pipeline* pipeline = &r_wgpu_state->pipelines[Renderer_WebGPU_Shader_Kind_Rect];
    wgpuRenderPassEncoderSetPipeline(pass, pipeline->render_pipeline);

    // Process each batch group
    u64 instance_offset = 0;
    for (List_Node<Renderer_Batch_Group_2D_Node>* group_node = params->rects.first; group_node != nullptr; group_node = group_node->next)
    {
        Renderer_Batch_Group_2D_Node* group = &group_node->v;
        // Count instances in this group
        u64 group_instance_count = 0;
        for (List_Node<Renderer_Batch>* batch_node = group->batches.first; batch_node != nullptr; batch_node = batch_node->next)
        {
            Renderer_Batch* batch = &batch_node->v;
            group_instance_count += batch->byte_count / sizeof(Renderer_Rect_2D_Inst);
        }

        if (group_instance_count == 0)
        {
            continue;
        }

        // Create uniform buffer for this group
        struct RectUniforms
        {
            Vec2<f32> viewport_size_px;
            f32 opacity;
            f32 _padding;
            Mat4x4<f32> texture_sample_channel_map;
        } uniforms;

        // Get window size from equip
        uniforms.viewport_size_px = equip->size;
        uniforms.opacity = 1.0f - group->params.transparency;
        uniforms.texture_sample_channel_map = renderer_webgpu_sample_channel_map_from_tex_2d_format(Renderer_Tex_2D_Format_RGBA8);

        WGPUBuffer uniformBuffer = nullptr;
        {
            WGPUBufferDescriptor bufDesc = {};
            bufDesc.nextInChain = nullptr;
            bufDesc.label = {"Rect Uniforms", WGPU_STRLEN};
            bufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
            bufDesc.size = sizeof(uniforms);
            bufDesc.mappedAtCreation = false;

            uniformBuffer = wgpuDeviceCreateBuffer(r_wgpu_state->device, &bufDesc);
            wgpuQueueWriteBuffer(r_wgpu_state->queue, uniformBuffer, 0, &uniforms, sizeof(uniforms));
        }

        // Get texture
        WGPUTextureView textureView = nullptr;
        WGPUSampler sampler = nullptr;

        if (group->params.tex.u64s[0] != 0 && group->params.tex.u64s[0] <= r_wgpu_state->texture_count)
        {
            u64 tex_idx = group->params.tex.u64s[0] - 1;
            Renderer_WebGPU_Tex_2D* tex = &r_wgpu_state->textures[tex_idx];
            textureView = tex->view;
            sampler = tex->sampler;
        }
        else
        {
            // Create default white texture if needed
            static WGPUTexture defaultWhiteTexture = nullptr;
            static WGPUTextureView defaultWhiteTextureView = nullptr;
            static WGPUSampler defaultSampler = nullptr;

            if (!defaultWhiteTexture)
            {
                WGPUTextureDescriptor texDesc = {};
                texDesc.nextInChain = nullptr;
                texDesc.label = {"Default White Texture", WGPU_STRLEN};
                texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
                texDesc.dimension = WGPUTextureDimension_2D;
                texDesc.size = {1, 1, 1};
                texDesc.format = WGPUTextureFormat_RGBA8Unorm;
                texDesc.mipLevelCount = 1;
                texDesc.sampleCount = 1;
                texDesc.viewFormatCount = 0;
                texDesc.viewFormats = nullptr;

                defaultWhiteTexture = wgpuDeviceCreateTexture(r_wgpu_state->device, &texDesc);

                u32 white = 0xFFFFFFFF;
                WGPUTexelCopyTextureInfo textureCopyInfo = {};
                textureCopyInfo.texture = defaultWhiteTexture;
                textureCopyInfo.mipLevel = 0;
                textureCopyInfo.origin = {0, 0, 0};
                textureCopyInfo.aspect = WGPUTextureAspect_All;

                WGPUTexelCopyBufferLayout dataLayout = {};
                dataLayout.offset = 0;
                dataLayout.bytesPerRow = 4;
                dataLayout.rowsPerImage = 1;

                wgpuQueueWriteTexture(r_wgpu_state->queue, &textureCopyInfo, &white, 4, &dataLayout, &texDesc.size);

                WGPUTextureViewDescriptor viewDesc = {};
                viewDesc.nextInChain = nullptr;
                viewDesc.label = {"Default White Texture View", WGPU_STRLEN};
                viewDesc.format = WGPUTextureFormat_RGBA8Unorm;
                viewDesc.dimension = WGPUTextureViewDimension_2D;
                viewDesc.baseMipLevel = 0;
                viewDesc.mipLevelCount = 1;
                viewDesc.baseArrayLayer = 0;
                viewDesc.arrayLayerCount = 1;
                viewDesc.aspect = WGPUTextureAspect_All;

                defaultWhiteTextureView = wgpuTextureCreateView(defaultWhiteTexture, &viewDesc);

                WGPUSamplerDescriptor samplerDesc = {};
                samplerDesc.nextInChain = nullptr;
                samplerDesc.label = {"Default Sampler", WGPU_STRLEN};
                samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
                samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
                samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
                samplerDesc.magFilter = WGPUFilterMode_Linear;
                samplerDesc.minFilter = WGPUFilterMode_Linear;
                samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
                samplerDesc.lodMinClamp = 0.0f;
                samplerDesc.lodMaxClamp = 32.0f;
                samplerDesc.compare = WGPUCompareFunction_Undefined;
                samplerDesc.maxAnisotropy = 1;

                defaultSampler = wgpuDeviceCreateSampler(r_wgpu_state->device, &samplerDesc);
            }

            textureView = defaultWhiteTextureView;
            sampler = defaultSampler;
        }

        // Create bind group
        WGPUBindGroupEntry bindGroupEntries[3] = {};

        bindGroupEntries[0].binding = 0;
        bindGroupEntries[0].buffer = uniformBuffer;
        bindGroupEntries[0].offset = 0;
        bindGroupEntries[0].size = sizeof(uniforms);

        bindGroupEntries[1].binding = 1;
        bindGroupEntries[1].textureView = textureView;

        bindGroupEntries[2].binding = 2;
        bindGroupEntries[2].sampler = sampler;

        WGPUBindGroupDescriptor bindGroupDesc = {};
        bindGroupDesc.nextInChain = nullptr;
        bindGroupDesc.label = {"Rect Bind Group", WGPU_STRLEN};
        bindGroupDesc.layout = pipeline->bind_group_layout;
        bindGroupDesc.entryCount = 3;
        bindGroupDesc.entries = bindGroupEntries;

        WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(r_wgpu_state->device, &bindGroupDesc);

        // Set bind group
        wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);

        // Set vertex buffer
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, r_wgpu_state->rect_instance_buffer, instance_offset * sizeof(Renderer_Rect_2D_Inst), group_instance_count * sizeof(Renderer_Rect_2D_Inst));

        // Set scissor rect for clipping
        if (group->params.clip.max.x != group->params.clip.min.x || group->params.clip.max.y != group->params.clip.min.y)
        {
            wgpuRenderPassEncoderSetScissorRect(pass,
                                                (u32)group->params.clip.min.x,
                                                (u32)group->params.clip.min.y,
                                                (u32)(group->params.clip.max.x - group->params.clip.min.x),
                                                (u32)(group->params.clip.max.y - group->params.clip.min.y));
        }

        // Draw
        wgpuRenderPassEncoderDraw(pass, 4, group_instance_count, 0, instance_offset);

        // Cleanup
        wgpuBindGroupRelease(bindGroup);
        wgpuBufferRelease(uniformBuffer);

        instance_offset += group_instance_count;
    }

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
}

void
renderer_webgpu_render_pass_geo_3d(Renderer_Pass_Params_Geo_3D* params, WGPUCommandEncoder encoder, WGPUTextureView target_view)
{
    if (!params || params->mesh_batches.slots_count == 0)
    {
        return;
    }

    // Get window equip for depth buffer
    Renderer_WebGPU_Window_Equip* equip = nullptr;
    for (u64 i = 0; i < r_wgpu_state->window_equip_count; i++)
    {
        if (r_wgpu_state->window_equips[i].surface)
        {
            equip = &r_wgpu_state->window_equips[i];
            break;
        }
    }

    if (!equip)
    {
        log_error("No window equip found for 3D rendering");
        return;
    }

    // Create render pass
    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = target_view;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Load;
    colorAttachment.storeOp = WGPUStoreOp_Store;

    WGPURenderPassDepthStencilAttachment depthAttachment = {};
    depthAttachment.view = equip->depth_texture_view;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.depthReadOnly = false;
    depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
    depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
    depthAttachment.stencilReadOnly = true;

    WGPURenderPassDescriptor passDesc = {};
    passDesc.nextInChain = nullptr;
    passDesc.label = {"3D Render Pass", WGPU_STRLEN};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = &depthAttachment;
    passDesc.occlusionQuerySet = nullptr;
    passDesc.timestampWrites = nullptr;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);

    // Set pipeline
    Renderer_WebGPU_Pipeline* pipeline = &r_wgpu_state->pipelines[Renderer_WebGPU_Shader_Kind_Mesh];
    wgpuRenderPassEncoderSetPipeline(pass, pipeline->render_pipeline);

    // Set viewport
    if (params->viewport.max.x != params->viewport.min.x || params->viewport.max.y != params->viewport.min.y)
    {
        wgpuRenderPassEncoderSetViewport(pass,
                                         params->viewport.min.x,
                                         params->viewport.min.y,
                                         params->viewport.max.x - params->viewport.min.x,
                                         params->viewport.max.y - params->viewport.min.y,
                                         0.0f, 1.0f);
    }

    // Process mesh batches
    for (u32 i = 0; i < params->mesh_batches.slots_count; i++)
    {
        Renderer_Batch_Group_3D_Map_Node* node = params->mesh_batches.slots[i];
        if (!node)
            continue;

        // Get vertex and index buffers
        if (node->params.mesh_vertices.u64s[0] == 0 || node->params.mesh_vertices.u64s[0] > r_wgpu_state->buffer_count)
        {
            continue;
        }

        u64 vertex_buf_idx = node->params.mesh_vertices.u64s[0] - 1;
        Renderer_WebGPU_Buffer* vertex_buf = &r_wgpu_state->buffers[vertex_buf_idx];

        WGPUBuffer index_buffer = nullptr;
        u64 index_count = 0;
        if (node->params.mesh_indices.u64s[0] != 0 && node->params.mesh_indices.u64s[0] <= r_wgpu_state->buffer_count)
        {
            u64 index_buf_idx = node->params.mesh_indices.u64s[0] - 1;
            Renderer_WebGPU_Buffer* index_buf = &r_wgpu_state->buffers[index_buf_idx];
            index_buffer = index_buf->buffer;
            index_count = index_buf->size / sizeof(u32);
        }

        // Process instances
        for (Renderer_Batch_Node* batch = node->batches.first; batch != nullptr; batch = batch->next)
        {
            Renderer_Mesh_3D_Inst* instances = (Renderer_Mesh_3D_Inst*)batch->v.v;
            u64 instance_count = batch->v.byte_count / sizeof(Renderer_Mesh_3D_Inst);

            for (u64 inst_idx = 0; inst_idx < instance_count; inst_idx++)
            {
                // Update uniforms
                struct MeshUniforms
                {
                    Mat4x4<f32> projection;
                    Mat4x4<f32> view;
                    Mat4x4<f32> model;
                } uniforms;

                uniforms.projection = params->projection;
                uniforms.view = params->view;
                uniforms.model = instances[inst_idx].xform * node->params.xform;

                wgpuQueueWriteBuffer(r_wgpu_state->queue, r_wgpu_state->mesh_uniform_buffer, 0, &uniforms, sizeof(uniforms));

                // Get texture
                WGPUTextureView textureView = nullptr;
                WGPUSampler sampler = nullptr;

                if (node->params.albedo_tex.u64s[0] != 0 && node->params.albedo_tex.u64s[0] <= r_wgpu_state->texture_count)
                {
                    u64 tex_idx = node->params.albedo_tex.u64s[0] - 1;
                    Renderer_WebGPU_Tex_2D* tex = &r_wgpu_state->textures[tex_idx];
                    textureView = tex->view;
                    sampler = tex->sampler;
                }
                else
                {
                    // Use default white texture (created in UI pass if needed)
                    continue; // Skip for now
                }

                // Create bind group
                WGPUBindGroupEntry bindGroupEntries[3] = {};

                bindGroupEntries[0].binding = 0;
                bindGroupEntries[0].buffer = r_wgpu_state->mesh_uniform_buffer;
                bindGroupEntries[0].offset = 0;
                bindGroupEntries[0].size = sizeof(uniforms);

                bindGroupEntries[1].binding = 1;
                bindGroupEntries[1].textureView = textureView;

                bindGroupEntries[2].binding = 2;
                bindGroupEntries[2].sampler = sampler;

                WGPUBindGroupDescriptor bindGroupDesc = {};
                bindGroupDesc.nextInChain = nullptr;
                bindGroupDesc.label = {"Mesh Bind Group", WGPU_STRLEN};
                bindGroupDesc.layout = pipeline->bind_group_layout;
                bindGroupDesc.entryCount = 3;
                bindGroupDesc.entries = bindGroupEntries;

                WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(r_wgpu_state->device, &bindGroupDesc);

                // Set bind group
                wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);

                // Set vertex buffer
                wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertex_buf->buffer, 0, vertex_buf->size);

                // Draw
                if (index_buffer)
                {
                    wgpuRenderPassEncoderSetIndexBuffer(pass, index_buffer, WGPUIndexFormat_Uint32, 0, index_count * sizeof(u32));
                    wgpuRenderPassEncoderDrawIndexed(pass, index_count, 1, 0, 0, 0);
                }
                else
                {
                    u64 vertex_count = vertex_buf->size / (sizeof(Vec3<f32>) + sizeof(Vec2<f32>) + sizeof(Vec3<f32>) + sizeof(Vec4<f32>));
                    wgpuRenderPassEncoderDraw(pass, vertex_count, 1, 0, 0);
                }

                // Cleanup
                wgpuBindGroupRelease(bindGroup);
            }
        }
    }

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
}

void
renderer_webgpu_render_pass_blur(Renderer_Pass_Params_Blur* params, WGPUCommandEncoder encoder, WGPUTextureView target_view)
{
    Scratch scratch_arena = scratch_begin(r_wgpu_state->arena);
    {
    }

    scratch_end(&scratch_arena);

    // TODO: Implement blur pass
    // This requires creating temporary render targets and multi-pass rendering
    log_warn("Blur pass not yet implemented for WebGPU");
}
