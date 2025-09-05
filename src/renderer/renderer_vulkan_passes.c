#include "renderer_vulkan.h"
#include "../base/base_inc.h"

// Helper to allocate from a dynamic buffer
typedef struct Dynamic_Buffer Dynamic_Buffer;
struct Dynamic_Buffer
{
    VkBuffer       buffer;
    VkDeviceMemory memory;
    void          *mapped;
    u64            size;
    u64            offset;
};

static Dynamic_Buffer g_instance_buffer = {0};

// Uniform buffer structures
typedef struct UI_Uniforms UI_Uniforms;
struct UI_Uniforms
{
    Vec2_f32   viewport_size_px;
    f32        opacity;
    f32        _pad;
    Mat4x4_f32 texture_sample_channel_map;
};

typedef struct Geo_3D_Uniforms Geo_3D_Uniforms;
struct Geo_3D_Uniforms
{
    Mat4x4_f32 view;
    Mat4x4_f32 projection;
};

// Forward declarations
void
renderer_vulkan_submit_ui_pass(VkCommandBuffer cmd, Renderer_Pass_Params_UI *params,
                               Renderer_Vulkan_Window_Equipment *equip);
void
renderer_vulkan_submit_blur_pass(VkCommandBuffer cmd, Renderer_Pass_Params_Blur *params,
                                 Renderer_Vulkan_Window_Equipment *equip);
void
renderer_vulkan_submit_geo_3d_pass(VkCommandBuffer cmd, Renderer_Pass_Params_Geo_3D *params,
                                   Renderer_Vulkan_Window_Equipment *equip);

void
renderer_window_submit(void *window, Renderer_Handle window_equip, Renderer_Pass_List *passes)
{
    ZoneScoped;
    Renderer_Vulkan_Window_Equipment *equip = (Renderer_Vulkan_Window_Equipment *)window_equip.u64s[0];
    if (!equip || !passes || !equip->frame_begun)
        return;

    VkCommandBuffer cmd = equip->command_buffers[equip->current_frame];

    // Reset instance buffer offset for new frame
    g_instance_buffer.offset = 0;

    // Begin render pass
    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = equip->render_pass;
    // Get the nth framebuffer from the array with bounds checking
    if (equip->current_image_index >= equip->swapchain_image_count)
    {
        printf("ERROR: current_image_index %d >= swapchain_image_count %d\n",
               equip->current_image_index, equip->swapchain_image_count);
        return;
    }
    if (!equip->framebuffers)
    {
        printf("ERROR: framebuffers array is NULL!\n");
        return;
    }

    // Validate framebuffer before use
    if (!equip->framebuffers || equip->framebuffers[equip->current_image_index] == VK_NULL_HANDLE)
    {
        log_error("Invalid framebuffer at index %u!", equip->current_image_index);
        return;
    }

    render_pass_info.framebuffer = equip->framebuffers[equip->current_image_index];
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    render_pass_info.renderArea.extent = equip->swapchain_extent;
    printf("Render area: %dx%d, framebuffer[%d]: %p (count: %d)\n",
           equip->swapchain_extent.width, equip->swapchain_extent.height,
           equip->current_image_index,
           (void *)(uintptr_t)equip->framebuffers[equip->current_image_index],
           equip->swapchain_image_count);

    VkClearValue clear_values[2];
    clear_values[0].color.float32[0] = 0.3f; // Match C++ version's gray
    clear_values[0].color.float32[1] = 0.3f;
    clear_values[0].color.float32[2] = 0.3f;
    clear_values[0].color.float32[3] = 1.0f;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    printf("Started render pass with gray clear color\n");

    // Submit each pass
    int pass_count = 0;
    for (Renderer_Pass_Node *node = passes->first; node; node = node->next)
    {
        Renderer_Pass *pass = &node->v;

        pass_count++;
        printf("Processing pass %d of kind %d\n", pass_count, pass->kind);

        switch (pass->kind)
        {
        case Renderer_Pass_Kind_UI:
            printf("Submitting UI pass\n");
            renderer_vulkan_submit_ui_pass(cmd, pass->params_ui, equip);
            break;

        case Renderer_Pass_Kind_Blur:
            printf("Submitting blur pass\n");
            renderer_vulkan_submit_blur_pass(cmd, pass->params_blur, equip);
            break;

        case Renderer_Pass_Kind_Geo_3D:
            renderer_vulkan_submit_geo_3d_pass(cmd, pass->params_geo_3d, equip);
            break;
        }
    }

    printf("Processed %d passes total. Ending render pass.\n", pass_count);
    vkCmdEndRenderPass(cmd);
}

static void
ensure_dynamic_buffer(Dynamic_Buffer *buf, u64 required_size)
{
    if (buf->size < buf->offset + required_size)
    {
        // Need to grow buffer
        u64 new_size = Max((buf->offset + required_size) * 2, MB(16));

        // Destroy old buffer
        if (buf->buffer)
        {
            vkDeviceWaitIdle(g_vulkan->device);
            vkUnmapMemory(g_vulkan->device, buf->memory);
            vkDestroyBuffer(g_vulkan->device, buf->buffer, NULL);
            vkFreeMemory(g_vulkan->device, buf->memory, NULL);
        }

        // Create new buffer with larger size
        buf->size = new_size;

        VkBufferCreateInfo buffer_info = {0};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = buf->size;
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(g_vulkan->device, &buffer_info, NULL, &buf->buffer);

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(g_vulkan->device, buf->buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info = {0};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = renderer_vulkan_find_memory_type(
            mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(g_vulkan->device, &alloc_info, NULL, &buf->memory);
        vkBindBufferMemory(g_vulkan->device, buf->buffer, buf->memory, 0);
        vkMapMemory(g_vulkan->device, buf->memory, 0, buf->size, 0, &buf->mapped);

        // Reset offset when we recreate the buffer
        buf->offset = 0;
    }
    // Don't reset offset if buffer is large enough
}

void
renderer_vulkan_submit_ui_pass(VkCommandBuffer cmd, Renderer_Pass_Params_UI *params,
                               Renderer_Vulkan_Window_Equipment *equip)
{
    ZoneScopedN("VulkanSubmitUIPass");
    // Bind UI pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vulkan->pipelines.ui);

    // Set viewport and scissor (using physical pixels for viewport)
    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)equip->swapchain_extent.width;
    viewport.height = (f32)equip->swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Set initial scissor to full viewport
    VkRect2D initial_scissor = {0};
    initial_scissor.offset.x = 0;
    initial_scissor.offset.y = 0;
    initial_scissor.extent.width = equip->swapchain_extent.width;
    initial_scissor.extent.height = equip->swapchain_extent.height;
    vkCmdSetScissor(cmd, 0, 1, &initial_scissor);

    // Update uniform buffer for UI
    struct Frame_Resources *frame = &equip->frame_resources[equip->current_frame];

    // Update uniform data (using logical coordinates for uniforms)
    f32         scale = equip->dpi_scale > 0 ? equip->dpi_scale : 1.0f;
    UI_Uniforms uniforms = {0};
    uniforms.viewport_size_px.x = (f32)equip->swapchain_extent.width / scale;
    uniforms.viewport_size_px.y = (f32)equip->swapchain_extent.height / scale;
    printf("UI Uniforms: viewport_size=(%.1f, %.1f) scale=%.1f\n",
           uniforms.viewport_size_px.x, uniforms.viewport_size_px.y, scale);
    uniforms.opacity = 1.0f;
    // Identity matrix for texture sampling
    for (int i = 0; i < 4; i++)
        uniforms.texture_sample_channel_map.m[i][i] = 1.0f;

    // Copy to uniform buffer
    memcpy((u8 *)g_vulkan->uniform_buffer.mapped + frame->uniform_offset, &uniforms, sizeof(uniforms));

    // Update descriptor set
    VkDescriptorBufferInfo buffer_info = {0};
    buffer_info.buffer = g_vulkan->uniform_buffer.buffer;
    buffer_info.offset = frame->uniform_offset;
    buffer_info.range = sizeof(UI_Uniforms);

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = frame->ui_global_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(g_vulkan->device, 1, &write, 0, NULL);

    // Bind global descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vulkan->pipeline_layouts.ui,
                            0, 1, &frame->ui_global_set, 0, NULL);

    // Count total instances to allocate buffer space
    u64 total_instance_size = 0;
    for (Renderer_Batch_Group_2D_Node *group_node = params->rects.first;
         group_node;
         group_node = group_node->next)
    {
        for (Renderer_Batch_Node *batch_node = group_node->batches.first;
             batch_node;
             batch_node = batch_node->next)
        {
            total_instance_size += batch_node->v.byte_count;
        }
    }

    // Ensure buffer is large enough
    ensure_dynamic_buffer(&g_instance_buffer, total_instance_size);

    // Process each batch group
    int group_count = 0;
    for (Renderer_Batch_Group_2D_Node *group_node = params->rects.first;
         group_node;
         group_node = group_node->next)
    {
        group_count++;
        printf("Processing batch group %d\n", group_count);
        Renderer_Batch_Group_2D_Params *group_params = &group_node->params;

        // Set scissor based on clip rect
        VkRect2D scissor = {0};
        scissor.offset.x = (s32)group_params->clip.min.x;
        scissor.offset.y = (s32)group_params->clip.min.y;
        scissor.extent.width = (u32)(group_params->clip.max.x - group_params->clip.min.x);
        scissor.extent.height = (u32)(group_params->clip.max.y - group_params->clip.min.y);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Bind texture descriptor set
        VkDescriptorSet texture_set = VK_NULL_HANDLE;

        // Check if we have a texture
        Renderer_Vulkan_Texture_2D *tex = NULL;
        if (group_params->tex.u64s[0] != 0)
        {
            tex = (Renderer_Vulkan_Texture_2D *)group_params->tex.u64s[0];
        }

        // Allocate a new descriptor set for the texture
        VkDescriptorSetAllocateInfo tex_alloc_info = {0};
        tex_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        tex_alloc_info.descriptorPool = g_vulkan->descriptor_pool;
        tex_alloc_info.descriptorSetCount = 1;
        tex_alloc_info.pSetLayouts = &g_vulkan->descriptor_set_layouts.ui_texture;

        if (vkAllocateDescriptorSets(g_vulkan->device, &tex_alloc_info, &texture_set) == VK_SUCCESS)
        {
            // Update texture descriptor
            VkDescriptorImageInfo image_info = {0};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = tex ? tex->view : g_vulkan->white_texture_view;
            image_info.sampler = (group_params->tex_sample_kind == Renderer_Tex_2D_Sample_Kind_Linear)
                                     ? g_vulkan->sampler_linear
                                     : g_vulkan->sampler_nearest;

            VkWriteDescriptorSet tex_write = {0};
            tex_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            tex_write.dstSet = texture_set;
            tex_write.dstBinding = 0;
            tex_write.dstArrayElement = 0;
            tex_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            tex_write.descriptorCount = 1;
            tex_write.pImageInfo = &image_info;

            vkUpdateDescriptorSets(g_vulkan->device, 1, &tex_write, 0, NULL);

            // Bind texture descriptor set
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vulkan->pipeline_layouts.ui,
                                    1, 1, &texture_set, 0, NULL);
        }

        // Draw each batch in the group
        for (Renderer_Batch_Node *batch_node = group_node->batches.first;
             batch_node;
             batch_node = batch_node->next)
        {
            Renderer_Batch *batch = &batch_node->v;

            if (batch->byte_count == 0)
                continue;

            // Copy instance data to dynamic buffer
            memcpy((u8 *)g_instance_buffer.mapped + g_instance_buffer.offset, batch->v, batch->byte_count);

            // Bind instance buffer
            VkDeviceSize offset = g_instance_buffer.offset;
            vkCmdBindVertexBuffers(cmd, 0, 1, &g_instance_buffer.buffer, &offset);

            // Draw instanced rectangles (4 vertices per rect, using triangle strip)
            u32 instance_count = batch->byte_count / sizeof(Renderer_Rect_2D_Inst);
            vkCmdDraw(cmd, 4, instance_count, 0, 0);

            g_instance_buffer.offset += batch->byte_count;
        }
    }
}

void
renderer_vulkan_submit_blur_pass(VkCommandBuffer cmd, Renderer_Pass_Params_Blur *params,
                                 Renderer_Vulkan_Window_Equipment *equip)
{
    ZoneScopedN("VulkanSubmitBlurPass");
    // TODO: Implement blur pass
    // This typically involves:
    // 1. Rendering to an offscreen texture
    // 2. Horizontal blur pass
    // 3. Vertical blur pass
    // 4. Composite back to main framebuffer
}

void
renderer_vulkan_submit_geo_3d_pass(VkCommandBuffer cmd, Renderer_Pass_Params_Geo_3D *params,
                                   Renderer_Vulkan_Window_Equipment *equip)
{
    ZoneScopedN("VulkanSubmitGeo3DPass");
    if (!g_vulkan->pipelines.geo_3d)
    {
        log_error("geo_3d pipeline is null!");
        return;
    }

    // Bind 3D pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vulkan->pipelines.geo_3d);

    // Set viewport based on params (converting logical to physical coordinates)
    f32        scale = equip->dpi_scale > 0 ? equip->dpi_scale : 1.0f;
    VkViewport viewport = {0};
    viewport.x = params->viewport.min.x * scale;
    viewport.y = params->viewport.min.y * scale;
    viewport.width = (params->viewport.max.x - params->viewport.min.x) * scale;
    viewport.height = (params->viewport.max.y - params->viewport.min.y) * scale;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Set scissor based on clip rect (converting logical to physical coordinates)
    VkRect2D scissor = {0};
    scissor.offset.x = (s32)(params->clip.min.x * scale);
    scissor.offset.y = (s32)(params->clip.min.y * scale);
    scissor.extent.width = (u32)((params->clip.max.x - params->clip.min.x) * scale);
    scissor.extent.height = (u32)((params->clip.max.y - params->clip.min.y) * scale);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Update uniform buffer for 3D rendering
    struct Frame_Resources *frame = &equip->frame_resources[equip->current_frame];

    // Update uniform data
    Geo_3D_Uniforms uniforms = {0};
    uniforms.view = params->view;
    uniforms.projection = params->projection;

    // Allocate space in uniform buffer
    // Each frame has 256KB, we'll use a proper offset for 3D uniforms
    u64 uniform_alignment = 256; // Typical uniform buffer alignment
    u64 ui_uniforms_size = AlignPow2(sizeof(UI_Uniforms), uniform_alignment);
    u64 uniform_offset = frame->uniform_offset + ui_uniforms_size;
    memcpy((u8 *)g_vulkan->uniform_buffer.mapped + uniform_offset, &uniforms, sizeof(uniforms));

    // Update the pre-allocated descriptor set
    VkDescriptorBufferInfo buffer_info = {0};
    buffer_info.buffer = g_vulkan->uniform_buffer.buffer;
    buffer_info.offset = uniform_offset;
    buffer_info.range = sizeof(Geo_3D_Uniforms);

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = frame->geo_3d_global_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(g_vulkan->device, 1, &write, 0, NULL);

    // Bind global descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vulkan->pipeline_layouts.geo_3d,
                            0, 1, &frame->geo_3d_global_set, 0, NULL);

    // Count total instances for buffer allocation
    u64 total_instance_size = 0;
    for (u64 i = 0; i < params->mesh_batches.slots_count; i++)
    {
        Renderer_Batch_Group_3D_Map_Node *node = params->mesh_batches.slots[i];
        while (node)
        {
            for (Renderer_Batch_Node *batch_node = node->batches.first;
                 batch_node;
                 batch_node = batch_node->next)
            {
                total_instance_size += batch_node->v.byte_count;
            }
            node = node->next;
        }
    }

    if (total_instance_size > 0)
    {
        ensure_dynamic_buffer(&g_instance_buffer, total_instance_size);
    }

    // Process mesh batches
    for (u64 i = 0; i < params->mesh_batches.slots_count; i++)
    {
        Renderer_Batch_Group_3D_Map_Node *node = params->mesh_batches.slots[i];
        while (node)
        {
            Renderer_Batch_Group_3D_Params *group_params = &node->params;

            // Bind vertex and index buffers
            if (group_params->mesh_vertices.u64s[0] && group_params->mesh_indices.u64s[0])
            {
                Renderer_Vulkan_Buffer *vertex_buffer = (Renderer_Vulkan_Buffer *)group_params->mesh_vertices.u64s[0];
                Renderer_Vulkan_Buffer *index_buffer = (Renderer_Vulkan_Buffer *)group_params->mesh_indices.u64s[0];

                VkBuffer     vertex_buffers[] = {vertex_buffer->buffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
                vkCmdBindIndexBuffer(cmd, index_buffer->buffer, 0, VK_INDEX_TYPE_UINT32);

                // Bind texture descriptor set
                VkDescriptorSet texture_set = VK_NULL_HANDLE;

                // Check if we have a texture
                Renderer_Vulkan_Texture_2D *tex = NULL;
                if (group_params->albedo_tex.u64s[0] != 0)
                {
                    tex = (Renderer_Vulkan_Texture_2D *)group_params->albedo_tex.u64s[0];
                }

                // Allocate a new descriptor set for the texture
                VkDescriptorSetAllocateInfo tex_alloc_info = {0};
                tex_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                tex_alloc_info.descriptorPool = g_vulkan->descriptor_pool;
                tex_alloc_info.descriptorSetCount = 1;
                tex_alloc_info.pSetLayouts = &g_vulkan->descriptor_set_layouts.geo_3d_texture;

                if (vkAllocateDescriptorSets(g_vulkan->device, &tex_alloc_info, &texture_set) == VK_SUCCESS)
                {
                    // Update texture descriptor
                    VkDescriptorImageInfo image_info = {0};
                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    image_info.imageView = tex ? tex->view : g_vulkan->white_texture_view;
                    image_info.sampler = (group_params->albedo_tex_sample_kind == Renderer_Tex_2D_Sample_Kind_Linear)
                                             ? g_vulkan->sampler_linear
                                             : g_vulkan->sampler_nearest;

                    VkWriteDescriptorSet tex_write = {0};
                    tex_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    tex_write.dstSet = texture_set;
                    tex_write.dstBinding = 0;
                    tex_write.dstArrayElement = 0;
                    tex_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    tex_write.descriptorCount = 1;
                    tex_write.pImageInfo = &image_info;

                    vkUpdateDescriptorSets(g_vulkan->device, 1, &tex_write, 0, NULL);

                    // Bind texture descriptor set
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vulkan->pipeline_layouts.geo_3d,
                                            1, 1, &texture_set, 0, NULL);
                }

                // Draw each instance batch
                for (Renderer_Batch_Node *batch_node = node->batches.first;
                     batch_node;
                     batch_node = batch_node->next)
                {
                    Renderer_Batch *batch = &batch_node->v;

                    if (batch->byte_count == 0)
                        continue;

                    // Copy instance data to dynamic buffer
                    memcpy((u8 *)g_instance_buffer.mapped + g_instance_buffer.offset, batch->v, batch->byte_count);

                    // Bind instance buffer
                    VkBuffer     instance_buffers[] = {g_instance_buffer.buffer};
                    VkDeviceSize instance_offsets[] = {g_instance_buffer.offset};
                    vkCmdBindVertexBuffers(cmd, 1, 1, instance_buffers, instance_offsets);

                    // Draw indexed instances
                    u32 instance_count = batch->byte_count / sizeof(Renderer_Mesh_3D_Inst);
                    u32 index_count = index_buffer->size / sizeof(u32);
                    vkCmdDrawIndexed(cmd, index_count, instance_count, 0, 0, 0);

                    g_instance_buffer.offset += batch->byte_count;
                }
            }

            node = node->next;
        }
    }
}