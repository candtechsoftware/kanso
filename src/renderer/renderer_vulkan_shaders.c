#include "renderer_vulkan.h"
#include "../base/base_inc.h"
#include "../generated/vulkan_shaders.h"

void
renderer_vulkan_create_shaders()
{
    // Create shader modules from pre-compiled SPIR-V bytecode
    g_vulkan->shaders.rect_vert = renderer_vulkan_create_shader_module(
        renderer_vulkan_rect_vert_shader_src,
        renderer_vulkan_rect_vert_shader_src_size);

    g_vulkan->shaders.rect_frag = renderer_vulkan_create_shader_module(
        renderer_vulkan_rect_frag_shader_src,
        renderer_vulkan_rect_frag_shader_src_size);

    g_vulkan->shaders.blur_vert = renderer_vulkan_create_shader_module(
        renderer_vulkan_blur_vert_shader_src,
        renderer_vulkan_blur_vert_shader_src_size);

    g_vulkan->shaders.blur_frag = renderer_vulkan_create_shader_module(
        renderer_vulkan_blur_frag_shader_src,
        renderer_vulkan_blur_frag_shader_src_size);

    g_vulkan->shaders.mesh_vert = renderer_vulkan_create_shader_module(
        renderer_vulkan_mesh_vert_shader_src,
        renderer_vulkan_mesh_vert_shader_src_size);

    g_vulkan->shaders.mesh_frag = renderer_vulkan_create_shader_module(
        renderer_vulkan_mesh_frag_shader_src,
        renderer_vulkan_mesh_frag_shader_src_size);
}

void
renderer_vulkan_destroy_shaders()
{
    if (g_vulkan->shaders.rect_vert)
        vkDestroyShaderModule(g_vulkan->device, g_vulkan->shaders.rect_vert, NULL);
    if (g_vulkan->shaders.rect_frag)
        vkDestroyShaderModule(g_vulkan->device, g_vulkan->shaders.rect_frag, NULL);
    if (g_vulkan->shaders.blur_vert)
        vkDestroyShaderModule(g_vulkan->device, g_vulkan->shaders.blur_vert, NULL);
    if (g_vulkan->shaders.blur_frag)
        vkDestroyShaderModule(g_vulkan->device, g_vulkan->shaders.blur_frag, NULL);
    if (g_vulkan->shaders.mesh_vert)
        vkDestroyShaderModule(g_vulkan->device, g_vulkan->shaders.mesh_vert, NULL);
    if (g_vulkan->shaders.mesh_frag)
        vkDestroyShaderModule(g_vulkan->device, g_vulkan->shaders.mesh_frag, NULL);

    MemoryZero(&g_vulkan->shaders, sizeof(g_vulkan->shaders));
}

void
renderer_vulkan_create_descriptor_set_layouts()
{
    // UI Global descriptor set layout (set = 0)
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            // Uniform buffer for global data
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL}};

        VkDescriptorSetLayoutCreateInfo layout_info = {0};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = bindings;

        vkCreateDescriptorSetLayout(g_vulkan->device, &layout_info, NULL, &g_vulkan->descriptor_set_layouts.ui_global);
    }

    // UI Texture descriptor set layout (set = 1)
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            // Texture sampler
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL}};

        VkDescriptorSetLayoutCreateInfo layout_info = {0};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = bindings;

        vkCreateDescriptorSetLayout(g_vulkan->device, &layout_info, NULL, &g_vulkan->descriptor_set_layouts.ui_texture);
    }

    // Blur descriptor set layout (set = 0)
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            // Uniform buffer for blur params
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL},
            // Input texture
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL}};

        VkDescriptorSetLayoutCreateInfo layout_info = {0};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 2;
        layout_info.pBindings = bindings;

        vkCreateDescriptorSetLayout(g_vulkan->device, &layout_info, NULL, &g_vulkan->descriptor_set_layouts.blur_global);
    }

    // Geo 3D Global descriptor set layout (set = 0)
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            // Uniform buffer for view/projection matrices
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = NULL}};

        VkDescriptorSetLayoutCreateInfo layout_info = {0};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = bindings;

        vkCreateDescriptorSetLayout(g_vulkan->device, &layout_info, NULL, &g_vulkan->descriptor_set_layouts.geo_3d_global);
    }

    // Geo 3D Texture descriptor set layout (set = 1)
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            // Albedo texture
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL}};

        VkDescriptorSetLayoutCreateInfo layout_info = {0};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = bindings;

        vkCreateDescriptorSetLayout(g_vulkan->device, &layout_info, NULL, &g_vulkan->descriptor_set_layouts.geo_3d_texture);
    }
}

void
renderer_vulkan_create_pipeline_layouts()
{
    // UI pipeline layout
    {
        VkDescriptorSetLayout layouts[] = {
            g_vulkan->descriptor_set_layouts.ui_global,
            g_vulkan->descriptor_set_layouts.ui_texture};

        VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 2;
        pipeline_layout_info.pSetLayouts = layouts;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = NULL;

        vkCreatePipelineLayout(g_vulkan->device, &pipeline_layout_info, NULL, &g_vulkan->pipeline_layouts.ui);
    }

    // Blur pipeline layout
    {
        VkDescriptorSetLayout layouts[] = {
            g_vulkan->descriptor_set_layouts.blur_global};

        VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = layouts;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = NULL;

        vkCreatePipelineLayout(g_vulkan->device, &pipeline_layout_info, NULL, &g_vulkan->pipeline_layouts.blur);
    }

    // Geo 3D pipeline layout
    {
        VkDescriptorSetLayout layouts[] = {
            g_vulkan->descriptor_set_layouts.geo_3d_global,
            g_vulkan->descriptor_set_layouts.geo_3d_texture};

        VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 2;
        pipeline_layout_info.pSetLayouts = layouts;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = NULL;

        vkCreatePipelineLayout(g_vulkan->device, &pipeline_layout_info, NULL, &g_vulkan->pipeline_layouts.geo_3d);
    }
}

void
renderer_vulkan_create_pipelines(VkRenderPass render_pass)
{
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    // Common dynamic states
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamic_state = {0};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    // Create UI Pipeline
    {
        // Shader stages
        VkPipelineShaderStageCreateInfo shader_stages[2] = {0};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = g_vulkan->shaders.rect_vert;
        shader_stages[0].pName = "main";

        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = g_vulkan->shaders.rect_frag;
        shader_stages[1].pName = "main";

        // Vertex input - instanced rendering
        VkVertexInputBindingDescription binding_desc = {0};
        binding_desc.binding = 0;
        binding_desc.stride = sizeof(Renderer_Rect_2D_Inst);
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        VkVertexInputAttributeDescription attr_descs[8] = {0};
        // dst_rect
        attr_descs[0].binding = 0;
        attr_descs[0].location = 0;
        attr_descs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr_descs[0].offset = offsetof(Renderer_Rect_2D_Inst, dst);

        // src_rect
        attr_descs[1].binding = 0;
        attr_descs[1].location = 1;
        attr_descs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr_descs[1].offset = offsetof(Renderer_Rect_2D_Inst, src);

        // colors[0-3]
        for (int i = 0; i < 4; i++)
        {
            attr_descs[2 + i].binding = 0;
            attr_descs[2 + i].location = 2 + i;
            attr_descs[2 + i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr_descs[2 + i].offset = offsetof(Renderer_Rect_2D_Inst, colors) + i * sizeof(Vec4_f32);
        }

        // corner_radii
        attr_descs[6].binding = 0;
        attr_descs[6].location = 6;
        attr_descs[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr_descs[6].offset = offsetof(Renderer_Rect_2D_Inst, corner_radii);

        // style (border_thickness, edge_softness, white_texture_override, unused)
        attr_descs[7].binding = 0;
        attr_descs[7].location = 7;
        attr_descs[7].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr_descs[7].offset = offsetof(Renderer_Rect_2D_Inst, border_thickness);

        VkPipelineVertexInputStateCreateInfo vertex_input = {0};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.vertexBindingDescriptionCount = 1;
        vertex_input.pVertexBindingDescriptions = &binding_desc;
        vertex_input.vertexAttributeDescriptionCount = 8;
        vertex_input.pVertexAttributeDescriptions = attr_descs;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Viewport state
        VkViewport viewport = {0};
        VkRect2D   scissor = {0};

        VkPipelineViewportStateCreateInfo viewport_state = {0};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer = {0};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling = {0};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth stencil
        VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_FALSE;
        depth_stencil.depthWriteEnable = VK_FALSE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

        // Color blending - premultiplied alpha
        VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_TRUE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blending = {0};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        // Create pipeline
        VkGraphicsPipelineCreateInfo pipeline_info = {0};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = g_vulkan->pipeline_layouts.ui;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;

        if (vkCreateGraphicsPipelines(g_vulkan->device, pipeline_cache, 1, &pipeline_info,
                                      NULL, &g_vulkan->pipelines.ui) != VK_SUCCESS)
        {
            assert(0 && "Failed to create UI graphics pipeline!");
        }
    }

    // Create Blur Pipeline (simplified for now)
    {
        VkPipelineShaderStageCreateInfo shader_stages[2] = {0};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = g_vulkan->shaders.blur_vert;
        shader_stages[0].pName = "main";

        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = g_vulkan->shaders.blur_frag;
        shader_stages[1].pName = "main";

        // No vertex input for fullscreen triangle
        VkPipelineVertexInputStateCreateInfo vertex_input = {0};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Reuse other states from UI pipeline
        VkViewport viewport = {0};
        VkRect2D   scissor = {0};

        VkPipelineViewportStateCreateInfo viewport_state = {0};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {0};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling = {0};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_FALSE;
        depth_stencil.depthWriteEnable = VK_FALSE;

        // No blending for blur (renders to offscreen target)
        VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending = {0};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        VkGraphicsPipelineCreateInfo pipeline_info = {0};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = g_vulkan->pipeline_layouts.blur;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;

        // Create both horizontal and vertical blur pipelines
        if (vkCreateGraphicsPipelines(g_vulkan->device, pipeline_cache, 1, &pipeline_info,
                                      NULL, &g_vulkan->pipelines.blur_horizontal) != VK_SUCCESS)
        {
            assert(0 && "Failed to create horizontal blur graphics pipeline!");
        }

        // For now, use same pipeline for vertical blur
        g_vulkan->pipelines.blur_vertical = g_vulkan->pipelines.blur_horizontal;
    }

    // Create Geo 3D Pipeline
    {
        VkPipelineShaderStageCreateInfo shader_stages[2] = {0};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = g_vulkan->shaders.mesh_vert;
        shader_stages[0].pName = "main";

        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = g_vulkan->shaders.mesh_frag;
        shader_stages[1].pName = "main";

        // Vertex input for 3D meshes
        VkVertexInputBindingDescription binding_descs[2] = {0};
        // Vertex data
        binding_descs[0].binding = 0;
        binding_descs[0].stride = sizeof(f32) * 12; // pos(3) + tex(2) + norm(3) + color(4)
        binding_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Instance data
        binding_descs[1].binding = 1;
        binding_descs[1].stride = sizeof(Mat4x4_f32);
        binding_descs[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        VkVertexInputAttributeDescription attr_descs[8] = {0};
        // Position
        attr_descs[0].binding = 0;
        attr_descs[0].location = 0;
        attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attr_descs[0].offset = 0;

        // Texcoord
        attr_descs[1].binding = 0;
        attr_descs[1].location = 1;
        attr_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attr_descs[1].offset = sizeof(f32) * 3;

        // Normal
        attr_descs[2].binding = 0;
        attr_descs[2].location = 2;
        attr_descs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attr_descs[2].offset = sizeof(f32) * 5;

        // Color
        attr_descs[3].binding = 0;
        attr_descs[3].location = 3;
        attr_descs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr_descs[3].offset = sizeof(f32) * 8;

        // Instance transform (mat4 = 4 vec4s)
        for (int i = 0; i < 4; i++)
        {
            attr_descs[4 + i].binding = 1;
            attr_descs[4 + i].location = 4 + i;
            attr_descs[4 + i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr_descs[4 + i].offset = sizeof(Vec4_f32) * i;
        }

        VkPipelineVertexInputStateCreateInfo vertex_input = {0};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.vertexBindingDescriptionCount = 2;
        vertex_input.pVertexBindingDescriptions = binding_descs;
        vertex_input.vertexAttributeDescriptionCount = 8;
        vertex_input.pVertexAttributeDescriptions = attr_descs;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Viewport state
        VkViewport viewport = {0};
        VkRect2D   scissor = {0};

        VkPipelineViewportStateCreateInfo viewport_state = {0};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        // Rasterizer with back-face culling
        VkPipelineRasterizationStateCreateInfo rasterizer = {0};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling = {0};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth testing enabled
        VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

        // Alpha blending - standard alpha blending (not premultiplied)
        VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_TRUE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blending = {0};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        // Create pipeline
        VkGraphicsPipelineCreateInfo pipeline_info = {0};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = g_vulkan->pipeline_layouts.geo_3d;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;

        if (vkCreateGraphicsPipelines(g_vulkan->device, pipeline_cache, 1, &pipeline_info,
                                      NULL, &g_vulkan->pipelines.geo_3d) != VK_SUCCESS)
        {
            assert(0 && "Failed to create 3D geometry graphics pipeline!");
        }
    }
}